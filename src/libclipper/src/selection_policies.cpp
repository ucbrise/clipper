#include <float.h>
#include <math.h>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/util.hpp>

namespace clipper {

// *********
// * State *
// *********

void PolicyState::set_model_map(Map map) {
  model_map_ = map;
}
void PolicyState::add_model(VersionedModelId id, ModelInfo model) {
  model_map_.insert({id, model});
}
void PolicyState::set_weight_sum(double sum) {
  weight_sum_ = sum;
}

// ********
// * EXP3 *
// ********

PolicyState Exp3Policy::initialize(const std::vector<VersionedModelId>& candidate_models_) {
  
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"weight", 1.0}, {"max_loss", 0.0}};
    map.insert({id, info});
  }
  PolicyState state;
  state.set_model_map(map);
  state.set_weight_sum(map.size() * 1.0);
  return state;
}

PolicyState Exp3Policy::add_models(PolicyState state,
                            const std::vector<VersionedModelId>& new_models) {
  
  double avg;
  if (state.model_map_.empty()) { // State hasn't been initiated or no models
    avg = 1.0;
  } else {
    avg = state.weight_sum_ / state.model_map_.size();
  }
  
  for (VersionedModelId id : new_models) {
    ModelInfo info = {{"weight", avg}, {"max_loss", 0.0}};
    state.add_model(id, info);
    state.set_weight_sum(state.weight_sum_ + avg);
  }
  return state;
}

VersionedModelId Exp3Policy::select(PolicyState& state) {
  
  // Helper function for randomly drawing an arm based on its normalized weight
  VersionedModelId selected_model;
  if (state.model_map_.empty()) {
    std::cout << "No models to select from" << std::endl;
  } else {
    auto rand_num = rand() % 1; // Pick random number between [0, 1]
    auto it = state.model_map_.begin();
    while (rand_num >= 0) { // Find the corresponding model based on rand_num
      rand_num -= (1 - eta) * (it->second["weight"] / state.weight_sum_) +
                eta / state.model_map_.size();
      selected_model = it->first;
      it++;
    }
  }
  return selected_model;
}

std::vector<PredictTask> Exp3Policy::select_predict_tasks(PolicyState& state,
                                                Query query,
                                                long query_id) {
  auto selected_model = select(state);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output Exp3Policy::combine_predictions(PolicyState /*state*/, Query /*query*/,
                                       std::vector<Output> predictions) {
  
  if (predictions.empty()) {
    std::cout << "No predictions to combine" << std::endl;
    Output output;
    return output;
  }
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp3Policy::select_feedback_tasks(PolicyState& state, FeedbackQuery feedback,
                                  long query_id) {
  // Predict Task
  auto selected_model = select(state);
  auto predict_task = PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  std::vector<PredictTask> predict_tasks{predict_task};
  // Feedback Task
  auto feedback_task = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  std::vector<FeedbackTask> feedback_tasks{feedback_task};

  return make_pair(predict_tasks, feedback_tasks);
}

PolicyState Exp3Policy::process_feedback(PolicyState state, Feedback feedback,
                                       std::vector<Output> predictions) {
  
  if (predictions.empty()) { // Edge case
    std::cout << "No predictions, so can't update state." << std::endl;
    return state;
  }
  
  // Compute loss and find which model to update
  auto loss = std::abs(predictions.front().y_hat_ - feedback.y_);
  auto model_id = predictions.front().models_used_.front();
  // Update max loss and Normalize loss
  if (state.model_map_[model_id]["max_loss"] < loss)
    state.model_map_[model_id]["max_loss"] = loss;
  loss /= state.model_map_[model_id]["max_loss"];
  // Update arm with normalized loss
  auto s_i = state.model_map_[model_id]["weight"];
  state.model_map_[model_id]["weight"] += exp(-eta * loss / (s_i / state.weight_sum_));
  return state;
}

std::string Exp3Policy::serialize_state(PolicyState state) {
  std::stringstream ss;
  boost::archive::binary_oarchive oa(ss);
  oa << state.model_map_;
  oa << state.weight_sum_;
  return ss.str();
}

PolicyState Exp3Policy::deserialize_state(const std::string& bytes) {
  std::stringstream ss;
  ss << bytes;
  boost::archive::binary_iarchive ia(ss);
  Map map;
  PolicyState state;
  double sum;
  ia >> map;
  ia >> sum;
  state.set_model_map(map);
  state.set_weight_sum(sum);
  return state;
}

std::string Exp3Policy::state_debug_string(const PolicyState& state) {
  // State string example: "3.0;model1,1.0,0.4;model2,1.0,0.4;......."
  std::string string_state = "Exp3State;";
  if (state.model_map_.empty()) {
    std::cout << "State is empty" << std::endl;
    return string_state;
  }
  string_state += std::to_string(state.weight_sum_) + ";"; // Weight Sum
  for (auto it: state.model_map_) {
    string_state += it.first.first + std::to_string(it.first.second); // Model Name
    for (auto model_info_it : it.second) { // Model information
      string_state += "," + std::to_string(model_info_it.second);
    }
    string_state += ";";
  }
  return string_state;
};

//// ********
//// * EXP4 *
//// ********
//// Descripton: Ensemble Model Selection Policy, combined version of EXP3
PolicyState Exp4Policy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Same as Exp3
  return Exp3Policy::initialize(candidate_models_);
}

PolicyState Exp4Policy::add_models(
    PolicyState state, const std::vector<VersionedModelId>& new_models) {
  // Same as Exp3
  return Exp3Policy::add_models(state, new_models);
}

std::vector<PredictTask> Exp4Policy::select_predict_tasks(PolicyState& /*state*/,
                                                          Query query,
                                                          long query_id) {
  // Pass along all models selected
  std::vector<PredictTask> tasks;
  for (VersionedModelId id : query.candidate_models_) {
    auto task =
        PredictTask(query.input_, id, 1.0, query_id, query.latency_micros_);
    tasks.push_back(task);
  }
  return tasks;
}

Output Exp4Policy::combine_predictions(PolicyState state, Query /*query*/,
                                       std::vector<Output> predictions) {
  // Weighted Combination of All predictions
  auto y_hat = 0;
  std::vector<VersionedModelId> models;
  for (auto p : predictions) {
    auto model_id = (p.models_used_).front();
    y_hat += (state.model_map_[model_id]["weight"] / state.weight_sum_) * p.y_hat_;
    models.push_back(model_id);
  }
  auto output = Output(y_hat, models);
  return output;
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp4Policy::select_feedback_tasks(PolicyState& /*state*/, FeedbackQuery feedback,
                                  long query_id) {
  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  for (VersionedModelId id : feedback.candidate_models_) {
    auto predict_task = PredictTask(feedback.feedback_.input_, id, -1, query_id, -1);
    auto feedback_task = FeedbackTask(feedback.feedback_, id, query_id, -1);
    predict_tasks.push_back(predict_task);
    feedback_tasks.push_back(feedback_task);
  }
  return std::make_pair(predict_tasks, feedback_tasks);
}

PolicyState Exp4Policy::process_feedback(PolicyState state, Feedback feedback,
                                       std::vector<Output> predictions) {
  // Update every individual model's distribution
  for (auto p : predictions) {
    // Compute loss and find which model to update
    auto loss = std::abs(feedback.y_ - p.y_hat_);
    auto model_id = p.models_used_.front();
    // Update max loss and Normalize loss
    if (state.model_map_[model_id]["max_loss"] < loss) {
      state.model_map_[model_id]["max_loss"] = loss;
    }
    loss /= state.model_map_[model_id]["max_loss"];
    // Update arm with normalized loss
    auto s_i = state.model_map_[model_id]["weight"];
    state.model_map_[model_id]["weight"] += exp(-eta * loss / (s_i / state.weight_sum_));
  }
  return state;
}

std::string Exp4Policy::serialize_state(PolicyState state) {
  // Same as Exp3
  return Exp3Policy::serialize_state(state);
}

PolicyState Exp4Policy::deserialize_state(const std::string& bytes) {
  // Same as Exp3
  return Exp3Policy::deserialize_state(bytes);
}

std::string Exp4Policy::state_debug_string(const PolicyState& state) {
  return Exp3Policy::state_debug_string(state);
};

// ******************
// * Epsilon Greedy *
// ******************
// Descripton: Single Model Selection Policy, select lowest expected loss
PolicyState EpsilonGreedyPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Epsilon Greedy State: unordered_map (model_id, Pair (expected loss,
  // Vector(loss)))
  PolicyState state;
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"expected_loss", 0.0}, {"times_selected", 0.0}};
    map.insert({id, info});
  }
  state.set_model_map(map);
  return state;
}

PolicyState EpsilonGreedyPolicy::add_models(
    PolicyState state, const std::vector<VersionedModelId>& new_models) {
  // Calculate expected loss from old models
  auto sum = 0.0;
  for (auto model : state.model_map_) {
    sum += model.second.at("expected_loss");
  }
  auto avg = sum / state.model_map_.size();
  // Add new model with average reward
  for (auto id : new_models) {
    ModelInfo info = {{"expected_loss", avg}, {"times_selected", 0.0}};
    state.add_model(id, info);
  }
  return state;
}

VersionedModelId EpsilonGreedyPolicy::select(PolicyState& state) {
  
  // Helper function for selecting an arm based on lowest expected loss
  VersionedModelId selected_model;
  if (state.model_map_.empty()) { // Edge case
    std::cout << "No models to select from." << std::endl;
  } else {
    auto rand_num = rand() % 1;
    if (rand_num >= epsilon) { // Select best model
      auto min_loss = DBL_MAX;
      for (auto id = state.model_map_.begin(); id != state.model_map_.end(); ++id) {
        auto model_loss = id->second["expected_loss"];
        if (model_loss < min_loss) {
          min_loss = model_loss;
          selected_model = id->first;
        }
      }
    } else { // Randomly select
      auto random_it = next(begin(state.model_map_), rand() % state.model_map_.size());
      selected_model = random_it->first;
    }
  }

  return selected_model;
}

std::vector<PredictTask> EpsilonGreedyPolicy::select_predict_tasks(
    PolicyState& state, Query query, long query_id) {
  return Exp3Policy::select_predict_tasks(state, query, query_id);
}

Output EpsilonGreedyPolicy::combine_predictions(
    PolicyState state, Query query,
    std::vector<Output> predictions) {
  return Exp3Policy::combine_predictions(state, query, predictions);
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
EpsilonGreedyPolicy::select_feedback_tasks(PolicyState& state,
                                           FeedbackQuery feedback,
                                           long query_id) {
  return Exp3Policy::select_feedback_tasks(state, feedback, query_id);
}

PolicyState EpsilonGreedyPolicy::process_feedback(
    PolicyState state, Feedback feedback,
    std::vector<Output> predictions) {
  
  // Edge case
  if (predictions.empty()) {
    return state;
  }
  
  auto model_id = predictions.front().models_used_.front();
  auto new_loss = std::abs(feedback.y_ - predictions.front().y_hat_);
  // Update times selected
  state.model_map_[model_id]["times_selected"] += 1;
  // Update expected loss
  state.model_map_[model_id]["expected_loss"] +=
      (new_loss - state.model_map_[model_id]["expected_loss"]) /
      state.model_map_[model_id]["times_selected"];

  return state;
}

std::string EpsilonGreedyPolicy::serialize_state(PolicyState state) {
  std::stringstream ss;
  boost::archive::binary_oarchive oa(ss);
  oa << state.model_map_;
  return ss.str();
}

PolicyState EpsilonGreedyPolicy::deserialize_state(
    const std::string& bytes) {
  std::stringstream ss;
  ss << bytes;
  boost::archive::binary_iarchive ia(ss);
  PolicyState state;
  Map map;
  ia >> map;
  state.set_model_map(map);
  return state;
}

std::string EpsilonGreedyPolicy::state_debug_string(const PolicyState& state) {
  return Exp3Policy::state_debug_string(state);
};

// ********
// * UCB1 *
// ********
// Descripton: Single Model Selection Policy, similar to E-greedy besides select
// method
PolicyState UCBPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  return EpsilonGreedyPolicy::initialize(candidate_models_);
}

PolicyState UCBPolicy::add_models(
    PolicyState state, const std::vector<VersionedModelId>& new_models) {
  return EpsilonGreedyPolicy::add_models(state, new_models);
}

VersionedModelId UCBPolicy::select(PolicyState& state) {
  
  // Helper function for selecting an arm based on lowest upper confidence bound
  VersionedModelId selected_model;
  if (state.model_map_.empty()) { //Edge case
    std::cout << "No models to select from." << std::endl;
  } else {
    auto min_upper_bound = DBL_MAX;
    for (auto id = state.model_map_.begin(); id != state.model_map_.end(); ++id) {
      auto model_loss = id->second["expected_loss"];
      auto bound = sqrt(2 * log(state.model_map_.size()) / id->second["times_selected"]);
      if (model_loss + bound < min_upper_bound) {
        min_upper_bound = model_loss + bound;
        selected_model = id->first;
      }
    }
  }

  return selected_model;
}

std::vector<PredictTask> UCBPolicy::select_predict_tasks(PolicyState& state,
                                                         Query query,
                                                         long query_id) {
  return Exp3Policy::select_predict_tasks(state, query, query_id);
}

Output UCBPolicy::combine_predictions(PolicyState state, Query query,
                                      std::vector<Output> predictions) {
  return Exp3Policy::combine_predictions(state, query, predictions);
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
UCBPolicy::select_feedback_tasks(PolicyState& state, FeedbackQuery feedback,
                                 long query_id) {
  return Exp3Policy::select_feedback_tasks(state, feedback, query_id);
}

PolicyState UCBPolicy::process_feedback(PolicyState state, Feedback feedback,
                                     std::vector<Output> predictions) {
  return EpsilonGreedyPolicy::process_feedback(state, feedback, predictions);
}

std::string UCBPolicy::serialize_state(PolicyState state) {
  return EpsilonGreedyPolicy::serialize_state(state);
}

PolicyState UCBPolicy::deserialize_state(const std::string& bytes) {
  return EpsilonGreedyPolicy::deserialize_state(bytes);
}

std::string UCBPolicy::state_debug_string(const PolicyState& state) {
  return Exp3Policy::state_debug_string(state);
};

}  // namespace clipper
