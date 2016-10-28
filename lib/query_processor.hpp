#ifndef CLIPPER_LIB_QUERY_PROCESSOR_H
#define CLIPPER_LIB_QUERY_PROCESSOR_H

#include <string>
#include <utility>
#include <tuple>

#include <boost/thread.hpp>

#include "datatypes.h"

namespace clipper {

using VersionedModelId = std::tuple<std::string, int>;
using QueryId = long;

class Query {
  public:
    long user_id_;
    std::shared_ptr<Input> input_;
    long latency_micros_;
    std::string selection_policy_;
    std::vector<VersionedModelId> candidate_models_;
};


class Response {
  public:
    Query query_;
    QueryId query_id_;
    long duration_micros_;
    std::unique_ptr<Output> output_;
    std::vector<VersionedModelId> models_used_;
};

using Feedback = std::pair<std::shared_ptr<Input>, std::shared_ptr<Output>>;

class FeedbackQuery {
  public:
    long user_id_;
    std::vector<Feedback> feedback;
    std::string selection_policy_;
    std::vector<VersionedModelId> candidate_models_;
};


class QueryProcessor {
  public:
    // TODO virtual dtor?
    ~QueryProcessor() = default;

    // Disallow copies
    QueryProcessor(const QueryProcessor& other) = delete;
    QueryProcessor& operator=(const QueryProcessor& other) = delete;
    
    // Default move constructor and assignment.
    QueryProcessor(QueryProcessor&& other) = default;
    QueryProcessor& operator=(QueryProcessor&& other) = default;

    boost::future<Response> predict(Query query);
    boost::future<long> update(Query query);
};

} // namespace clipper


#endif

