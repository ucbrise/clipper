#ifndef CLIPPER_LIB_SELECTION_POLICY_H
#define CLIPPER_LIB_SELECTION_POLICY_H

#include <map>
#include <memory>
#include <unordered_map>

#include "datatypes.hpp"
#include "task_executor.hpp"

namespace clipper {

// *********
// * State *
// *********

// Indicate information of a model
// Key as info name, value as info value
using ModelInfo = std::unordered_map<std::string, double>;
// A map of all models to corresponding model info
using Map = std::unordered_map<VersionedModelId, ModelInfo,
std::function<size_t(const VersionedModelId&)>>;

class PolicyState {
  public:
    PolicyState() = default;
    ~PolicyState() = default;
  
    void set_model_map(Map map);
    void add_model(VersionedModelId id, ModelInfo model);
    void set_weight_sum(double sum);
    std::string serialize() const;
    static PolicyState deserialize(const std::string& bytes);
    std::string debug_string() const;
  
    Map model_map_;
    double weight_sum_ = 0.0;
};

  
// **********
// * Policy *
// **********
template <typename Derived>
class SelectionPolicy {
 public:
  // Don't let this class be instantiated
  SelectionPolicy() = delete;
  ~SelectionPolicy() = delete;

  static PolicyState initialize(
      const std::vector<VersionedModelId>& candidate_models) {
    return Derived::initialize(candidate_models);
  };

  static PolicyState add_models(PolicyState state,
                          const std::vector<VersionedModelId>& new_models) {
    return Derived::add_models(state, new_models);
  };

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

  // Query Pre-processing: select models and generate tasks
  static std::vector<PredictTask> select_predict_tasks(PolicyState state, Query query,
                                                       long query_id) {
    return Derived::select_predict_tasks(state, query, query_id);
  }

  // TODO: change this method name
  // TODO: I think it may make sense to decouple combine_predictions()
  // from select_predict_tasks in some cases
  static Output combine_predictions(PolicyState state, Query query,
                                    std::vector<Output> predictions) {
    return Derived::combine_predictions(state, query,
                                        predictions);
  }

  /// When feedback is received, the selection policy can choose
  /// to schedule both feedback and prediction tasks. Prediction tasks
  /// can be used to get y_hat for e.g. updating a bandit algorithm,
  /// while feedback tasks can be used to optionally propogate feedback
  /// into the model containers.
  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(PolicyState& state, FeedbackQuery query, long query_id) {
    return Derived::select_feedback_tasks(state, query, query_id);
  }

  /// This method will be called if at least one PredictTask
  /// was scheduled for this piece of feedback. This method
  /// is guaranteed to be called sometime after all the predict
  /// tasks scheduled by `select_feedback_tasks` complete.
  static PolicyState process_feedback(PolicyState state, Feedback feedback,
                                std::vector<Output> predictions) {
    return Derived::process_feedback(state, feedback,
                                     predictions);
  }

  static std::string serialize_state(PolicyState state) {
    return Derived::serialize_state(state);
  }

  static PolicyState deserialize_state(const std::string& bytes) {
    return Derived::deserialize_state(bytes);
  }

  /**
   * Human readable debug string for the state
   */
  static std::string state_debug_string(const PolicyState& state) {
    return Derived::state_debug_string(state);
  }
};

class Exp3Policy : public SelectionPolicy<Exp3Policy> {
  // Exp3
  // Select: weighted sampling
  // Update: update weights based on Loss and respond rate

 public:
  Exp3Policy() = delete;
  ~Exp3Policy() = delete;
  typedef PolicyState state_type;

  constexpr static double eta = 0.01;  // How fast clipper respond to feedback

  static PolicyState initialize(
      const std::vector<VersionedModelId>& candidate_models);

  static PolicyState add_models(PolicyState state,
                              const std::vector<VersionedModelId>& new_models);

  static long hash_models(
      const std::vector<VersionedModelId>& /*candidate_models*/) {
    return 0;
  };

  static std::vector<PredictTask> select_predict_tasks(PolicyState state,
                                                       Query query,
                                                       long query_id);

  static Output combine_predictions(PolicyState state, Query query,
                                    std::vector<Output> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(PolicyState& state, FeedbackQuery query, long query_id);

  static PolicyState process_feedback(PolicyState state, Feedback feedback,
                                    std::vector<Output> predictions);

  static std::string serialize_state(PolicyState state);

  static PolicyState deserialize_state(const std::string& bytes);

  static std::string state_debug_string(const PolicyState& state);
  
 private:
  static VersionedModelId select(PolicyState state);
};

class Exp4Policy : public SelectionPolicy<Exp4Policy> {
  // Exp4
  // Select: all models
  // Update: update individual model weights (same as Exp3)

 public:
  Exp4Policy() = delete;
  ~Exp4Policy() = delete;
  typedef PolicyState state_type;

  constexpr static double eta = 0.01;

  static PolicyState initialize(
      const std::vector<VersionedModelId>& candidate_models);

  static PolicyState add_models(PolicyState state,
                              const std::vector<VersionedModelId>& new_models);

  static long hash_models(
      const std::vector<VersionedModelId>& /*candidate_models*/) {
    return 0;
  };

  static std::vector<PredictTask> select_predict_tasks(PolicyState& state,
                                                       Query query,
                                                       long query_id);

  static Output combine_predictions(PolicyState state, Query query,
                                    std::vector<Output> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(PolicyState& state, FeedbackQuery feedback, long query_id);

  static PolicyState process_feedback(PolicyState state, Feedback feedback,
                                    std::vector<Output> predictions);

  static std::string serialize_state(PolicyState state);

  static PolicyState deserialize_state(const std::string& bytes);
  
  static std::string state_debug_string(const PolicyState& state);
};

class EpsilonGreedyPolicy : public SelectionPolicy<Exp4Policy> {
  // Epsilon Greedy
  // Select: epsilon chance randomly select,
  //         (1-epsilon) change select model with the highest expected reward
  // Update: update individual model expected reward

 public:
  EpsilonGreedyPolicy() = delete;
  ~EpsilonGreedyPolicy() = delete;
  typedef PolicyState state_type;

  constexpr static double epsilon = 0.1;  // Random Selection Chance

  static PolicyState initialize(
      const std::vector<VersionedModelId>& candidate_models);

  static PolicyState add_models(
      PolicyState state,
      const std::vector<VersionedModelId>& new_models);

  static long hash_models(
      const std::vector<VersionedModelId>& /*candidate_models*/) {
    return 0;
  };

  static std::vector<PredictTask> select_predict_tasks(PolicyState& state,
                                                       Query query,
                                                       long query_id);

  static Output combine_predictions(PolicyState state, Query query,
                                    std::vector<Output> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(PolicyState& state, FeedbackQuery feedback,
                        long query_id);

  static PolicyState process_feedback(PolicyState state,
                                             Feedback feedback,
                                             std::vector<Output> predictions);

  static std::string serialize_state(PolicyState state);

  static PolicyState deserialize_state(const std::string& bytes);
  
  static std::string state_debug_string(const PolicyState& state);

 private:
  static VersionedModelId select(PolicyState& state);
};

class UCBPolicy : public SelectionPolicy<UCBPolicy> {
  // Upper Confidence Bound (UCB1)
  // Select: highest expected reward upper confidence bound
  // Update: update individual model expected reward upper confidence bound

 public:
  UCBPolicy() = delete;
  ~UCBPolicy() = delete;
  typedef PolicyState state_type;

  static PolicyState initialize(
      const std::vector<VersionedModelId>& candidate_models);

  static PolicyState add_models(PolicyState state,
                             const std::vector<VersionedModelId>& new_models);

  static long hash_models(
      const std::vector<VersionedModelId>& /*candidate_models*/) {
    return 0;
  };

  static std::vector<PredictTask> select_predict_tasks(PolicyState& state,
                                                       Query query,
                                                       long query_id);

  static Output combine_predictions(PolicyState state, Query query,
                                    std::vector<Output> predictions);

  static std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(PolicyState& state, FeedbackQuery feedback, long query_id);

  static PolicyState process_feedback(PolicyState state, Feedback feedback,
                                   std::vector<Output> predictions);

  static std::string serialize_state(PolicyState state);

  static PolicyState deserialize_state(const std::string& bytes);
  
  static std::string state_debug_string(const PolicyState& state);

 private:
  static VersionedModelId select(PolicyState& state);
};
}

#endif  // CLIPPER_LIB_SELECTION_POLICY_H
