#ifndef CLIPPER_LIB_QUERY_PROCESSOR_H
#define CLIPPER_LIB_QUERY_PROCESSOR_H

#include <atomic>
#include <string>
#include <tuple>
#include <utility>

#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "persistent_state.hpp"
#include "selection_policies.hpp"
#include "task_executor.hpp"
#include "timers.hpp"
#include "rpc_service.hpp"

namespace clipper {

class QueryProcessor {
 public:
  ~QueryProcessor() = default;

  QueryProcessor();

  // Disallow copies
  QueryProcessor(const QueryProcessor& other) = delete;
  QueryProcessor& operator=(const QueryProcessor& other) = delete;

  // Default move constructor and assignment.
  QueryProcessor(QueryProcessor&& other) = default;
  QueryProcessor& operator=(QueryProcessor&& other) = default;

  boost::future<Response> predict(Query query);
  boost::future<FeedbackAck> update(FeedbackQuery feedback);
  
  std::shared_ptr<StateDB> get_state_table() const;

 private:
  std::atomic<long> query_counter_{0};
  std::shared_ptr<StateDB> state_db_;
  TaskExecutor<PowerTwoChoicesScheduler> task_executor_;
  TimerSystem<HighPrecisionClock> timer_system_{HighPrecisionClock()};
};

}  // namespace clipper

#endif
