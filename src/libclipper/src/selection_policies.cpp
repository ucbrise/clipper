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

// ********
// * EXP3 *
// ********
// Descripton: Single Model Selection Policy, iteratively update weights
Exp3State Exp3Policy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Exp3: sum of weights; each model has "weight", "max loss"
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"weight", 1.0}, {"max loss", 0.0}};
    map.insert({id, info});
  }
  return make_pair(map.size() * 1.0, map);
}

Exp3State Exp3Policy::add_models(
    Exp3State state, const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id : new_models) {
    ModelInfo info = {{"weight", avg}, {"max loss", 0.0}};
    state.second.insert({id, info});
    state.first += avg;
  }
  return state;
}

VersionedModelId Exp3Policy::select(Exp3State state,
                                    std::vector<VersionedModelId>& models) {
  // Helper function for randomly drawing an arm based on its normalized weight
  auto rand_num = rand() % 1;
  VersionedModelId selected_model;
  auto it = models.begin();
  while (rand_num >= 0) {
    rand_num -= (1 - eta) * (state.second[*it]["weight"] / state.first) +
                eta / state.second.size();
    selected_model = *it;
    it++;
  }
  return selected_model;
}

std::vector<PredictTask> Exp3Policy::select_predict_tasks(Exp3State state,
                                                          Query query,
                                                          long query_id) {
  auto selected_model = select(state, query.candidate_models_);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output Exp3Policy::combine_predictions(Exp3State /*state*/, Query /*query*/,
                                       std::vector<Output> predictions) {
  // Don't need to do anything
  // TODO: what if predictions is empty?
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp3Policy::select_feedback_tasks(Exp3State state, FeedbackQuery feedback,
                                  long query_id) {
  // Predict Task
  auto selected_model = select(state, feedback.candidate_models_);
  auto task1 =
      PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  std::vector<PredictTask> predict_tasks{task1};
  // Feedback Task
  auto task2 = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  std::vector<FeedbackTask> feedback_tasks{task2};

  return make_pair(predict_tasks, feedback_tasks);
}

Exp3State Exp3Policy::process_feedback(Exp3State state, Feedback feedback,
                                       std::vector<Output> predictions) {
  // Compute loss and find which model to update
  auto loss = std::abs(predictions.front().y_hat_ - feedback.y_);
  auto model_id = predictions.front().models_used_.front();
  // Update max loss and Normalize loss
  if (state.second[model_id]["max loss"] < loss)
    state.second[model_id]["max loss"] = loss;
  loss /= state.second[model_id]["max loss"];
  // Update arm with normalized loss
  auto s_i = state.second[model_id]["weight"];
  state.second[model_id]["weight"] += exp(-eta * loss / (s_i / state.first));
  return state;
}

std::string Exp3Policy::serialize_state(Exp3State state) {
  std::stringstream ss;
  boost::archive::binary_oarchive oa(ss);
  oa << state.first;
  oa << state.second;
  return ss.str();
}

Exp3State Exp3Policy::deserialize_state(const std::string& bytes) {
  std::stringstream ss;
  ss << bytes;
  boost::archive::binary_iarchive ia(ss);
  double num;
  Map map;
  ia >> num;
  ia >> map;
  return std::make_pair(num, map);
}

// ********
// * EXP4 *
// ********
// Descripton: Ensemble Model Selection Policy, combined version of EXP3
Exp4State Exp4Policy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Same as Exp3
  return Exp3Policy::initialize(candidate_models_);
}

Exp4State Exp4Policy::add_models(
    Exp4State state, const std::vector<VersionedModelId>& new_models) {
  // Same as Exp3
  return Exp3Policy::add_models(state, new_models);
}

std::vector<PredictTask> Exp4Policy::select_predict_tasks(Exp4State /*state*/,
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

Output Exp4Policy::combine_predictions(Exp4State state, Query /*query*/,
                                       std::vector<Output> predictions) {
  // Weighted Combination of All predictions
  auto y_hat = 0;
  std::vector<VersionedModelId> models;

  for (auto p : predictions) {
    auto model_id = (p.models_used_).front();
    y_hat += (state.second[model_id]["weight"] / state.first) * p.y_hat_;
    models.push_back(model_id);
  }
  auto output = Output(y_hat, models);
  return output;
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp4Policy::select_feedback_tasks(Exp4State /*state*/, FeedbackQuery feedback,
                                  long query_id) {
  std::vector<PredictTask> predict_tasks;
  std::vector<FeedbackTask> feedback_tasks;
  for (VersionedModelId id : feedback.candidate_models_) {
    auto task1 = PredictTask(feedback.feedback_.input_, id, -1, query_id, -1);
    auto task2 = FeedbackTask(feedback.feedback_, id, query_id, -1);
    predict_tasks.push_back(task1);
    feedback_tasks.push_back(task2);
  }
  return std::make_pair(predict_tasks, feedback_tasks);
}

Exp4State Exp4Policy::process_feedback(Exp4State state, Feedback feedback,
                                       std::vector<Output> predictions) {
  // Update every individual model's distribution
  for (auto p : predictions) {
    // Compute loss and find which model to update
    auto loss = std::abs(feedback.y_ - p.y_hat_);
    auto model_id = p.models_used_.front();
    // Update max loss and Normalize loss
    if (state.second[model_id]["max loss"] < loss) {
      state.second[model_id]["max loss"] = loss;
    }
    loss /= state.second[model_id]["max loss"];
    // Update arm with normalized loss
    auto s_i = state.second[model_id]["weight"];
    state.second[model_id]["weight"] += exp(-eta * loss / (s_i / state.first));
  }
  return state;
}

std::string Exp4Policy::serialize_state(Exp4State state) {
  // Same as Exp3
  return Exp3Policy::serialize_state(state);
}

Exp4State Exp4Policy::deserialize_state(const std::string& bytes) {
  // Same as Exp3
  return Exp3Policy::deserialize_state(bytes);
}

// ******************
// * Epsilon Greedy *
// ******************
// Descripton: Single Model Selection Policy, select lowest expected loss
EpsilonGreedyState EpsilonGreedyPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Epsilon Greedy State: unordered_map (model_id, Pair (expected loss,
  // Vector(loss)))
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id : candidate_models_) {
    ModelInfo info = {{"expected loss", 0.0}, {"times selected", 0.0}};
    map.insert({id, info});
  }
  return map;
}

EpsilonGreedyState EpsilonGreedyPolicy::add_models(
    EpsilonGreedyState state, const std::vector<VersionedModelId>& new_models) {
  // Calculate expected loss from old models
  auto sum = 0.0;
  for (auto model : state) {
    sum += model.second.at("expected loss");
  }
  auto avg = sum / state.size();
  // Add new model with average reward
  for (auto id : new_models) {
    ModelInfo info = {{"expected loss", avg}, {"times selected", 0.0}};
    state.insert({id, info});
  }
  return state;
}

VersionedModelId EpsilonGreedyPolicy::select(
    EpsilonGreedyState state, std::vector<VersionedModelId>& models) {
  // Helper function for selecting an arm based on lowest expected loss
  auto rand_num = rand() % 1;
  VersionedModelId selected_model;

  if (rand_num >= epsilon) {  // Select best model
    auto min_loss = DBL_MAX;
    for (auto id = state.begin(); id != state.end(); ++id) {
      auto model_loss = id->second["expected loss"];
      if (model_loss < min_loss) {
        min_loss = model_loss;
        selected_model = id->first;
      }
    }
  } else {  // Randomly select
    auto random_it = next(begin(state), rand() % models.size());
    selected_model = random_it->first;
  }

  return selected_model;
}

std::vector<PredictTask> EpsilonGreedyPolicy::select_predict_tasks(
    EpsilonGreedyState state, Query query, long query_id) {
  auto selected_model = select(state, query.candidate_models_);
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id,
                          query.latency_micros_);
  std::vector<PredictTask> tasks{task};
  return tasks;
}

Output EpsilonGreedyPolicy::combine_predictions(
    EpsilonGreedyState /*state*/, Query /*query*/,
    std::vector<Output> predictions) {
  // Don't need to do anything
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
EpsilonGreedyPolicy::select_feedback_tasks(EpsilonGreedyState state,
                                           FeedbackQuery feedback,
                                           long query_id) {
  // Predict Task
  auto selected_model = select(state, feedback.candidate_models_);
  auto task1 =
      PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  std::vector<PredictTask> predict_tasks{task1};
  // Feedback Task
  auto task2 = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  std::vector<FeedbackTask> feedback_tasks{task2};

  return make_pair(predict_tasks, feedback_tasks);
}

EpsilonGreedyState EpsilonGreedyPolicy::process_feedback(
    EpsilonGreedyState state, Feedback feedback,
    std::vector<Output> predictions) {
  auto model_id = predictions.front().models_used_.front();
  auto new_loss = std::abs(feedback.y_ - predictions.front().y_hat_);
  // Update times selected
  state[model_id]["times selected"] += 1;
  // Update expected loss
  state[model_id]["expected loss"] +=
      (new_loss - state[model_id]["expected loss"]) /
      state[model_id]["times selected"];

  return state;
}

std::string EpsilonGreedyPolicy::serialize_state(EpsilonGreedyState state) {
  std::stringstream ss;
  boost::archive::binary_oarchive oa(ss);
  oa << state;
  return ss.str();
}

EpsilonGreedyState EpsilonGreedyPolicy::deserialize_state(
    const std::string& bytes) {
  std::stringstream ss;
  ss << bytes;
  boost::archive::binary_iarchive ia(ss);
  EpsilonGreedyState state;
  ia >> state;
  return state;
}

// ********
// * UCB1 *
// ********
// Descripton: Single Model Selection Policy, similar to E-greedy besides select
// method
UCBState UCBPolicy::initialize(
    const std::vector<VersionedModelId>& candidate_models_) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::initialize(candidate_models_);
}

UCBState UCBPolicy::add_models(
    EpsilonGreedyState state, const std::vector<VersionedModelId>& new_models) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::add_models(state, new_models);
}

VersionedModelId UCBPolicy::select(UCBState state,
                                   std::vector<VersionedModelId>& /*models*/) {
  // Helper function for selecting an arm based on lowest upper confidence bound
  VersionedModelId selected_model;
  auto min_upper_bound = DBL_MAX;
  for (auto id = state.begin(); id != state.end(); ++id) {
    auto model_loss = id->second["expected loss"];
    auto bound = sqrt(2 * log(state.size()) / id->second["times selected"]);
    if (model_loss + bound < min_upper_bound) {
      min_upper_bound = model_loss + bound;
      selected_model = id->first;
    }
  }

  return selected_model;
}

std::vector<PredictTask> UCBPolicy::select_predict_tasks(UCBState state,
                                                         Query query,
                                                         long query_id) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::select_predict_tasks(state, query, query_id);
}

Output UCBPolicy::combine_predictions(UCBState state, Query query,
                                      std::vector<Output> predictions) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::combine_predictions(state, query, predictions);
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
UCBPolicy::select_feedback_tasks(UCBState state, FeedbackQuery feedback,
                                 long query_id) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::select_feedback_tasks(state, feedback, query_id);
}

UCBState UCBPolicy::process_feedback(UCBState state, Feedback feedback,
                                     std::vector<Output> predictions) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::process_feedback(state, feedback, predictions);
}

std::string UCBPolicy::serialize_state(UCBState state) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::serialize_state(state);
}

UCBState UCBPolicy::deserialize_state(const std::string& bytes) {
  // Same as Epsilon Greedy
  return EpsilonGreedyPolicy::deserialize_state(bytes);
}

}  // namespace clipper
