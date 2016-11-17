#ifndef CLIPPER_LIB_SELECTION_POLICY_H
#define CLIPPER_LIB_SELECTION_POLICY_H

#include <memory>
#include <unordered_map>
#include <stdlib.h>

#include "datatypes.hpp"
#include "task_executor.hpp"

namespace clipper {
  using Map = std::unordered_map<VersionedModelId, double,
                              std::function<size_t(const VersionedModelId&)>>;
  using Exp3State = std::pair<double, Map>;
  using Exp4State = std::pair<double, Map>;
  using EpsilonGreedyState = std::pair<VersionedModelId, Map>;

template <typename Derived, typename State>
class SelectionPolicy {
 public:
  // Don't let this class be instantiated
  SelectionPolicy() = delete;
  ~SelectionPolicy() = delete;

  static State initialize(const std::vector<VersionedModelId>& candidate_models);

  static State add_models(State state,
                         const std::vector<VersionedModelId>& new_models);

  // Used to identify a unique selection policy instance. For example,
  // if using a bandit-algorithm that does not tolerate variable-armed
  // bandits, one could hash the candidate models to identify
  // which policy instance corresponds to this exact set of arms.
  // Similarly, it provides flexibility in how to deal with different
  // versions of the same arm (different versions of same model).
  // static long hash_models(
  //     const std::vector<VersionedModelId>& candidate_models) {
  //   return Derived::hash_models(candidate_models);
  // }

  // Query Pre-processing: select models and generate tasks
  static std::vector<PredictTask> select_predict_tasks(State state,
                                                   Query query,
                                                   long query_id) {
    return Derived::select_predict_tasks(state, query, query_id);
  }

  // from select_predict_tasks in some cases
  static std::shared_ptr<Output> combine_predictions(
      State state, Query query,
      std::vector<std::shared_ptr<Output>> predictions) {
    return Derived::combine_predictions(std::forward(state), query, predictions);
  }

  /// When feedback is received, the selection policy can choose
  /// to schedule both feedback and prediction tasks. Prediction tasks
  /// can be used to get y_hat for e.g. updating a bandit algorithm,
  /// while feedback tasks can be used to optionally propogate feedback
  /// into the model containers.
  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(State state, FeedbackQuery feedback) {
    return Derived::select_feedback_tasks(std::forward(state), feedback);
  }

  /// This method will be called if at least one PredictTask
  /// was scheduled for this piece of feedback. This method
  /// is guaranteed to be called sometime after all the predict
  /// tasks scheduled by `select_feedback_tasks` complete.
  static State process_feedback(
      State state, Feedback feedback,
      std::vector<std::shared_ptr<Output>> predictions) {
    return Derived::process_feedback(std::forward(state), feedback, predictions);
  }

  static ByteBuffer serialize_state(State state) {
    return Derived::serialize_state(state);
  };

  static State deserialize_state(const ByteBuffer& bytes) {
    return Derived::deserialize_state(bytes);
  };

};



class Exp3Policy: public SelectionPolicy<Exp3Policy, Exp3State> {
  // Exp3
  // Select: weighted sampling
  // Update: update weights based on Loss and respond rate
  

public:
  Exp3Policy() = delete;
  ~Exp3Policy() = delete;
  
  constexpr static double eta = 0.01; // How fast clipper respond to feedback

  static Exp3State initialize(const std::vector<VersionedModelId>& candidate_models);

  static Exp3State add_models(Exp3State state,
                              const std::vector<VersionedModelId>& new_models);

  static std::vector<PredictTask> select_predict_tasks(Exp3State state,
                                                       Query query,
                                                       long query_id);

  static std::shared_ptr<Output> combine_predictions(
                                Exp3State state,
                                Query query,
                                std::vector<std::shared_ptr<Output>> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(Exp3State state, 
                        FeedbackQuery feedback,
                        long query_id);

  static Exp3State process_feedback(Exp3State state, 
                                    Feedback feedback,
                                    std::vector<std::shared_ptr<Output>> predictions);

  static ByteBuffer serialize_state(Exp3State state);

  static Exp3State deserialize_state(const ByteBuffer& bytes);

private:
  static VersionedModelId select(Exp3State state, 
                              std::vector<VersionedModelId>& models);
};



class Exp4Policy: public SelectionPolicy<Exp4Policy, Exp4State> {
  // Exp4
  // Select: all models
  // Update: update individual model weights (same as Exp3)

public:
  Exp4Policy() = delete;
  ~Exp4Policy() = delete;
  
  constexpr static double eta = 0.01;

  static Exp4State initialize(const std::vector<VersionedModelId>& candidate_models);

  static Exp4State add_models(Exp4State state,
                              const std::vector<VersionedModelId>& new_models);

  static std::vector<PredictTask> select_predict_tasks(Exp4State state,
                                                       Query query,
                                                       long query_id);

  static std::shared_ptr<Output> combine_predictions(
                                Exp4State state,
                                Query query,
                                std::vector<std::shared_ptr<Output>> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(Exp3State state, 
                        FeedbackQuery feedback,
                        long query_id);

  static Exp4State process_feedback(Exp4State state, 
                                  Feedback feedback,
                                  std::vector<std::shared_ptr<Output>> predictions);

  static ByteBuffer serialize_state(Exp4State state);

  static Exp4State deserialize_state(const ByteBuffer& bytes);

};

class EpsilonGreedyPolicy: public SelectionPolicy<Exp4Policy, Exp4State> {
  // Epsilon Greedy
  // Select: epsilon chance randomly select,
  //         (1-epsilon) change select model with the highest expected reward
  // Update: update individual model expected reward
  
public:
  EpsilonGreedyPolicy() = delete;
  ~EpsilonGreedyPolicy() = delete;
  
  constexpr static double epsilon = 0.1; // Random Selection Chance
  
  static EpsilonGreedyState initialize(
                        const std::vector<VersionedModelId>& candidate_models);
  
  static EpsilonGreedyState add_models(
                          EpsilonGreedyState state,
                          const std::vector<VersionedModelId>& new_models);
  
  static std::vector<PredictTask> select_predict_tasks(EpsilonGreedyState state,
                                                   Query query,
                                                   long query_id);
  
  static std::shared_ptr<Output> combine_predictions(
                                  EpsilonGreedyState state,
                                  Query query,
                                  std::vector<std::shared_ptr<Output>> predictions);
  
  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(EpsilonGreedyState state,
                        FeedbackQuery feedback,
                        long query_id);
  
  static EpsilonGreedyState process_feedback(
                           EpsilonGreedyState state,
                           Feedback feedback,
                           std::vector<std::shared_ptr<Output>> predictions);
  
  static ByteBuffer serialize_state(EpsilonGreedyState state);
  
  static EpsilonGreedyState deserialize_state(const ByteBuffer& bytes);

private:
  static VersionedModelId select(EpsilonGreedyState state,
                              std::vector<VersionedModelId>& models);
};

}

#endif  // CLIPPER_LIB_SELECTION_POLICY_H
