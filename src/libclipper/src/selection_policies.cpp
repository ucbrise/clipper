
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>

namespace clipper {

std::shared_ptr<SelectionPolicy> SelectionPolicyFactory::create(
    const std::string& policy_name) {
  if (policy_name.compare("most_recent")) {
    return std::make_shared<NewestModelSelectionPolicy>();
  } else {
    return std::shared_ptr<SelectionPolicy>(nullptr);
  }
}

NewestModelSelectionState::NewestModelSelectionState(VersionedModelId model)
    : model_id_(std::move(model)) {}

ByteBuffer NewestModelSelectionState::serialize() const {
  // TODO: IMPLEMENT
  std::vector<uint8_t> v;
  return v;
}

std::unique_ptr<NewestModelSelectionState>
NewestModelSelectionState::deserialize(const ByteBuffer& buffer) {
  auto vm = std::make_pair("m", 1);
  return std::unique_ptr<NewestModelSelectionState>(
      new NewestModelSelectionState(vm));
}

std::unique_ptr<SelectionState> NewestModelSelectionPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models) const {
  // TODO: IMPLEMENT
  auto vm = std::make_pair("m", 1);
  return std::unique_ptr<SelectionState>(new NewestModelSelectionState(vm));
}

std::unique_ptr<SelectionState> NewestModelSelectionPolicy::add_models(
    std::unique_ptr<SelectionState> state,
    std::vector<VersionedModelId> new_models) const {
  return state;
}

long NewestModelSelectionPolicy::hash_models(
    const std::vector<VersionedModelId>& candidate_models) const {
  return 0;
}

std::vector<PredictTask> NewestModelSelectionPolicy::select_predict_tasks(
    std::unique_ptr<SelectionState> state, Query query, long query_id) const {
  std::vector<PredictTask> task_vec{1};
  auto cast_state =
      dynamic_unique_ptr_cast<NewestModelSelectionState,
                              decltype(state.get_deleter())>(std::move(state));

  // construct the task and put in the vector
  task_vec.emplace_back(query.input_, cast_state->model_id_, 1.0, query_id_,
                        query.latency_micros_);
  return task_vec;
}

std::unique_ptr<Output> NewestModelSelectionPolicy::combine_predictions(
    std::unique_ptr<SelectionState> state, Query query,
    std::vector<std::shared_ptr<Output>> predictions) const {
  // just return the first prediction
  return predictions.front();
}

/// When feedback is received, the selection policy can choose
/// to schedule both feedback and prediction tasks. Prediction tasks
/// can be used to get y_hat for e.g. updating a bandit algorithm,
/// while feedback tasks can be used to optionally propogate feedback
/// into the model containers.
std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
NewestModelSelectionPolicy::select_feedback_tasks(
    std::unique_ptr<SelectionState> state, Query query) const {
  return std::pair(std::vector::empty(), std::vector::empty());
}

/// This method will be called if at least one PredictTask
/// was scheduled for this piece of feedback. This method
/// is guaranteed to be called sometime after all the predict
/// tasks scheduled by `select_feedback_tasks` complete.
std::unique_ptr<SelectionState> NewestModelSelectionPolicy::process_feedback(
    std::unique_ptr<SelectionState> state, Feedback feedback,
    std::vector<std::shared_ptr<Output>> predictions) const {
  return state;
}

ByteBuffer NewestModelSelectionPolicy::serialize_state(
    std::unique_ptr<SelectionState> state) const {
  return (static_cast<std::unique_ptr<NewestModelSelectionState>> state)
      ->serialize();
}

std::unique_ptr<SelectionState> NewestModelSelectionPolicy::deserialize_state(
    const ByteBuffer& bytes) const {
  return static_cast<std::unique_ptr<SelectionState>>
      NewestModelSelectionState::deserialize_state(bytes);
}

}  // namespace clipper
