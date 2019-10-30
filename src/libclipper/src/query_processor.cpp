#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

#define PROVIDES_EXECUTORS
#include <boost/exception_ptr.hpp>
#include <boost/optional.hpp>

#include <boost/thread/executors/basic_thread_pool.hpp>

#include <folly/Unit.h>
#include <folly/futures/Future.h>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/exceptions.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/timers.hpp>

#define UNREACHABLE() assert(false)

using std::tuple;
using std::vector;

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

folly::Future<std::vector<Response>> QueryProcessor::predict(
    const Query &query) {
  long first_subquery_id = query_counter_.fetch_add(query.input_batch_.size());
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
  std::vector<PredictTask> tasks;
  std::vector<VersionedModelId> models;
  std::tie(tasks, models) = current_policy->select_predict_tasks(
      selection_state, query, first_subquery_id);

  log_info_formatted(LOGGING_TAG_QUERY_PROCESSOR, "Found {} tasks",
                     models.size() * tasks.size());

  std::vector<folly::Future<Output>> task_futures =
      task_executor_.schedule_predictions(tasks, models);
  if (task_futures.empty()) {
    default_explanation = "No connected models found for query";
    log_error_formatted(
        LOGGING_TAG_QUERY_PROCESSOR,
        "No connected models found for query with ids: {} to {}",
        first_subquery_id, first_subquery_id + query.input_batch_.size() - 1);
  }

  folly::Future<folly::Unit> timer_future =
      timer_system_.set_timer(query.latency_budget_micros_);

  std::shared_ptr<std::mutex> outputs_mutex = std::make_shared<std::mutex>();
  std::shared_ptr<std::vector<Output>> outputs_ptr =
      std::make_shared<std::vector<Output>>(task_futures.size());

  std::vector<folly::Future<folly::Unit>> wrapped_task_futures;
  for (size_t i = 0; i < task_futures.size(); ++i) {
    auto &t = task_futures[i];
    wrapped_task_futures.push_back(
        std::move(t)
            .thenValue([outputs_mutex, outputs_ptr, i](Output output) {
            std::lock_guard<std::mutex> lock(*outputs_mutex);
            (*outputs_ptr)[i] = output;
          }).thenError(folly::tag_t<std::exception>{}, [](const std::exception& e) {
          log_error_formatted(
              LOGGING_TAG_QUERY_PROCESSOR,
              "Unexpected error while executing prediction tasks: {}",
              e.what());
        }));
  }

  folly::Future<folly::Unit> all_tasks_completed_future =
      folly::collect(wrapped_task_futures)
          .thenValue([](std::vector<folly::Unit> /* outputs */) {});

  std::vector<folly::Future<folly::Unit>> when_either_futures;
  when_either_futures.push_back(std::move(all_tasks_completed_future));
  when_either_futures.push_back(std::move(timer_future));

  folly::Future<std::pair<size_t, folly::Try<folly::Unit>>>
      response_ready_future = folly::collectAny(when_either_futures);

  folly::Promise<std::vector<Response>> response_promise;
  folly::Future<std::vector<Response>> response_future = response_promise.getFuture();

  std::move(response_ready_future)
      .thenValue(
          [outputs_ptr, outputs_mutex, query, first_subquery_id,
                  selection_state, current_policy,
                  response_promise = std::move(response_promise),
                  default_explanation
  ](const std::pair<size_t,
                    folly::Try<folly::Unit>>& /* completed_future */) mutable {
    std::lock_guard<std::mutex> outputs_lock(*outputs_mutex);

    std::vector<std::pair<Output, bool>> final_output = current_policy->combine_predictions(
        selection_state, query, *outputs_ptr);

    std::chrono::time_point<std::chrono::high_resolution_clock> end =
        std::chrono::high_resolution_clock::now();
    long duration_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - query.create_time_)
            .count();

    std::vector<Response> responses;
    responses.reserve(final_output.size());
    long subquery_id = first_subquery_id;
    for (const auto& f : final_output) {
      responses.emplace_back(
          subquery_id, duration_micros, f.first, f.second,
          ((default_explanation || !f.second)
               ? default_explanation
               : boost::optional<std::string>("Failed to retrieve a prediction "
                                              "response within the specified "
                                              "latency SLO")));
      ++subquery_id;
    }

    response_promise.setValue(responses);
  });
  return response_future;
}

folly::Future<FeedbackAck> QueryProcessor::update(const FeedbackQuery& feedback) {
  log_info(LOGGING_TAG_QUERY_PROCESSOR, "Received feedback for user {}",
           feedback.user_id_);

  long query_id = query_counter_.fetch_add(1);
  folly::Future<FeedbackAck> error_response = folly::makeFuture(false);

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
  std::vector<VersionedModelId> models;
  std::tie(predict_tasks, feedback_tasks, models) =
      current_policy->select_feedback_tasks(selection_state, feedback,
                                            query_id);

  log_info_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                     "Scheduling {} prediction tasks and {} feedback tasks",
                     predict_tasks.size() * models.size(),
                     feedback_tasks.size() * models.size());

  // 1) Wait for all prediction_tasks to complete
  // 2) Update selection policy
  // 3) Complete select_policy_update_promise
  // 4) Wait for all feedback_tasks to complete (feedback_processed future)

  std::vector<folly::Future<Output>> predict_task_futures =
      task_executor_.schedule_predictions(predict_tasks, models);

  std::vector<folly::Future<FeedbackAck>> feedback_task_futures =
      task_executor_.schedule_feedback(feedback_tasks, models);

  folly::Future<std::vector<Output>> all_preds_completed =
      folly::collect(predict_task_futures);

  folly::Future<std::vector<FeedbackAck>> all_feedback_completed =
      folly::collect(feedback_task_futures);

  // This promise gets completed after selection policy state update has
  // finished.
  folly::Promise<FeedbackAck> select_policy_update_promise;
  folly::Future<FeedbackAck> select_policy_updated =
      select_policy_update_promise.getFuture();
  auto state_table = get_state_table();

  std::move(all_preds_completed).thenValue([
    moved_promise = std::move(select_policy_update_promise), selection_state,
    current_policy, state_table, feedback, query_id, state_key
  ](std::vector<Output> preds) mutable {
    auto new_selection_state = current_policy->process_feedback(
        selection_state, feedback.feedback_, preds);
    state_table->put(state_key, current_policy->serialize(new_selection_state));
    moved_promise.setValue(true);
  });

  auto feedback_ack_ready_future =
      folly::collect(all_feedback_completed, select_policy_updated);

  folly::Future<FeedbackAck> final_feedback_future =
      std::move(feedback_ack_ready_future).thenValue(
          [](std::tuple<std::vector<FeedbackAck>, FeedbackAck> results) {
            bool select_policy_update_result = std::get<1>(results);
            if (!select_policy_update_result) {
              return false;
            }
            for (FeedbackAck task_feedback : std::get<0>(results)) {
              if (!task_feedback) {
                return false;
              }
            }
            return true;
          });

  return final_feedback_future;
}

}  // namespace clipper
