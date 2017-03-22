
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define PROVIDES_EXECUTORS
#include <boost/thread.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/future.hpp>
#include <clipper/logging.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>
#include <clipper/timers.hpp>

#define UNREACHABLE() assert(false)

using std::vector;
using std::tuple;

namespace clipper {

// TODO: replace these template methods with a better way to do
// polymorphic dispatch
template <typename Policy>
std::pair<std::vector<PredictTask>, std::string> select_predict_tasks(
    Query query, long query_id, std::shared_ptr<StateDB> state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  std::string serialized_state;
  if (auto state_opt =
          state_db->get(StateKey{query.label_, query.user_id_, hashkey})) {
    serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
    serialized_state = Policy::serialize_state(state);
  }
  return std::make_pair(Policy::select_predict_tasks(state, query, query_id),
                        serialized_state);
}

template <typename Policy>
Output combine_predictions(Query query, std::vector<Output> predictions,
                           const std::string& serialized_state) {
  // typename Policy::state_type state;
  const auto state = Policy::deserialize_state(serialized_state);
  return Policy::combine_predictions(state, query, predictions);
}

template <typename Policy>
std::pair<std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>,
          std::string>
select_feedback_tasks(FeedbackQuery query, long query_id,
                      std::shared_ptr<StateDB> state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  std::string serialized_state;
  if (auto state_opt =
          state_db->get(StateKey{query.label_, query.user_id_, hashkey})) {
    serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
    serialized_state = Policy::serialize_state(state);
  }
  return std::make_pair(Policy::select_feedback_tasks(state, query, query_id),
                        serialized_state);
}

template <typename Policy>
void process_feedback(FeedbackQuery feedback, std::vector<Output> predictions,
                      const std::string& serialized_state,
                      std::shared_ptr<StateDB> state_db) {
  // typename Policy::state_type state;
  const auto state = Policy::deserialize_state(serialized_state);
  auto new_state =
      Policy::process_feedback(state, feedback.feedback_, predictions);
  auto serialized_new_state = Policy::serialize_state(new_state);
  auto hashkey = Policy::hash_models(feedback.candidate_models_);
  state_db->put(StateKey{feedback.label_, feedback.user_id_, hashkey},
                serialized_new_state);
}

QueryProcessor::QueryProcessor() : state_db_(std::make_shared<StateDB>()) {
  log_info(LOGGING_TAG_QUERY_PROCESSOR, "Query Processor started");
}

std::shared_ptr<StateDB> QueryProcessor::get_state_table() const {
  return state_db_;
}

boost::future<Response> QueryProcessor::predict(Query query) {
  long query_id = query_counter_.fetch_add(1);
  std::vector<PredictTask> tasks;
  std::string serialized_state;

  if (query.selection_policy_ == "EXP3") {
    auto tasks_and_state =
        select_predict_tasks<Exp3Policy>(query, query_id, get_state_table());
    tasks = tasks_and_state.first;
    serialized_state = tasks_and_state.second;

  } else if (query.selection_policy_ == "EXP4") {
    auto tasks_and_state =
        select_predict_tasks<Exp4Policy>(query, query_id, get_state_table());
    tasks = tasks_and_state.first;
    serialized_state = tasks_and_state.second;

  } else if (query.selection_policy_ == "EpsilonGreedy") {
    auto tasks_and_state = select_predict_tasks<EpsilonGreedyPolicy>(
        query, query_id, get_state_table());
    tasks = tasks_and_state.first;
    serialized_state = tasks_and_state.second;

  } else if (query.selection_policy_ == "UCB") {
    auto tasks_and_state =
        select_predict_tasks<UCBPolicy>(query, query_id, get_state_table());
    tasks = tasks_and_state.first;
    serialized_state = tasks_and_state.second;
  } else {
    log_error_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                        "{} is an invalid selection policy",
                        query.selection_policy_);
    // TODO better error handling
    return boost::make_ready_future(
        Response{query, query_id, 20000, Output{1.0, {std::make_pair("m1", 1)}},
                 std::vector<VersionedModelId>()});
  }
  log_info_formatted(LOGGING_TAG_QUERY_PROCESSOR, "Found {} tasks",
                     tasks.size());

  vector<boost::future<Output>> task_futures =
      task_executor_.schedule_predictions(tasks);
  boost::future<void> timer_future =
      timer_system_.set_timer(query.latency_micros_);

  // vector<boost::future<Output>> task_completion_futures;

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

  // NOTE: We capture the num_completed and completed_flag variables
  // so that they outlive the composed_futures.
  response_ready_future.then([
    query, query_id, response_promise = std::move(response_promise),
    serialized_state = std::move(serialized_state),
    task_futures = std::move(task_futures), num_completed, completed_flag
  ](auto) mutable {

    vector<Output> outputs;
    vector<VersionedModelId> used_models;
    for (auto r = task_futures.begin(); r != task_futures.end(); ++r) {
      if ((*r).is_ready()) {
        outputs.push_back((*r).get());
      }
    }

    Output final_output;

    if (query.selection_policy_ == "EXP3") {
      final_output =
          combine_predictions<Exp3Policy>(query, outputs, serialized_state);

    } else if (query.selection_policy_ == "EXP4") {
      final_output =
          combine_predictions<Exp4Policy>(query, outputs, serialized_state);
    } else if (query.selection_policy_ == "EpsilonGreedy") {
      final_output = combine_predictions<EpsilonGreedyPolicy>(query, outputs,
                                                              serialized_state);

    } else if (query.selection_policy_ == "UCB") {
      final_output =
          combine_predictions<UCBPolicy>(query, outputs, serialized_state);
    } else {
      UNREACHABLE();
    }
    std::chrono::time_point<std::chrono::high_resolution_clock> end =
        std::chrono::high_resolution_clock::now();
    long duration_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - query.create_time_)
            .count();

    Response response{query, query_id, duration_micros, final_output,
                      query.candidate_models_};
    response_promise.set_value(response);

  });
  return response_future;
}

boost::future<FeedbackAck> QueryProcessor::update(FeedbackQuery feedback) {
  log_info(LOGGING_TAG_QUERY_PROCESSOR, "Received feedback for user {}",
           feedback.user_id_);

  long query_id = query_counter_.fetch_add(1);
  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  std::string serialized_state;

  if (feedback.selection_policy_ == "EXP3") {
    auto tasks_and_state = select_feedback_tasks<Exp3Policy>(feedback, query_id,
                                                             get_state_table());
    // TODO: clean this up
    predict_tasks = tasks_and_state.first.first;
    feedback_tasks = tasks_and_state.first.second;
    serialized_state = tasks_and_state.second;

  } else if (feedback.selection_policy_ == "EXP4") {
    auto tasks_and_state = select_feedback_tasks<Exp4Policy>(feedback, query_id,
                                                             get_state_table());
    // TODO: clean this up
    predict_tasks = tasks_and_state.first.first;
    feedback_tasks = tasks_and_state.first.second;
    serialized_state = tasks_and_state.second;
  } else if (feedback.selection_policy_ == "EpsilonGreedy") {
    auto tasks_and_state = select_feedback_tasks<EpsilonGreedyPolicy>(
        feedback, query_id, get_state_table());
    // TODO: clean this up
    predict_tasks = tasks_and_state.first.first;
    feedback_tasks = tasks_and_state.first.second;
    serialized_state = tasks_and_state.second;

  } else if (feedback.selection_policy_ == "UCB") {
    auto tasks_and_state =
        select_feedback_tasks<UCBPolicy>(feedback, query_id, get_state_table());
    // TODO: clean this up
    predict_tasks = tasks_and_state.first.first;
    feedback_tasks = tasks_and_state.first.second;
    serialized_state = tasks_and_state.second;
  } else {
    log_error_formatted(LOGGING_TAG_QUERY_PROCESSOR,
                        "{} is an invalid feedback selection policy",
                        feedback.selection_policy_);
    // TODO better error handling
    return boost::make_ready_future(false);
  }
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

  // TODO: replace with clipper::future implementation of when_all
  // when this future completes, we are ready to update the selection state
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
  auto state_db_ptr = get_state_table();
  auto select_policy_updated = select_policy_update_promise.get_future();
  all_preds_completed.then([
    moved_promise = std::move(select_policy_update_promise),
    moved_serialized_state = std::move(serialized_state),
    state_table = std::move(state_db_ptr), feedback, query_id,
    predict_task_futures = std::move(predict_task_futures)
  ](auto) mutable {
    // auto pred_futures = pred_tasks_future.get();

    std::vector<Output> preds;
    // collect actual predictions from their futures
    for (auto& r : predict_task_futures) {
      preds.push_back(r.get());
    }

    if (feedback.selection_policy_ == "EXP3") {
      process_feedback<Exp3Policy>(feedback, preds, moved_serialized_state,
                                   state_table);

    } else if (feedback.selection_policy_ == "EXP4") {
      process_feedback<Exp4Policy>(feedback, preds, moved_serialized_state,
                                   state_table);
    } else if (feedback.selection_policy_ == "EpsilonGreedy") {
      process_feedback<EpsilonGreedyPolicy>(
          feedback, preds, moved_serialized_state, state_table);

    } else if (feedback.selection_policy_ == "UCB") {
      process_feedback<UCBPolicy>(feedback, preds, moved_serialized_state,
                                  state_table);
    } else {
      UNREACHABLE();
    }
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
        // check that all feedback was successful
        // auto result = f.get();
        // auto feedback_results = std::get<0>(result).get();
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
