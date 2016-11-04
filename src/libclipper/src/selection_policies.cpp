
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>

namespace clipper {

VersionedModelId NewestModelSelectionPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models) {
  // TODO: IMPLEMENT
  return std::make_pair("m", 1);
}

VersionedModelId NewestModelSelectionPolicy::add_models(
    VersionedModelId state, std::vector<VersionedModelId> new_models) {
  return state;
}

long NewestModelSelectionPolicy::hash_models(
    const std::vector<VersionedModelId>& candidate_models) {
  return 0;
}

std::vector<PredictTask> NewestModelSelectionPolicy::select_predict_tasks(
    VersionedModelId state, Query query, long query_id) {
  std::vector<PredictTask> task_vec;
  // construct the task and put in the vector
  task_vec.emplace_back(query.input_, state, 1.0, query_id,
                        query.latency_micros_);
  return task_vec;
}

std::shared_ptr<Output> NewestModelSelectionPolicy::combine_predictions(
    VersionedModelId state, Query query,
    std::vector<std::shared_ptr<Output>> predictions) {
  // just return the first prediction
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
NewestModelSelectionPolicy::select_feedback_tasks(VersionedModelId state,
                                                  Query query) {
  return std::make_pair(std::vector<PredictTask>(),
                        std::vector<FeedbackTask>());
}

VersionedModelId NewestModelSelectionPolicy::process_feedback(
    VersionedModelId state, Feedback feedback,
    std::vector<std::shared_ptr<Output>> predictions) {
  return state;
}

ByteBuffer NewestModelSelectionPolicy::serialize_state(VersionedModelId state) {
  std::vector<uint8_t> v;
  return v;
}

VersionedModelId NewestModelSelectionPolicy::deserialize_state(
    const ByteBuffer& bytes) {
  return std::make_pair("m", 1);
}

}  // namespace clipper
