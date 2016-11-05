
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
  // std::vector<PredictTask> tasks;
  // if (query.selection_policy_ == "newest_model") {
  //   tasks =
  //       select_tasks<NewestModelSelectionPolicy>(query, query_id, state_db_);
  // } else {
  //   std::cout << query.selection_policy_ << " is invalid selection policy"
  //             << std::endl;
  //   return boost::make_ready_future<Response>(Response{
  //       query, query_id, 20000, std::unique_ptr<Output>(new Output{1.0,
  //       "m1"}),
  //       std::vector<VersionedModelId>()});
  // }
  //////////////////////////////////////////////
  // use selection policy
  // auto found_policy = instantiated_policies_.find(query.selection_policy_);
  // std::shared_ptr<SelectionPolicy> policy;
  // if (found_policy != instantiated_policies_.end()) {
  //   policy = found_policy->second;
  // } else {
  //   if (auto init_policy =
  //           SelectionPolicyFactory::create(query.selection_policy_)) {
  //     policy = init_policy;
  //   } else {
  //     std::cerr << "Invalid policy" << std::endl;
  //     // TODO: Error handling for unknown policy
  //     return boost::make_ready_future<Response>(
  //         Response{query, query_id, 20000,
  //                  std::unique_ptr<Output>(new Output{1.0, "m1"}),
  //                  std::vector<VersionedModelId>()});
  //   }
  // }
  //
  // auto hashkey = policy->hash_models(query.candidate_models_);
  // std::unique_ptr<SelectionState> state;
  //
  // // = state_db_.get(std::tuple(query.label_, query.user_id_, hashkey));
  // if (auto state_opt =
  //         state_db_.get(StateKey{query.label_, query.user_id_, hashkey})) {
  //   auto serialized_state = *state_opt;
  //   state = policy->deserialize_state(serialized_state);
  // } else {
  //   state = policy->initialize(query.candidate_models_);
  // }
  // auto tasks = policy->select_predict_tasks(std::move(state), query,
  // query_id);
  //
  // // ignore tasks
  //
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
