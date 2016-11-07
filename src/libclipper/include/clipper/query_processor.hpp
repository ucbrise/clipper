#ifndef CLIPPER_LIB_QUERY_PROCESSOR_H
#define CLIPPER_LIB_QUERY_PROCESSOR_H

#include <atomic>
#include <string>
#include <tuple>
#include <utility>

#define BOOST_THREAD_VERSION 4
#include <boost/thread.hpp>
#include <boost/thread/future.hpp>

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

template <typename Policy>
std::vector<PredictTask> select_tasks(Query query, long query_id,
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

}  // namespace clipper

#endif
