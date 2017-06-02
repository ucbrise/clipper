#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

#define PROVIDES_EXECUTORS
#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
#include <clipper/future.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/timers.hpp>

#define UNREACHABLE() assert(false)

using std::vector;
using std::tuple;

namespace clipper {

QueryProcessor::QueryProcessor() : state_db_(std::make_shared<StateDB>()) {
  // Create selection policy instances
  selection_policies_.emplace(DefaultOutputSelectionPolicy::get_name(),
                              std::make_shared<DefaultOutputSelectionPolicy>());
  log_info(LOGGING_TAG_QUERY_PROCESSOR, "Query Processor started");
}

std::shared_ptr<StateDB> QueryProcessor::get_state_table() const {
  return state_db_;
}

boost::future<Response> QueryProcessor::predict(Query query) {
  long query_id = query_counter_.fetch_add(1);
  auto current_policy_iter = selection_policies_.find(query.selection_policy_);
  if (current_policy_iter == selection_policies_.end()) {
    std::stringstream err_msg_builder;
    err_msg_builder << query.selection_policy_ << " "
                    << "is an invalid selection_policy.";
    const std::string err_msg = err_msg_builder.str();
    log_error(LOGGING_TAG_QUERY_PROCESSOR, err_msg);
    throw PredictError(err_msg);
  }
  std::shared_ptr<SelectionPolicy> current_policy = current_policy_iter->second;

  auto state_opt = state_db_->get(StateKey{query.label_, query.user_id_, 0});
  if (!state_opt) {
    std::stringstream err_msg_builder;
    err_msg_builder << "No selection state found for query with user_id: "
                    << query.user_id_ << " and label: " << query.label_;
    const std::string err_msg = err_msg_builder.str();
    log_error(LOGGING_TAG_QUERY_PROCESSOR, err_msg);
    throw PredictError(err_msg);
  }
  std::shared_ptr<SelectionState> selection_state =
      current_policy->deserialize(*state_opt);

  boost::optional<std::string> default_explanation;
  std::vector<PredictTask> tasks =
      current_policy->select_predict_tasks(selection_state, query, query_id);

  log_info_formatted(LOGGING_TAG_QUERY_PROCESSOR, "Found {} tasks",
                     tasks.size());

  vector<boost::future<Output>> task_futures =
      task_executor_.schedule_predictions(tasks);
  if (task_futures.empty()) {
    default_explanation = "No connected models found for query";
    log_error_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                        "No connected models found for query with id: {}",
                        query_id);
  }
  boost::future<void> timer_future =
      timer_system_.set_timer(query.latency_budget_micros_);

  boost::future<void> all_tasks_completed;
  auto num_completed = std::make_shared<std::atomic<int>>(0);
  std::tie(all_tasks_completed, task_futures) =
      future::when_all(std::move(task_futures), num_completed);

  auto completed_flag = std::make_shared<std::atomic_flag>();
  // Due to some complexities of initializing the atomic_flag in a shared_ptr,
  // we explicitly set the value of the flag to false after initialization.
  completed_flag->clear();
  boost::future<void> response_ready_future;

  std::tie(response_ready_future, all_tasks_completed, timer_future) =
      future::when_either(std::move(all_tasks_completed),
                          std::move(timer_future), completed_flag);

  boost::promise<Response> response_promise;
  auto response_future = response_promise.get_future();

  // NOTE: We capture the num_completed, completed_flag, and default_explanation
  // variables
  // so that they outlive the composed_futures.
  response_ready_future.then([
    this, query, query_id, response_promise = std::move(response_promise),
    selection_state, current_policy, task_futures = std::move(task_futures),
    num_completed, completed_flag, default_explanation
  ](auto) mutable {

    vector<Output> outputs;
    vector<VersionedModelId> used_models;
    bool all_tasks_timed_out = true;
    for (auto r = task_futures.begin(); r != task_futures.end(); ++r) {
      if ((*r).is_ready()) {
        outputs.push_back((*r).get());
        all_tasks_timed_out = false;
      }
    }
    if (all_tasks_timed_out && !task_futures.empty() && !default_explanation) {
      default_explanation =
          "Failed to retrieve a prediction response within the specified "
          "latency SLO";
    }

    std::pair<Output, bool> final_output =
        current_policy->combine_predictions(selection_state, query, outputs);

    std::chrono::time_point<std::chrono::high_resolution_clock> end =
        std::chrono::high_resolution_clock::now();
    long duration_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - query.create_time_)
            .count();

    Response response{query,
                      query_id,
                      duration_micros,
                      final_output.first,
                      final_output.second,
                      default_explanation};
    response_promise.set_value(response);
  });
  return response_future;
}

boost::future<FeedbackAck> QueryProcessor::update(FeedbackQuery feedback) {
  log_info(LOGGING_TAG_QUERY_PROCESSOR, "Received feedback for user {}",
           feedback.user_id_);

  long query_id = query_counter_.fetch_add(1);
  boost::future<FeedbackAck> error_response = boost::make_ready_future(false);

  auto current_policy_iter =
      selection_policies_.find(feedback.selection_policy_);
  if (current_policy_iter == selection_policies_.end()) {
    log_error_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                        "{} is an invalid selection policy",
                        feedback.selection_policy_);
    // TODO better error handling
    return error_response;
  }
  std::shared_ptr<SelectionPolicy> current_policy = current_policy_iter->second;

  StateKey state_key{feedback.label_, feedback.user_id_, 0};
  auto state_opt = state_db_->get(state_key);
  if (!state_opt) {
    log_error_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                        "No selection state found for query with label: {}",
                        feedback.label_);
    // TODO better error handling
    return error_response;
  }
  std::shared_ptr<SelectionState> selection_state =
      current_policy->deserialize(*state_opt);

  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  std::tie(predict_tasks, feedback_tasks) =
      current_policy->select_feedback_tasks(selection_state, feedback,
                                            query_id);

  log_info_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                     "Scheduling {} prediction tasks and {} feedback tasks",
                     predict_tasks.size(), feedback_tasks.size());

  // 1) Wait for all prediction_tasks to complete
  // 2) Update selection policy
  // 3) Complete select_policy_update_promise
  // 4) Wait for all feedback_tasks to complete (feedback_processed future)

  vector<boost::future<Output>> predict_task_futures =
      task_executor_.schedule_predictions({predict_tasks});

  vector<boost::future<FeedbackAck>> feedback_task_futures =
      task_executor_.schedule_feedback(std::move(feedback_tasks));

  boost::future<void> all_preds_completed;
  auto num_preds_completed = std::make_shared<std::atomic<int>>(0);
  std::tie(all_preds_completed, predict_task_futures) =
      future::when_all(std::move(predict_task_futures), num_preds_completed);

  boost::future<void> all_feedback_completed;
  auto num_feedback_completed = std::make_shared<std::atomic<int>>(0);
  std::tie(all_feedback_completed, feedback_task_futures) = future::when_all(
      std::move(feedback_task_futures), num_feedback_completed);

  // This promise gets completed after selection policy state update has
  // finished.
  boost::promise<FeedbackAck> select_policy_update_promise;
  auto state_table = get_state_table();
  auto select_policy_updated = select_policy_update_promise.get_future();
  all_preds_completed.then([
    moved_promise = std::move(select_policy_update_promise), selection_state,
    current_policy, state_table, feedback, query_id, state_key,
    predict_task_futures = std::move(predict_task_futures)
  ](auto) mutable {

    std::vector<Output> preds;
    // collect actual predictions from their futures
    for (auto& r : predict_task_futures) {
      preds.push_back(r.get());
    }
    auto new_selection_state = current_policy->process_feedback(
        selection_state, feedback.feedback_, preds);
    state_table->put(state_key, current_policy->serialize(new_selection_state));
    moved_promise.set_value(true);
  });

  boost::future<void> feedback_ack_ready_future;
  auto num_futures_completed = std::make_shared<std::atomic<int>>(0);
  std::tie(feedback_ack_ready_future, all_feedback_completed,
           select_policy_updated) =
      future::when_both(std::move(all_feedback_completed),
                        std::move(select_policy_updated),
                        num_futures_completed);

  boost::future<FeedbackAck> final_feedback_future =
      feedback_ack_ready_future.then([
        select_policy_updated = std::move(select_policy_updated),
        feedback_task_futures = std::move(feedback_task_futures)
      ](auto) mutable {
        auto select_policy_update_result = select_policy_updated.get();
        if (!select_policy_update_result) {
          return false;
        }
        for (auto& r : feedback_task_futures) {
          if (!r.get()) {
            return false;
          }
        }
        return true;
      });

  return final_feedback_future;
}

}  // namespace clipper
