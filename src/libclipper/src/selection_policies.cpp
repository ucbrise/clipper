#include <float.h>
#include <math.h>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <time.h>

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

void BanditPolicyState::set_model_map(Map map) {
  model_map_ = map;
}
void BanditPolicyState::add_model(VersionedModelId id, ModelInfo model) {
  model_map_.insert({id, model});
}
void BanditPolicyState::set_weight_sum(double sum) {
  weight_sum_ = sum;
}

std::string BanditPolicyState::serialize() const {
  std::stringstream ss;
  boost::archive::binary_oarchive oa(ss);
  oa << weight_sum_ << model_map_.size() << model_map_; // save weight_sum, map size and map
  return ss.str();
};

BanditPolicyState BanditPolicyState::deserialize(const std::string& bytes) {
  
  std::stringstream ss;
  ss.str(bytes);
  boost::archive::binary_iarchive ia(ss);
  BanditPolicyState state;
  double sum;
  size_t size;
  ia >> sum >> size; // load weight_sum and map size
  Map map(size, &versioned_model_hash);
  ia >> map;
  state.set_model_map(map);
  state.set_weight_sum(sum);
  
  return state;
};

std::string BanditPolicyState::debug_string() const {
  /* State string representation:
      For each model: Model Name, Model ID, Model Property Value 1, Model Property Value 2 
      Different models are separated by semi-colon
      e.g. "3.0;classification,00001,1.0,0.4;regression,203422,1.0,0.4;......."
  */
  
  std::string string_state = "Exp3State;";
  if (model_map_.empty()) {
    std::cout << "State is empty" << std::endl;
    return string_state;
  }
  string_state += std::to_string(weight_sum_) + ";"; // Weight Sum
  for (auto it: model_map_) {
    string_state += it.first.first + std::to_string(it.first.second); // Model Name
    for (auto model_info_it : it.second) { // Model information
      string_state += "," + std::to_string(model_info_it.second);
    }
    string_state += ";";
  }
  return string_state;
};


// ********
// * EXP3 *
// ********

BanditPolicyState Exp3Policy::initialize(const std::vector<VersionedModelId>& candidate_models_) {
  
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"weight", 1.0}};
    map.insert({id, info});
  }
  BanditPolicyState state;
  state.set_model_map(map);
  state.set_weight_sum(map.size() * 1.0);
  return state;
}

BanditPolicyState Exp3Policy::add_models(BanditPolicyState state,
                            const std::vector<VersionedModelId>& new_models) {
  
  double avg;
  if (state.model_map_.empty()) { // State hasn't been initiated or no models
    avg = 1.0;
  } else {
    avg = state.weight_sum_ / state.model_map_.size();
  }
  
  for (VersionedModelId id : new_models) {
    ModelInfo info = {{"weight", avg}};
    state.add_model(id, info);
    state.set_weight_sum(state.weight_sum_ + avg);
  }
  return state;
}

VersionedModelId Exp3Policy::select(BanditPolicyState& state) {
  // Helper function for randomly drawing an arm based on its normalized weight
  VersionedModelId selected_model;
  if (state.model_map_.empty()) {
    std::cout << "No models to select from" << std::endl;
    return selected_model;
  }
  double rand_num = (double) rand() / (RAND_MAX); // Pick random number between [0, 1]
  for (auto it = state.model_map_.begin(); it != state.model_map_.end() && rand_num >= 0; ++it) {
    rand_num -= it->second["weight"] / state.weight_sum_;
    selected_model = it->first;
  }
  return selected_model;
}

std::vector<PredictTask> Exp3Policy::select_predict_tasks(BanditPolicyState state,
                                                Query query,
                                                long query_id) {
  auto selected_model = select(state);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output Exp3Policy::combine_predictions(BanditPolicyState /*state*/, Query /*query*/,
                                       std::vector<Output> predictions) {
  
  if (predictions.empty()) {
    std::cout << "No predictions to combine" << std::endl;
    Output output;
    return output;
  }
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp3Policy::select_feedback_tasks(BanditPolicyState& state, FeedbackQuery feedback,
                                  long query_id) {
  // Predict Task
  auto selected_model = select(state);
  auto predict_task = PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  std::vector<PredictTask> predict_tasks{predict_task};
  // Feedback Task
  // auto feedback_task = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  std::vector<FeedbackTask> feedback_tasks;

  return make_pair(predict_tasks, feedback_tasks);
}

BanditPolicyState Exp3Policy::process_feedback(BanditPolicyState state, Feedback feedback,
                                       std::vector<Output> predictions) {
  
  if (predictions.empty()) { // Edge case
    std::cout << "No predictions, so can't update state." << std::endl;
    return state;
  }
  
  // Compute loss and find which model to update
  auto loss = std::abs(predictions.front().y_hat_ - feedback.y_);
  auto model_id = predictions.front().models_used_.front();
  // Update arm weight and weight_sum
  auto s_i = state.model_map_[model_id]["weight"];
  if (s_i != 0) {
    auto update = exp(-eta * loss / (s_i / state.weight_sum_));
    state.model_map_[model_id]["weight"] = s_i * update;
    state.set_weight_sum(state.weight_sum_ - s_i + state.model_map_[model_id]["weight"]);
  }
  return state;
}

std::string Exp3Policy::serialize_state(BanditPolicyState state) {
  return state.serialize();
}

BanditPolicyState Exp3Policy::deserialize_state(const std::string& bytes) {
  return BanditPolicyState::deserialize(bytes);
}

std::string Exp3Policy::state_debug_string(const BanditPolicyState& state) {
  return state.debug_string();
};

//// ********
//// * EXP4 *
//// ********

BanditPolicyState Exp4Policy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  return Exp3Policy::initialize(candidate_models_);
}

BanditPolicyState Exp4Policy::add_models(
    BanditPolicyState state, const std::vector<VersionedModelId>& new_models) {
  return Exp3Policy::add_models(state, new_models);
}

std::vector<PredictTask> Exp4Policy::select_predict_tasks(BanditPolicyState& /*state*/,
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

Output Exp4Policy::combine_predictions(BanditPolicyState state, Query /*query*/,
                                  std::vector<Output> predictions) {
  // Weighted Combination of All predictions
  auto y_hat = 0;
  std::vector<VersionedModelId> models;
  for (auto p : predictions) {
    auto model_id = (p.models_used_).front();
    y_hat += (state.model_map_[model_id]["weight"] / state.weight_sum_) * p.y_hat_;
    models.push_back(model_id);
  }
  // Turn y_hat into either 0 or 1
  if (y_hat < 0.5) {
    y_hat = 0;
  } else {
    y_hat = 1;
  }
  
  auto output = Output(y_hat, models);
  return output;
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp4Policy::select_feedback_tasks(BanditPolicyState& /*state*/,
                              FeedbackQuery feedback,
                              long query_id) {
  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  for (VersionedModelId id : feedback.candidate_models_) {
    auto predict_task = PredictTask(feedback.feedback_.input_, id, -1, query_id, -1);
    // auto feedback_task = FeedbackTask(feedback.feedback_, id, query_id, -1);
    predict_tasks.push_back(predict_task);
    // feedback_tasks.push_back(feedback_task);
  }
  return std::make_pair(predict_tasks, feedback_tasks);
}

BanditPolicyState Exp4Policy::process_feedback(BanditPolicyState state,
                                  Feedback feedback,
                                  std::vector<Output> predictions) {
  
  if (predictions.empty()) { // Edge case
    std::cout << "No predictions, so can't update state." << std::endl;
    return state;
  }
  // Update every individual model's distribution
  for (auto p : predictions) {
    // Compute loss and find which model to update
    auto loss = std::abs(feedback.y_ - p.y_hat_);
    auto model_id = p.models_used_.front();

    // Update arm weight and weight_sum
    auto s_i = state.model_map_[model_id]["weight"];
    if (s_i != 0) {
      double update = exp(-eta * loss / (s_i / state.weight_sum_));
      state.model_map_[model_id]["weight"] *= update;
      state.set_weight_sum(state.weight_sum_ - s_i + state.model_map_[model_id]["weight"]);
    }
  }
  return state;
}

std::string Exp4Policy::serialize_state(BanditPolicyState state) {
  return state.serialize();
}

BanditPolicyState Exp4Policy::deserialize_state(const std::string& bytes) {
  return BanditPolicyState::deserialize(bytes);
}

std::string Exp4Policy::state_debug_string(const BanditPolicyState& state) {
  return state.debug_string();
};

// ******************
// * Epsilon Greedy *
// ******************

BanditPolicyState EpsilonGreedyPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {

  BanditPolicyState state;
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"expected_loss", 0.0}, {"times_selected", 0.0}};
    map.insert({id, info});
  }
  state.set_model_map(map);
  return state;
}

BanditPolicyState EpsilonGreedyPolicy::add_models(
    BanditPolicyState state, const std::vector<VersionedModelId>& new_models) {
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

VersionedModelId EpsilonGreedyPolicy::select(BanditPolicyState& state) {
  
  // Helper function for selecting an arm based on lowest expected loss
  VersionedModelId selected_model;
  if (state.model_map_.empty()) { // Edge case
    std::cout << "No models to select from." << std::endl;
    return selected_model;
  }
  double rand_num = (double) rand() / RAND_MAX;
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
    int rand_draw = rand() % state.model_map_.size();
    auto random_it = next(begin(state.model_map_), rand_draw);
    selected_model = random_it->first;
  }

  return selected_model;
}

std::vector<PredictTask> EpsilonGreedyPolicy::select_predict_tasks(
    BanditPolicyState& state, Query query, long query_id) {
  
  auto selected_model = select(state);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output EpsilonGreedyPolicy::combine_predictions(
    BanditPolicyState state, Query query,
    std::vector<Output> predictions) {
  return Exp3Policy::combine_predictions(state, query, predictions);
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
EpsilonGreedyPolicy::select_feedback_tasks(BanditPolicyState& state,
                                           FeedbackQuery feedback,
                                           long query_id) {
  return Exp3Policy::select_feedback_tasks(state, feedback, query_id);
}

BanditPolicyState EpsilonGreedyPolicy::process_feedback(
    BanditPolicyState state, Feedback feedback,
    std::vector<Output> predictions) {
  
  // Edge case
  if (predictions.empty()) {
    return state;
  }
  auto model_id = predictions.front().models_used_.front();
  auto new_loss = std::abs(feedback.y_ - predictions.front().y_hat_);
  // Update expected loss
  int times = state.model_map_[model_id]["times_selected"];
  state.model_map_[model_id]["expected_loss"] =
      (state.model_map_[model_id]["expected_loss"] * times + new_loss) / (times + 1);
  // Update times selected
  state.model_map_[model_id]["times_selected"] = times + 1;
  
  return state;
}

std::string EpsilonGreedyPolicy::serialize_state(BanditPolicyState state) {
  return state.serialize();
}

BanditPolicyState EpsilonGreedyPolicy::deserialize_state(const std::string& bytes) {
  return BanditPolicyState::deserialize(bytes);
}

std::string EpsilonGreedyPolicy::state_debug_string(const BanditPolicyState& state) {
  return state.debug_string();
};

// ********
// * UCB1 *
// ********

BanditPolicyState UCBPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  return EpsilonGreedyPolicy::initialize(candidate_models_);
}

BanditPolicyState UCBPolicy::add_models(
    BanditPolicyState state, const std::vector<VersionedModelId>& new_models) {
  return EpsilonGreedyPolicy::add_models(state, new_models);
}

VersionedModelId UCBPolicy::select(BanditPolicyState& state) {
  
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

std::vector<PredictTask> UCBPolicy::select_predict_tasks(BanditPolicyState& state,
                                                         Query query,
                                                         long query_id) {
  auto selected_model = select(state);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output UCBPolicy::combine_predictions(BanditPolicyState state, Query query,
                                      std::vector<Output> predictions) {
  return Exp3Policy::combine_predictions(state, query, predictions);
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
UCBPolicy::select_feedback_tasks(BanditPolicyState& state, FeedbackQuery feedback,
                                 long query_id) {
  return Exp3Policy::select_feedback_tasks(state, feedback, query_id);
}

BanditPolicyState UCBPolicy::process_feedback(BanditPolicyState state, Feedback feedback,
                                     std::vector<Output> predictions) {
  return EpsilonGreedyPolicy::process_feedback(state, feedback, predictions);
}

std::string UCBPolicy::serialize_state(BanditPolicyState state) {
  return state.serialize();
}

BanditPolicyState UCBPolicy::deserialize_state(const std::string& bytes) {
  return BanditPolicyState::deserialize(bytes);
}

std::string UCBPolicy::state_debug_string(const BanditPolicyState& state) {
  return state.debug_string();
};

}  // namespace clipper
