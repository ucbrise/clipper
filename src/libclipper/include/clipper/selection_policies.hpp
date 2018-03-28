#ifndef CLIPPER_LIB_SELECTION_POLICIES_H
#define CLIPPER_LIB_SELECTION_POLICIES_H

#include <map>
#include <memory>
#include <unordered_map>

#include "datatypes.hpp"
#include "task_executor.hpp"

namespace clipper {

const std::string LOGGING_TAG_SELECTION_POLICY = "SELECTIONPOLICY";

/**
 * An instance of a SelectionPolicy class must be stateless and Clipper
 * provides no guarantees about which instance of a SelectionPolicy
 * will be used at any given time. The only reason SelectionPolicy
 * objects are created at all is that in C++ using object hierarchies
 * is the simplest way to achieve the type of polymorphic specialization
 * we need for selection policies.
 *
 * Because these SelectionPolicy instances are stateless, all state
 * needed for processing is encapsulated in a SelectionState object
 * which is managed by Clipper and stored in a persistent
 * database. The separate of policy and state allows Clipper to re-use
 * the same SelectionPolicy instance with different SelectionStates.
 */
class SelectionState {
 public:
  SelectionState() = default;
  SelectionState(const SelectionState&) = default;
  SelectionState& operator=(const SelectionState&) = default;
  SelectionState(SelectionState&&) = default;
  SelectionState& operator=(SelectionState&&) = default;
  virtual ~SelectionState() = default;
  virtual std::string get_debug_string() const = 0;
};

class SelectionPolicy {
  // Note that we need to use pointers to the SelectionState because
  // C++ doesn't let you have an object with an abstract type on the stack
  // because it doesn't know how big it is. It must be on heap.
  // We use shared_ptr instead of unique_ptr so that we can do pointer
  // casts down the inheritance hierarchy.

 public:
  SelectionPolicy() = default;
  SelectionPolicy(const SelectionPolicy&) = default;
  SelectionPolicy& operator=(const SelectionPolicy&) = default;
  SelectionPolicy(SelectionPolicy&&) = default;
  SelectionPolicy& operator=(SelectionPolicy&&) = default;
  virtual ~SelectionPolicy() = default;

  // Query Pre-processing: select models and generate tasks
  virtual std::vector<PredictTask> select_predict_tasks(
      std::shared_ptr<SelectionState> state, Query query,
      long query_id) const = 0;

  /// Combines multiple prediction results to produce a single
  /// output for a query.
  ///
  /// @returns A pair containing the output and a boolean flag
  /// indicating whether or not it is the default output
  virtual const std::pair<Output, bool> combine_predictions(
      const std::shared_ptr<SelectionState>& state, Query query,
      std::vector<Output> predictions) const = 0;

  /// When feedback is received, the selection policy can choose
  /// to schedule both feedback and prediction tasks. Prediction tasks
  /// can be used to get y_hat for e.g. updating a bandit algorithm,
  /// while feedback tasks can be used to optionally propogate feedback
  /// into the model containers.
  virtual std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(const std::shared_ptr<SelectionState>& state,
                        FeedbackQuery query, long query_id) const = 0;

  /// This method will be called if at least one PredictTask
  /// was scheduled for this piece of feedback. This method
  /// is guaranteed to be called sometime after all the predict
  /// tasks scheduled by `select_feedback_tasks` complete.
  virtual std::shared_ptr<SelectionState> process_feedback(
      std::shared_ptr<SelectionState> state, Feedback feedback,
      std::vector<Output> predictions) const = 0;

  virtual std::shared_ptr<SelectionState> deserialize(
      std::string serialized_state) const = 0;
  virtual std::string serialize(
      std::shared_ptr<SelectionState> state) const = 0;
};

////////////////////////////////////////////////////////////////////

class DefaultOutputSelectionState : public SelectionState {
 public:
  DefaultOutputSelectionState() = default;
  DefaultOutputSelectionState(const DefaultOutputSelectionState&) = default;
  DefaultOutputSelectionState& operator=(const DefaultOutputSelectionState&) =
      default;
  DefaultOutputSelectionState(DefaultOutputSelectionState&&) = default;
  DefaultOutputSelectionState& operator=(DefaultOutputSelectionState&&) =
      default;
  explicit DefaultOutputSelectionState(Output default_output);
  explicit DefaultOutputSelectionState(std::string serialized_state);
  ~DefaultOutputSelectionState() = default;
  std::string serialize() const;
  std::string get_debug_string() const override;
  Output default_output_;

 private:
  static Output deserialize(std::string serialized_state);
  static std::string parse_y_hat(
      const std::shared_ptr<PredictionData>& output_y_hat);
};

class DefaultOutputSelectionPolicy : public SelectionPolicy {
 public:
  DefaultOutputSelectionPolicy() = default;
  DefaultOutputSelectionPolicy(const DefaultOutputSelectionPolicy&) = default;
  DefaultOutputSelectionPolicy& operator=(const DefaultOutputSelectionPolicy&) =
      default;
  DefaultOutputSelectionPolicy(DefaultOutputSelectionPolicy&&) = default;
  DefaultOutputSelectionPolicy& operator=(DefaultOutputSelectionPolicy&&) =
      default;
  ~DefaultOutputSelectionPolicy() = default;

  static std::string get_name();

  std::shared_ptr<SelectionState> init_state(Output default_output) const;

  std::vector<PredictTask> select_predict_tasks(
      std::shared_ptr<SelectionState> state, Query query,
      long query_id) const override;

  const std::pair<Output, bool> combine_predictions(
      const std::shared_ptr<SelectionState>& state, Query query,
      std::vector<Output> predictions) const override;

  std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
  select_feedback_tasks(const std::shared_ptr<SelectionState>& state,
                        FeedbackQuery query, long query_id) const override;

  std::shared_ptr<SelectionState> process_feedback(
      std::shared_ptr<SelectionState> state, Feedback feedback,
      std::vector<Output> predictions) const override;

  std::shared_ptr<SelectionState> deserialize(
      std::string serialized_state) const override;
  std::string serialize(std::shared_ptr<SelectionState> state) const override;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_SELECTION_POLICIES_H
