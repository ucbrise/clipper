
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

Query::Query(std::string label, long user_id, std::shared_ptr<Input> input,
             long latency_micros, std::string selection_policy,
             std::vector<VersionedModelId> candidate_models)
    : label_(label),
      user_id_(user_id),
      input_(input),
      latency_micros_(latency_micros),
      selection_policy_(selection_policy),
      candidate_models_(candidate_models) {}

Response::Response(Query query, QueryId query_id, long duration_micros,
                   std::unique_ptr<Output> output,
                   std::vector<VersionedModelId> models_used)
    : query_(std::move(query)),
      query_id_(query_id),
      duration_micros_(duration_micros),
      output_(std::move(output)),
      models_used_(models_used) {}

std::string Response::debug_string() const noexcept {
  std::string debug;
  debug.append("Query id: ");
  debug.append(std::to_string(query_id_));
  debug.append(" Output: ");
  debug.append(std::to_string(output_->y_hat_));
  return debug;
}

FeedbackQuery::FeedbackQuery(std::string label, long user_id,
                             std::vector<Feedback> feedback,
                             std::string selection_policy,
                             std::vector<VersionedModelId> candidate_models)
    : label_(label),
      user_id_(user_id),
      feedback_(feedback),
      selection_policy_(selection_policy),
      candidate_models_(candidate_models) {}

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

  // // use selection policy
  // auto found_policy = instantiated_policies_.find(&query.selection_policy_);
  // auto policy;
  // if (found_policy != instantiated_policies_.end()) {
  //   policy = found_policy->second;
  // } else {
  //   if (auto init_policy =
  //   SelectionPolicyFactory::init(&query.selection_policy_)) {
  //       policy = init_policy;
  //   } else {
  //     std::cerr << "Invalid policy" << std::endl;
  //     return boost::make_ready_future<Response>(Response());
  //     // TODO: Error handling for unknown policy
  //   }
  // }
  //
  // auto hashkey = policy->hash_models(&query.candidate_models_);
  // auto state = state_db_.get(std::tuple(query.label_, query.user_id_,
  // hashkey));

  return boost::make_ready_future<Response>(
      Response{query, query_counter_.fetch_add(1), 20000,
               std::unique_ptr<Output>(new Output{1.0, "m1"}),
               std::vector<VersionedModelId>()});
}

boost::future<long> QueryProcessor::update(FeedbackQuery feedback) {
  std::cout << "received feedback for user " << feedback.user_id_ << std::endl;
  return boost::make_ready_future<long>(0);
}

}  // namespace clipper
