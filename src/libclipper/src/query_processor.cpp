
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <unordered_map>

#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#define PROVIDES_EXECUTORS
#include <boost/thread.hpp>
#include <boost/thread/executors/basic_thread_pool.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>

#define UNREACHABLE() assert(false)

using boost::future;
using boost::shared_future;
using std::vector;
using std::tuple;

namespace clipper {

QueryProcessor::QueryProcessor(): state_db_(std::make_shared<StateDB>()) {

  std::cout << "Query processor constructed" << std::endl;
}

std::shared_ptr<StateDB> QueryProcessor::get_state_table() const {
  return state_db_;
}

future<Response> QueryProcessor::predict(Query query) {
  long query_id = query_counter_.fetch_add(1);
  std::vector<PredictTask> tasks;
  ByteBuffer serialized_state;
  
  // select tasks
  if (query.selection_policy_ == "newest_model") {
    auto tasks_and_state = select_predict_tasks<NewestModelSelectionPolicy>(
        query, query_id, get_state_table());
    tasks = tasks_and_state.first;
    serialized_state = tasks_and_state.second;

    std::cout << "Used NewestModelSelectionPolicy to select tasks" << std::endl;
  } else {
    std::cout << query.selection_policy_ << " is invalid selection policy"
              << std::endl;
    // TODO better error handling
    return boost::make_ready_future(
        Response{query, query_id, 20000, Output{1.0, std::make_pair("m1", 1)},
                 std::vector<VersionedModelId>()});
  }
  std::cout << "Found " << tasks.size() << " tasks" << std::endl;

  vector<shared_future<Output>> task_completion_futures =
      task_executor_.schedule_predictions(tasks);
    auto task_completion_copies = task_completion_futures;
  std::cout << "Found " << task_completion_futures.size()
            << " task completion futures" << std::endl;
  future<void> timer_future = timer_system_.set_timer(query.latency_micros_);

  auto all_tasks_completed = boost::when_all(task_completion_copies.begin(),
                                             task_completion_copies.end());
    auto make_response_future =
        boost::when_any(std::move(all_tasks_completed),
        std::move(timer_future));

  boost::promise<Response> promise;
  auto f = promise.get_future();

    make_response_future.then([
    query, query_id, p = std::move(promise), s = std::move(serialized_state),
    task_futures = std::move(task_completion_futures)
  ](auto result_future) mutable {
    std::cout << "ENTERED CONTINUATION LAMBDA" << std::endl;

    auto result = result_future.get();
    std::cout << std::boolalpha;
    std::cout << "All tasks finished: " << std::get<0>(result).is_ready()
              << ", Timer fired: " << std::get<1>(result).is_ready()
              << std::endl;
    vector<Output> outputs;
    vector<VersionedModelId> used_models;
    //    vector<shared_future<Output>> completed_tasks =
    //    std::get<0>(result).get();

//      vector<boost::shared_future<Output>> completed_tasks = task_futures.get();
      for (auto r = task_futures.begin(); r != task_futures.end(); ++r) {
        if ((*r).is_ready()) {
          outputs.push_back((*r).get());
        }
      }
    std::cout << "Found " << outputs.size() << " completed tasks" << std::endl;

    Output final_output;
    if (query.selection_policy_ == "newest_model") {
      final_output =
          combine_predictions<NewestModelSelectionPolicy>(query, outputs, s);
    } else {
      UNREACHABLE();
    }
    std::cout << "RESPONSE FUTURE THREAD: " << std::this_thread::get_id()
              << std::endl;
    Response response{query, query_id, 20000, final_output,
                      query.candidate_models_};
    p.set_value(response);

  });
  return f;
}


boost::future<FeedbackAck> QueryProcessor::update(FeedbackQuery feedback) {
  std::cout << "received feedback for user " << feedback.user_id_ << std::endl;

  long query_id = query_counter_.fetch_add(1);
  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  ByteBuffer serialized_state;

  // select tasks
  if (feedback.selection_policy_ == "newest_model") {
    auto tasks_and_state = select_feedback_tasks<NewestModelSelectionPolicy>(
        feedback, query_id, get_state_table());
    // TODO: clean this up
    predict_tasks = tasks_and_state.first.first;
    feedback_tasks = tasks_and_state.first.second;
    serialized_state = tasks_and_state.second;
    std::cout << "Used NewestModelSelectionPolicy to select tasks" << std::endl;
  } else {
    std::cout << feedback.selection_policy_ << " is invalid selection policy"
              << std::endl;
    // TODO better error handling
    return boost::make_ready_future(false);
  }
  
  std::cout << "Scheduling " << predict_tasks.size() << " predict tasks and "
  << feedback_tasks.size() << " feedback tasks " << std::endl;
  
  // 1) Wait for all prediction_tasks to complete
  // 2) Update selection policy
  // 3) Complete select_policy_update_promise
  // 4) Wait for all feedback_tasks to complete (feedback_processed future)

// copy the vector
  vector<shared_future<Output>> predict_task_completion_futures =
      task_executor_.schedule_predictions({predict_tasks});

  vector<boost::future<FeedbackAck>> feedback_task_completion_futures =
      task_executor_.schedule_feedback(std::move(feedback_tasks));

  // when this future completes, we are ready to update the selection state
  auto predictions_completed = boost::when_all(predict_task_completion_futures.begin(),
                                             predict_task_completion_futures.end());
  
  
  auto feedback_processed = boost::when_all(feedback_task_completion_futures.begin(),
                                             feedback_task_completion_futures.end());

  // This promise gets completed after selection policy state update has finished.
  boost::promise<FeedbackAck> select_policy_update_promise;
  auto state_db_ptr = get_state_table();
  auto select_policy_updated = select_policy_update_promise.get_future();
  predictions_completed.then([
  p = std::move(select_policy_update_promise),
  s = std::move(serialized_state),
  state_table = std::move(state_db_ptr),
  feedback, query_id] (auto pred_tasks_future) mutable {
  auto pred_futures = pred_tasks_future.get();
  std::vector<Output> preds;
  // collect actual predictions from their futures
  for (auto r = pred_futures.begin(); r != pred_futures.end(); ++r) {
    preds.push_back((*r).get());
  }
    if (feedback.selection_policy_ == "newest_model") {
        // update the selection policy state using the
        // appropriate selection policu
        process_feedback<NewestModelSelectionPolicy>(feedback, preds, s, state_table);
    } else {
      UNREACHABLE();
    }
    p.set_value(true);
  });
  

  auto feedback_ack_ready_future =
      boost::when_all(std::move(feedback_processed),
                      std::move(select_policy_updated))
          .then([](auto f) {
            // check that all feedback was successful
            auto result = f.get();
            auto feedback_results = std::get<0>(result).get();
            auto select_policy_update_result = std::get<1>(result).get();
            if (!select_policy_update_result) {
              return false;
            }
            for (auto r = feedback_results.begin(); r != feedback_results.end();
                 ++r) {
              if (!(*r).get()) {
                return false;
              }
            }
            return true;
          });

  return feedback_ack_ready_future;
}

}  // namespace clipper
