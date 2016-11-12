
#include <iostream>
#include <string>
#include <unordered_map>

#define BOOST_THREAD_VERSION 3
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#define BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY
#include <boost/thread.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>

// hack to get around unused argument compiler errors
#define UNREACHABLE() assert(false)

using boost::future;
using std::vector;

namespace clipper {

template<typename Policy>
vector<PredictTask> select_predict_tasks(Query query, long query_id,
                                         const StateDB &state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  if (auto state_opt =
      state_db.get(StateKey{query.label_, query.user_id_, hashkey})) {
    const auto serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
  }
  return Policy::select_predict_tasks(state, query, query_id);
}

// static std::shared_ptr<Output> combine_predictions
// State state, Query query,
// std::vector<Output> predictions);

template<typename Policy>
Output combine_predictions(Query query, std::vector<Output> predictions,
                                         const StateDB &state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  if (auto state_opt =
      state_db.get(StateKey{query.label_, query.user_id_, hashkey})) {
    const auto serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
  }
  return Policy::combine_predictions(state, query, predictions);
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
  if (query.selection_policy_ == "newest_model") {
    tasks = select_predict_tasks<NewestModelSelectionPolicy>(query, query_id,
                                                             state_db_);
    std::cout << "Used NewestModelSelectionPolicy to select tasks" << std::endl;
  } else {
    std::cout << query.selection_policy_ << " is invalid selection policy"
              << std::endl;
    // TODO better error handling
    return boost::make_ready_future(Response{
        query, query_id, 20000, Output{1.0, "m1"},
        std::vector<VersionedModelId>()});
  }

  vector<future<Output>> task_completion_futures = task_executor_.schedule_predictions(tasks);
  future<void> timer_future = timer_system_.set_timer(query.latency_micros_);
  auto all_tasks_completed = boost::when_all(task_completion_futures.begin(), task_completion_futures.end());
//  auto result = all_tasks_completed.get();
//  for (auto r = std::get<0>(result); r != std::get<1>(result); ++r) {
//    const auto output = (*r).get();
//    output;
//  }
  auto make_response_future = boost::when_any(std::move(all_tasks_completed), std::move(timer_future));
//  auto result = make_response_future.get();


  boost::promise<Response> promise;

  make_response_future.then([&](auto result) {
    std::vector<Output> outputs;
    std::vector<VersionedModelId> used_models;
    auto completed_tasks = std::get<0>(result).get();
    for (auto r = std::get<0>(completed_tasks); r != std::get<1>(completed_tasks); ++r) {
      if ((*r).is_ready()) {
        outputs.push_back((*r).get());
      }
    }

    Output final_output;
    if (query.selection_policy_ == "newest_model") {
      final_output = combine_predictions<NewestModelSelectionPolicy>(query, outputs, state_db_);
    } else {
      UNREACHABLE();
    }
    Response response(query, query_id, 0, final_output, )

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

  return boost::make_ready_future<FeedbackAck>(true);
}

}  // namespace clipper
