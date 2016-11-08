#ifndef CLIPPER_LIB_SELECTION_POLICY_H
#define CLIPPER_LIB_SELECTION_POLICY_H

#include <memory>

#include "datatypes.hpp"
// #include "query_processor.hpp"
#include "task_executor.hpp"

namespace clipper {

template <typename Derived, typename State>
class SelectionPolicy {
 public:
  // don't let this class be instantiated
  SelectionPolicy() = delete;
  ~SelectionPolicy() = delete;
  static State initialize(
      const std::vector<VersionedModelId>& candidate_models) {
    return Derived::initialize(candidate_models);
  }
  // virtual std::unique_ptr<SelectionState> initialize(
  //     const std::vector<VersionedModelId>& candidate_models) const = 0;

  static State add_models(State state,
                          const std::vector<VersionedModelId>& new_models) {
    return Derived::add_models(std::forward(state), new_models);
  }

  // Used to identify a unique selection policy instance. For example,
  // if using a bandit-algorithm that does not tolerate variable-armed
  // bandits, one could hash the candidate models to identify
  // which policy instance corresponds to this exact set of arms.
  // Similarly, it provides flexibility in how to deal with different
  // versions of the same arm (different versions of same model).
  static long hash_models(
      const std::vector<VersionedModelId>& candidate_models) {
    return Derived::hash_models(candidate_models);
  }

  // On the prediction path
  static std::vector<PredictTask> select_predict_tasks(State state, Query query,
                                                       long query_id) {
    return Derived::select_predict_tasks(state, query, query_id);
  }

  // TODO: change this method name
  // TODO: I think it may make sense to decouple combine_predictions()
  // from select_predict_tasks in some cases
  static Output combine_predictions(
      State state, Query query,
      std::vector<Output> predictions) {
    return Derived::combine_predictions(std::forward(state), query,
                                        predictions);
  }

  /// When feedback is received, the selection policy can choose
  /// to schedule both feedback and prediction tasks. Prediction tasks
  /// can be used to get y_hat for e.g. updating a bandit algorithm,
  /// while feedback tasks can be used to optionally propogate feedback
  /// into the model containers.
  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(State state, Query query) {
    return Derived::select_feedback_tasks(std::forward(state), query);
  }

  /// This method will be called if at least one PredictTask
  /// was scheduled for this piece of feedback. This method
  /// is guaranteed to be called sometime after all the predict
  /// tasks scheduled by `select_feedback_tasks` complete.
  static State process_feedback(
      State state, Feedback feedback,
      std::vector<std::shared_ptr<Output>> predictions) {
    return Derived::process_feedback(std::forward(state), feedback,
                                     predictions);
  }

  static ByteBuffer serialize_state(State state) {
    return Derived::serialize_state(std::forward(state));
  }

  static State deserialize_state(const ByteBuffer& bytes) {
    return Derived::deserialize_state(bytes);
  }
};

// class EpsilonGreedyPolicy: public SelectionPolicy<EpsilonGreedyPolicy,
// EpsilonGreedyState> {
//
//   static double epsilon 0.5;
//    typedef EpsilonGreedyState state_type;
//   EpsilonGreedyPolicy() = delete;
//   ~EpsilonGreedyPolicy() = delete;
//
//   static EpsilonGreedyState initialize(
//       const std::vector<VersionedModelId>& candidate_models);
//
//   static EpsilonGreedyState add_models(EpsilonGreedyState state,
//                                      std::vector<VersionedModelId>
//                                      new_models);
//
//   static long hash_models(
//       const std::vector<VersionedModelId>& candidate_models);
//
//   static std::vector<PredictTask> select_predict_tasks(EpsilonGreedyState
//   state,
//                                                        Query query,
//                                                        long query_id);
//
//   static std::shared_ptr<Output> combine_predictions(
//       EpsilonGreedyState state, Query query,
//       std::vector<std::shared_ptr<Output>> predictions);
//
//   static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
//   select_feedback_tasks(EpsilonGreedyState state, Query query);
//
//   static EpsilonGreedyState process_feedback(
//       EpsilonGreedyState state, Feedback feedback,
//       std::vector<std::shared_ptr<Output>> predictions);
//
//   static ByteBuffer serialize_state(EpsilonGreedyState state);
//
//   static EpsilonGreedyState deserialize_state(const ByteBuffer& bytes);
//
// };

class NewestModelSelectionPolicy
    : public SelectionPolicy<NewestModelSelectionPolicy, VersionedModelId> {
 public:
  typedef VersionedModelId state_type;

  NewestModelSelectionPolicy() = delete;
  ~NewestModelSelectionPolicy() = delete;
  static VersionedModelId initialize(
      const std::vector<VersionedModelId>& candidate_models);

  static VersionedModelId add_models(VersionedModelId state,
                                     std::vector<VersionedModelId> new_models);

  static long hash_models(
      const std::vector<VersionedModelId>& candidate_models);

  static std::vector<PredictTask> select_predict_tasks(VersionedModelId state,
                                                       Query query,
                                                       long query_id);

  static Output combine_predictions(
      VersionedModelId state, Query query,
      std::vector<Output> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(VersionedModelId state, Query query);

  static VersionedModelId process_feedback(
      VersionedModelId state, Feedback feedback,
      std::vector<std::shared_ptr<Output>> predictions);

  static ByteBuffer serialize_state(VersionedModelId state);

  static VersionedModelId deserialize_state(const ByteBuffer& bytes);
};
}

#endif  // CLIPPER_LIB_SELECTION_POLICY_H
