
#include <iostream>
#include <string>
#include <unordered_map>

#include <clipper/datatypes.hpp>
#include <clipper/query_processor.hpp>
#include <clipper/task_executor.hpp>

// #define BOOST_THREAD_VERSION 3
// #include <boost/thread.hpp>
// #include <boost/thread/future.hpp>

namespace clipper {

template <typename Policy>
std::vector<PredictTask> select_predict_tasks(Query query, long query_id,
                                              const StateDB& state_db) {
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

// TODO: This is a dummy implementation to get the API working
boost::future<Response> QueryProcessor::predict(Query query) {
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
    return boost::make_ready_future<Response>(Response{
        query, query_id, 20000, std::unique_ptr<Output>(new Output{1.0, "m1"}),
        std::vector<VersionedModelId>()});
  }

  // ignore tasks
  return boost::make_ready_future<Response>(Response{
      query, query_id, 20000, std::unique_ptr<Output>(new Output{1.0, "m1"}),
      std::vector<VersionedModelId>()});
}

boost::future<FeedbackAck> QueryProcessor::update(FeedbackQuery feedback) {
  std::cout << "received feedback for user " << feedback.user_id_ << std::endl;

  // TODO: Implement

  return boost::make_ready_future<FeedbackAck>(true);
}

}  // namespace clipper
