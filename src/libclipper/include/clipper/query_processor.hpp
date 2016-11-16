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
#include "task_executor.hpp"
#include "timers.hpp"

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
  TimerSystem timer_system_;
};
    
template <typename Policy>
    std::pair<std::vector<PredictTask>, ByteBuffer> select_predict_tasks(
    Query query, long query_id, std::shared_ptr<StateDB> state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  ByteBuffer serialized_state;
  if (auto state_opt =
          state_db->get(StateKey{query.label_, query.user_id_, hashkey})) {
    serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
    serialized_state = Policy::serialize_state(state);
  }
  return std::make_pair(Policy::select_predict_tasks(state, query, query_id),
                        serialized_state);
}


template <typename Policy>
Output combine_predictions(Query query, std::vector<Output> predictions,
                           const ByteBuffer& serialized_state) {
  // typename Policy::state_type state;
  const auto state = Policy::deserialize_state(serialized_state);
  return Policy::combine_predictions(state, query, predictions);
}

template <typename Policy>
    std::pair<std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>, ByteBuffer>
select_feedback_tasks(
    FeedbackQuery query, long query_id, std::shared_ptr<StateDB> state_db) {
  auto hashkey = Policy::hash_models(query.candidate_models_);
  typename Policy::state_type state;
  ByteBuffer serialized_state;
  if (auto state_opt =
          state_db->get(StateKey{query.label_, query.user_id_, hashkey})) {
    serialized_state = *state_opt;
    // if auto doesn't work: Policy::state_type
    state = Policy::deserialize_state(serialized_state);
  } else {
    state = Policy::initialize(query.candidate_models_);
    serialized_state = Policy::serialize_state(state);
  }
  return std::make_pair(Policy::select_feedback_tasks(state, query, query_id),
                        serialized_state);
}

template <typename Policy>
void process_feedback(FeedbackQuery feedback,
std::vector<Output> predictions,
                           const ByteBuffer& serialized_state,
                           std::shared_ptr<StateDB> state_db) {
  // typename Policy::state_type state;
  const auto state = Policy::deserialize_state(serialized_state);
  auto new_state = Policy::process_feedback(state, feedback.feedback_, predictions);
  auto serialized_new_state = Policy::serialize_state(new_state);
  auto hashkey = Policy::hash_models(feedback.candidate_models_);
  state_db->put(StateKey{feedback.label_, feedback.user_id_, hashkey}, serialized_new_state);
}

  

}  // namespace clipper

#endif
