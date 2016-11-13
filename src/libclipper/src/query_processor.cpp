
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>

#define BOOST_THREAD_VERSION 3
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>

#define UNREACHABLE() assert(false)

using boost::future;
using boost::shared_future;
using std::vector;
using std::tuple;

namespace clipper {

template <typename Policy>
std::pair<vector<PredictTask>, ByteBuffer> select_predict_tasks(
    Query query, long query_id, const StateDB& state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  ByteBuffer serialized_state;
  if (auto state_opt =
          state_db.get(StateKey{query.label_, query.user_id_, hashkey})) {
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

// static std::shared_ptr<Output> combine_predictions
// State state, Query query,
// std::vector<Output> predictions);

template <typename Policy>
Output combine_predictions(Query query, std::vector<Output> predictions,
                           const ByteBuffer& serialized_state) {
  // typename Policy::state_type state;
  const auto state = Policy::deserialize_state(serialized_state);
  return Policy::combine_predictions(state, query, predictions);
}

QueryProcessor::QueryProcessor() {
  std::cout << "Query processor constructed" << std::endl;
}

future<Response> QueryProcessor::predict(Query query) {
  // get instance of selection policy
  // fetch policy state from stateDB
  // generate tasks using selection policy
  // dispatch tasks to task_executor
  // set timer
  // compose futures
  // register post-processing callback
  // return future

  long query_id = query_counter_.fetch_add(1);

  std::vector<PredictTask> tasks;
  ByteBuffer serialized_state;
  if (query.selection_policy_ == "newest_model") {
    auto tasks_and_state = select_predict_tasks<NewestModelSelectionPolicy>(
        query, query_id, state_db_);
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

  vector<shared_future<Output>> task_completion_futures =
      task_executor_.schedule_predictions(tasks);
  future<void> timer_future = timer_system_.set_timer(query.latency_micros_);
  auto all_tasks_completed = boost::when_all(task_completion_futures.begin(),
                                             task_completion_futures.end());
  //  auto result = all_tasks_completed.get();
  //  for (auto r = std::get<0>(result); r != std::get<1>(result); ++r) {
  //    const auto output = (*r).get();
  //    output;
  //  }
  auto make_response_future =
      boost::when_any(std::move(all_tasks_completed), std::move(timer_future));
  //         int result = make_response_future.get();
  // using ComposedFuture =
  //     tuple<boost::future<tuple<vector<future<Output>>::iterator,
  //                               vector<future<Output>>::iterator>>,
  //           future<void>>;

  boost::promise<Response> promise;

  make_response_future.then([
    query, query_id, p = std::move(promise), s = std::move(serialized_state)
  ](auto result_future) mutable {

    auto result = result_future.get();
    vector<Output> outputs;
    vector<VersionedModelId> used_models;
    vector<shared_future<Output>> completed_tasks = std::get<0>(result).get();
    for (auto r = completed_tasks.begin(); r != completed_tasks.end(); ++r) {
      if ((*r).is_ready()) {
        outputs.push_back((*r).get());
      }
    }

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

  return promise.get_future();
}

// ignore tasks
//  return boost::make_ready_future<Response>(Response{
//      query, query_id, 20000, std::unique_ptr<Output>(new Output{1.0, "m1"}),
//      std::vector<VersionedModelId>()});
//}

boost::future<FeedbackAck> QueryProcessor::update(FeedbackQuery feedback) {
  std::cout << "received feedback for user " << feedback.user_id_ << std::endl;

  // TODO: Implement

  return boost::make_ready_future(true);
}

}  // namespace clipper
