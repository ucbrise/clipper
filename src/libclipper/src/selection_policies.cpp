
#include <cmath>
#include <memory>
#include <stdexcept>
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

std::string NewestModelSelectionPolicy::serialize_state(
    VersionedModelId state) {
  std::string v;
  UNUSED(state);
  return v;
}

VersionedModelId NewestModelSelectionPolicy::deserialize_state(
    const std::string& bytes) {
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

std::string SimplePolicy::serialize_state(SimpleState state) {
  std::string v;
  UNUSED(state);
  return v;
}

SimpleState SimplePolicy::deserialize_state(const std::string& bytes) {
  UNUSED(bytes);
  return {std::make_pair("m", 1), std::make_pair("j", 1)};
}

BanditState BanditPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models) {
  BanditState weights;
  float initial_weight = 1.0 / static_cast<float>(candidate_models.size());
  for (auto c : candidate_models) {
    weights.emplace_back(c, initial_weight);
  }
  return weights;
}

long BanditPolicy::hash_models(
    const std::vector<VersionedModelId>& /*candidate_models*/) {
  return 0;
}

std::vector<PredictTask> BanditPolicy::select_predict_tasks(BanditState state,
                                                            Query query,
                                                            long query_id) {
  std::vector<PredictTask> task_vec;

  // construct the task and put in the vector
  for (auto v : state) {
    task_vec.emplace_back(query.input_, v.first,
                          1.0 / static_cast<float>(state.size()), query_id,
                          query.latency_micros_);
  }
  return task_vec;
}

// expects binary classification models with -1, +1 labels
Output BanditPolicy::combine_predictions(BanditState state, Query /*query*/,
                                         std::vector<Output> predictions) {
  if (predictions.empty()) {
    return Output{-1.0, std::make_pair("none", 0)};
  } else {
    float score_sum = 0.0;
    float weight_sum = 0.0;
    for (auto p : predictions) {
      if (!(p.y_hat_ == -1.0 || p.y_hat_ == 1.0)) {
        std::stringstream error_str;
        error_str << "Model " << p.versioned_model_.first << ":"
                  << p.versioned_model_.first << " predicted label " << p.y_hat_
                  << ". Only labels of -1.0, 1.0 are supported"
                  << " by the Bandit selection policy";
        throw std::invalid_argument(error_str.str());
      }
      auto cur_model = p.versioned_model_;
      for (auto m : state) {
        if (p.versioned_model_ == m.first) {
          score_sum += m.second * p.y_hat_;
          weight_sum += m.second;
          break;
        }
      }
    }
    float weighted_score = score_sum / weight_sum;
    float pred_label = weighted_score >= 0.0 ? 1.0 : -1.0;

    // float score = std::round(score_sum / weight_sum);
    return Output{pred_label, std::make_pair("all", 0)};
  }
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
BanditPolicy::select_feedback_tasks(BanditState state, FeedbackQuery query,
                                    long query_id) {
  std::vector<PredictTask> pred_tasks_vec;

  // construct the task and put in the vector
  for (auto v : state) {
    pred_tasks_vec.emplace_back(query.feedback_.first, v.first, 1.0, query_id,
                                50000);
  }
  return std::make_pair(pred_tasks_vec, std::vector<FeedbackTask>());
}

BanditState BanditPolicy::process_feedback(BanditState state, Feedback feedback,
                                           std::vector<Output> predictions) {
  float weight_sum = 0.0;
  for (auto p : predictions) {
    auto cur_model = p.versioned_model_;
    for (std::pair<VersionedModelId, float>& model_weight : state) {
      if (p.versioned_model_ == model_weight.first) {
        if (p.y_hat_ == feedback.second.y_hat_) {
          // means this model predicted the right label
          model_weight.second *= 1.1;
          weight_sum += model_weight.second;
        } else {
          // means this model predicted the wrong label
          model_weight.second *= 0.9;
          weight_sum += model_weight.second;
        }
        break;
      }
    }
  }

  for (std::pair<VersionedModelId, float>& model_weight : state) {
    model_weight.second /= weight_sum;
  }
  return state;
}

std::string BanditPolicy::serialize_state(BanditState state) {
  std::ostringstream ss;
  // TODO: clean up this function. The nested pairs make it hard to read.
  for (auto m = state.begin(); m != state.end() - 1; ++m) {
    ss << m->first.first << ITEM_PART_CONCATENATOR << m->first.second
       << ITEM_PART_CONCATENATOR << std::to_string(m->second) << ITEM_DELIMITER;
  }
  // don't forget to save the last label
  ss << (state.end() - 1)->first.first << ITEM_PART_CONCATENATOR
     << (state.end() - 1)->first.second << ITEM_PART_CONCATENATOR
     << std::to_string((state.end() - 1)->second);
  std::cout << "BanditPolicy::serialize_state result: " << ss.str()
            << std::endl;
  return ss.str();
}

BanditState BanditPolicy::deserialize_state(const std::string& state_str) {
  size_t start = 0;
  size_t end = state_str.find(ITEM_DELIMITER);
  BanditState state;

  while (end != string::npos) {
    size_t vm_split =
        start +
        state_str.substr(start, end - start).find(ITEM_PART_CONCATENATOR);
    size_t weight_search_start = vm_split + ITEM_PART_CONCATENATOR.size();
    size_t weight_split =
        weight_search_start +
        state_str.substr(weight_search_start, end - weight_search_start)
            .find(ITEM_PART_CONCATENATOR);
    std::string model_name = state_str.substr(start, vm_split - start);
    std::cout << "model name: " << model_name << std::endl;
    std::string model_version_str = state_str.substr(
        weight_search_start, weight_split - weight_search_start);
    std::cout << "model version str: " << model_version_str << std::endl;
    int version = std::stoi(model_version_str);
    std::string weight_str = state_str.substr(
        weight_split + ITEM_PART_CONCATENATOR.size(), end - weight_split - 1);
    std::cout << "weight str: " << weight_str << std::endl;
    float weight = std::stof(weight_str);
    state.emplace_back(std::make_pair(model_name, version), weight);
    start = end + ITEM_DELIMITER.size();
    end = state_str.find(ITEM_DELIMITER, start);
  }

  // don't forget to parse the last model
  size_t vm_split =
      start + state_str.substr(start, end - start).find(ITEM_PART_CONCATENATOR);
  size_t weight_search_start = vm_split + ITEM_PART_CONCATENATOR.size();
  size_t weight_split =
      weight_search_start +
      state_str.substr(weight_search_start, end - weight_search_start)
          .find(ITEM_PART_CONCATENATOR);
  std::string model_name = state_str.substr(start, vm_split - start);
  std::cout << "model name: " << model_name << std::endl;
  std::string model_version_str =
      state_str.substr(weight_search_start, weight_split - weight_search_start);
  std::cout << "model version str: " << model_version_str << std::endl;
  int version = std::stoi(model_version_str);
  std::string weight_str = state_str.substr(
      weight_split + ITEM_PART_CONCATENATOR.size(), end - weight_split - 1);
  std::cout << "weight str: " << weight_str << std::endl;
  float weight = std::stof(weight_str);
  state.emplace_back(std::make_pair(model_name, version), weight);
  return state;
}

std::string BanditPolicy::state_debug_string(BanditState state) {
  return BanditPolicy::serialize_state(state);
}

}  // namespace clipper
