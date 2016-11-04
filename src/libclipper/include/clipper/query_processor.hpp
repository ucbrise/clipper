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
#include "persistent_state.hpp"
#include "selection_policy.hpp"

namespace clipper {

// forward declare SelectionPolicy
// class SelectionPolicy;

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
  // map to keep around an instance of each policy type. This is a hack
  // to get around the fact that we want polymorphic selection policies
  // (i.e. multiple policies that implement the same interface) but
  // can't have virtual static methods.
  std::unordered_map<std::string, std::shared_ptr<SelectionPolicy>>
      instantiated_policies_;
  // std::queue query_queue_;
  std::atomic<long> query_counter_{0};
  StateDB state_db_{StateDB()};
};

}  // namespace clipper

#endif
