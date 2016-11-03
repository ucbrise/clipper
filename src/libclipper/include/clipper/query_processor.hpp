#ifndef CLIPPER_LIB_QUERY_PROCESSOR_H
#define CLIPPER_LIB_QUERY_PROCESSOR_H

#include <atomic>
#include <string>
#include <tuple>
#include <utility>

#define BOOST_THREAD_VERSION 3
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

#include "datatypes.hpp"

namespace clipper {

using VersionedModelId = std::pair<std::string, int>;
using QueryId = long;

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, std::shared_ptr<Input> input,
        long latency_micros, std::string selection_policy,
        std::vector<VersionedModelId> candidate_models);

  // copy constructors
  Query(const Query&) = default;
  Query& operator=(const Query&) = default;

  // move constructors
  Query(Query&&) = default;
  Query& operator=(Query&&) = default;

  // Used to provide a namespace for queries. The expected
  // use is to distinguish queries coming from different
  // REST endpoints.
  std::string label_;
  long user_id_;
  std::shared_ptr<Input> input_;
  long latency_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
};

class Response {
 public:
  ~Response() = default;

  Response(Query query, QueryId query_id, long duration_micros,
           std::unique_ptr<Output> output,
           std::vector<VersionedModelId> models_used);

  // default copy constructors
  Response(const Response&) = default;
  Response& operator=(const Response&) = default;

  // default move constructors
  Response(Response&&) = default;
  Response& operator=(Response&&) = default;

  std::string debug_string() const noexcept;

  Query query_;
  QueryId query_id_;
  long duration_micros_;
  std::unique_ptr<Output> output_;
  std::vector<VersionedModelId> models_used_;
};

using Feedback = std::pair<std::shared_ptr<Input>, std::shared_ptr<Output>>;

class FeedbackQuery {
 public:
  ~FeedbackQuery() = default;
  FeedbackQuery(std::string label, long user_id, std::vector<Feedback> feedback,
                std::string selection_policy,
                std::vector<VersionedModelId> candidate_models);

  FeedbackQuery(const FeedbackQuery&) = default;
  FeedbackQuery& operator=(const FeedbackQuery&) = default;

  FeedbackQuery(FeedbackQuery&&) = default;
  FeedbackQuery& operator=(FeedbackQuery&&) = default;

  // Used to provide a namespace for queries. The expected
  // use is to distinguish queries coming from different
  // REST endpoints.
  std::string label_;
  long user_id_;
  std::vector<Feedback> feedback_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
};

class QueryProcessor {
 public:
  ~QueryProcessor() = default;

  QueryProcessor() = default;

  // Disallow copies
  QueryProcessor(const QueryProcessor& other) = delete;
  QueryProcessor& operator=(const QueryProcessor& other) = delete;

  // Default move constructor and assignment.
  QueryProcessor(QueryProcessor&& other) = default;
  QueryProcessor& operator=(QueryProcessor&& other) = default;

  boost::future<Response> predict(Query query);
  boost::future<long> update(FeedbackQuery feedback);

 private:
  // map to keep around an instance of each policy type. This is a hack
  // to get around the fact that we want polymorphic selection policies
  // (i.e. multiple policies that implement the same interface) but
  // can't have virtual static methods.
  // std::unordered_map<std::string, std::shared_ptr<SelectionPolicy>>
  //     instantiated_policies_;
  // std::queue query_queue_;
  std::atomic<long> query_counter_{0};
};

}  // namespace clipper

#endif
