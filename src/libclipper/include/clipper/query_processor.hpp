#ifndef CLIPPER_LIB_QUERY_PROCESSOR_H
#define CLIPPER_LIB_QUERY_PROCESSOR_H

#include <atomic>
#include <string>
#include <tuple>
#include <utility>

#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "persistent_state.hpp"
#include "selection_policy.hpp"

namespace clipper {

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
  boost::future<FeedbackAck> update(FeedbackQuery feedback);

 private:
  std::atomic<long> query_counter_{0};
  StateDB state_db_{StateDB()};
};

}  // namespace clipper

#endif
