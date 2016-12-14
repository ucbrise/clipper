
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/concurrency.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>

// hack to get around unused argument compiler errors

namespace clipper {

VersionedModelId NewestModelSelectionPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models) {
  // TODO: IMPLEMENT
  assert(candidate_models.size() > 0);
  return candidate_models.front();
}

VersionedModelId NewestModelSelectionPolicy::add_models(
    VersionedModelId state, std::vector<VersionedModelId> new_models) {
  UNUSED(state);
  return new_models.front();
}

long NewestModelSelectionPolicy::hash_models(
    const std::vector<VersionedModelId>& candidate_models) {
  UNUSED(candidate_models);
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

Output NewestModelSelectionPolicy::combine_predictions(
    VersionedModelId state, Query query, std::vector<Output> predictions) {
  UNUSED(state);
  UNUSED(query);
  // just return the first prediction
  if (predictions.empty()) {
    return Output{0.0, std::make_pair("none", 0)};
  } else {
    return predictions.front();
  }
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
NewestModelSelectionPolicy::select_feedback_tasks(VersionedModelId state,
                                                  FeedbackQuery query,
                                                  long query_id) {
  UNUSED(state);
  UNUSED(query);
  UNUSED(query_id);
  return std::make_pair(std::vector<PredictTask>(),
                        std::vector<FeedbackTask>());
}

VersionedModelId NewestModelSelectionPolicy::process_feedback(
    VersionedModelId state, Feedback feedback,
    std::vector<Output> predictions) {
  UNUSED(feedback);
  UNUSED(predictions);
  return state;
}

ByteBuffer NewestModelSelectionPolicy::serialize_state(VersionedModelId state) {
  std::vector<uint8_t> v;
  UNUSED(state);
  return v;
}

VersionedModelId NewestModelSelectionPolicy::deserialize_state(
    const ByteBuffer& bytes) {
  UNUSED(bytes);
  return std::make_pair("m", 1);
}

///////////////////////////////////////////////////////////

SimpleState SimplePolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models) {
  // TODO: IMPLEMENT
  assert(candidate_models.size() > 0);
  return SimpleState(candidate_models);
}

SimpleState SimplePolicy::add_models(SimpleState state,
                                     std::vector<VersionedModelId> new_models) {
  state.insert(state.end(), new_models.begin(), new_models.end());
  return state;
}

long SimplePolicy::hash_models(
    const std::vector<VersionedModelId>& candidate_models) {
  UNUSED(candidate_models);
  return 0;
}

std::vector<PredictTask> SimplePolicy::select_predict_tasks(SimpleState state,
                                                            Query query,
                                                            long query_id) {
  std::vector<PredictTask> task_vec;

  // construct the task and put in the vector
  for (auto v : state) {
    task_vec.emplace_back(query.input_, v, 1.0 / (float)state.size(), query_id,
                          query.latency_micros_);
  }
  return task_vec;
}

Output SimplePolicy::combine_predictions(SimpleState state, Query query,
                                         std::vector<Output> predictions) {
  UNUSED(state);
  UNUSED(query);
  // just return the first prediction
  if (predictions.empty()) {
    return Output{0.0, std::make_pair("none", 0)};
  } else {
    float sum = 0;
    for (auto o : predictions) {
      sum += o.y_hat_;
    }
    return Output{sum, std::make_pair("all", 0)};
  }
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
SimplePolicy::select_feedback_tasks(SimpleState state, FeedbackQuery query,
                                    long query_id) {
  UNUSED(state);
  UNUSED(query);
  UNUSED(query_id);
  std::vector<PredictTask> pred_tasks_vec;

  // construct the task and put in the vector
  for (auto v : state) {
    pred_tasks_vec.emplace_back(query.feedback_.first, v, 1.0, query_id, 10000);
  }
  return std::make_pair(pred_tasks_vec, std::vector<FeedbackTask>());
}

SimpleState SimplePolicy::process_feedback(SimpleState state, Feedback feedback,
                                           std::vector<Output> predictions) {
  UNUSED(feedback);
  UNUSED(predictions);
  return state;
}

ByteBuffer SimplePolicy::serialize_state(SimpleState state) {
  std::vector<uint8_t> v;
  UNUSED(state);
  return v;
}

SimpleState SimplePolicy::deserialize_state(const ByteBuffer& bytes) {
  UNUSED(bytes);
  return {std::make_pair("m", 1), std::make_pair("j", 1)};
}

}  // namespace clipper
