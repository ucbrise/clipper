
#include <iostream>
#include <functional>
#include <utility>
#include <string>
#include <vector>
#include <math.h> 
#include <random>
#include <float.h>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>

#define UNUSED(expr) \
do {               \
(void)(expr);    \
} while (0)

namespace clipper {

//// Exp3
Exp3State Exp3Policy::initialize(
                          const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id: candidate_models_) {
    std::vector<double> rewards;
    map.insert({id, std::make_pair(1.0, rewards)});
  }
  return std::make_pair(map.size(), map);
}

Exp3State Exp3Policy::add_models(
                          Exp3State state,
                          const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id: new_models) {
    std::vector<double> rewards;
    state.second.insert({id, std::make_pair(avg, rewards)});
    state.first += avg;
  }
  return state;
}

VersionedModelId Exp3Policy::select(
                          Exp3State state,
                          std::vector<VersionedModelId>& models) {
  // Helper function for selecting an arm
  auto rand_num = rand()%1;
  VersionedModelId selected_model;
  auto it = models.begin();
  while (rand_num >= 0) {
    rand_num -= (1-eta) * (state.second[*it].first / state.first) + eta / state.second.size();
    selected_model = *it;
    it++;
  }
  return selected_model;

}

std::vector<PredictTask> Exp3Policy::select_predict_tasks(
                          Exp3State state,
                          Query query,
                          long query_id) {
  std::vector<PredictTask> result;
  auto selected_model = select(state, query.candidate_models_);
  auto task = PredictTask(query.input_, selected_model, 1.0,
                          query_id, query.latency_micros_);
  result.push_back(task);
  return result;
}

std::shared_ptr<Output> Exp3Policy::combine_predictions(
                          Exp3State state,
                          Query query,
                          std::vector<std::shared_ptr<Output>> predictions) {
  // Don't need to do anything
  UNUSED(state);
  UNUSED(query);
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp3Policy::select_feedback_tasks(
                          Exp3State state,
                          FeedbackQuery feedback,
                          long query_id) {
  
  // Predict Task
  std::vector<PredictTask> predict_task;
  auto selected_model = select(state, feedback.candidate_models_);
  auto task1 = PredictTask(feedback.feedback_.input_,
                           selected_model, -1, query_id, -1);
  predict_task.push_back(task1);
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  auto task2 = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  feedback_task.push_back(task2);

  return std::make_pair(predict_task, feedback_task);
}


Exp3State Exp3Policy::process_feedback(
                           Exp3State state,
                           Feedback feedback,
                           std::vector<std::shared_ptr<Output>> predictions) {
  
  // Compute loss and update that model
  auto y_hat = predictions.front()->y_hat_;
  auto y = feedback.y_;
  auto loss = std::abs(y - y_hat);
  auto losses = state.second[feedback.model_id_].second;
  losses.push_back(loss);
  // Normalize loss
  auto max_loss = std::max_element(std::begin(losses), std::end(losses));
  loss /= max_loss;
  // Update arm
  auto s_i = state.second[feedback.model_id_].first;
  state.second[feedback.model_id_].first
              += exp (-eta * loss / (s_i / state.first));
  return state;
}

ByteBuffer Exp3Policy::serialize_state(Exp3State state) {
  //TODO
  UNUSED(state);
  std::vector<uint8_t> v;
  return v;
}

Exp3State Exp3Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
  UNUSED(bytes);
  Map map;
  return std::make_pair(0, map);
}


//// Exp4
Exp4State Exp4Policy::initialize(
                            const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map(candidate_models_.size(), &versioned_model_hash);
  double sum = 0.0;
  for (VersionedModelId id: candidate_models_) {
    map.insert({id, 1.0});
    sum += 1.0;
  }
  return std::make_pair(sum, map);
}

Exp4State Exp4Policy::add_models(
                            Exp4State state,
                            const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id: new_models) {
    state.second.insert({id, avg});
    state.first += avg;
  }
  return state;
}

std::vector<PredictTask> Exp4Policy::select_predict_tasks(
                            Exp4State state,
                            Query query,
                            long query_id) {
  // Pass along all models selected
  UNUSED(state);
  UNUSED(query);
  std::vector<PredictTask> tasks;
  for (VersionedModelId id: query.candidate_models_) {
      auto task = PredictTask(query.input_, id, 1.0, query_id, query.latency_micros_);
      tasks.push_back(task);
  }
  return tasks;
}

std::shared_ptr<Output> Exp4Policy::combine_predictions(
                            Exp4State state,
                            Query query,
                            std::vector<std::shared_ptr<Output>> predictions) {
  // Weighted Combination of All predictions
  UNUSED(state);
  UNUSED(query);
  auto y_hat = 0;
  std::vector<VersionedModelId> models;
  
  // Iterate through all shared pointers that contain outputs in prediction vector
  for (auto id_it = predictions.begin(); id_it != predictions.end(); ++id_it) {
    auto model_id = ((*id_it)->model_id_).front();
    y_hat += (state.second[model_id] / state.first) * (*id_it)->y_hat_;
    models.push_back(model_id);
  }
  auto result = std::make_shared<Output>(Output(y_hat, models));
  return result;
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp4Policy::select_feedback_tasks(Exp4State state, 
                                FeedbackQuery feedback,
                                long query_id) {
  UNUSED(state);
  // Predict Task
  std::vector<PredictTask> predict_task;
  for (VersionedModelId id: feedback.candidate_models_) {
      auto task = PredictTask(feedback.feedback_.input_, id, -1, query_id, -1);
      predict_task.push_back(task);
  }
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  for (VersionedModelId id: feedback.candidate_models_) {
      auto task = FeedbackTask(feedback.feedback_, id, query_id, -1);
      feedback_task.push_back(task);
  }
  return std::make_pair(predict_task, feedback_task);
}


Exp4State Exp4Policy::process_feedback( // FIXME: Update function
                            Exp4State state,
                            Feedback feedback,
                            std::vector<std::shared_ptr<Output>> predictions) {
  // Update individual model distribution
  auto y = feedback.y_;
  for (auto id_it = predictions.begin(); id_it != predictions.end(); ++id_it) {
    auto loss = 1;
    if (y != (*id_it)->y_hat_)
      loss = 0;
    auto model_id = ((*id_it)->model_id_).front();
    auto model_weight = state.second[model_id];
    state.second[model_id]
              = model_weight * exp (-eta * loss / (model_weight / state.first));
  }
  return state;
}

ByteBuffer Exp4Policy::serialize_state(Exp4State state) {
  //TODO
  UNUSED(state);
  std::vector<uint8_t> v;
  return v;
}

Exp4State Exp4Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
  UNUSED(bytes);
  Map map;
  return std::make_pair(0, map);
}
  

// ************************
// **** Epsilon Greedy ****
// ************************
EpsilonGreedyState EpsilonGreedyPolicy::initialize(
                          const vector<VersionedModelId>& candidate_models_) {
  // Epsilon Greedy State: unordered_map (model_id, Pair (expected loss, Vector(loss)))
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id: candidate_models_) {
    vector<double> losses;
    map.insert({id, make_pair(0.0, losses)});
  }
  return map;
}

EpsilonGreedyState EpsilonGreedyPolicy::add_models(
                 EpsilonGreedyState state,
                 const vector<VersionedModelId>& new_models) {
  // Calculate average loss from old models
  auto sum = 0.0;
  for (auto it = state.begin(); it != state.end(); ++it) {
    sum += it->second.first;
  }
  auto avg = sum / state.size();
  // Add new model with average reward
  for (VersionedModelId id: new_models) {
    vector<double> losses;
    state.insert({id, make_pair(avg, losses)});
  }
  return state;
}

VersionedModelId EpsilonGreedyPolicy::select(
                          EpsilonGreedyState state,
                          vector<VersionedModelId>& models) {
  // Helper function for selecting an arm based on lowest expected loss
  auto rand_num = rand()%1;
  VersionedModelId selected_model;
  
  if (rand_num >= epsilon) { // Select best model
    auto min_loss = DBL_MAX;
    for (auto id = state.begin(); id != state.end(); ++id) {
      auto model_loss = id->second.first;
      if (model_loss < min_loss)
        min_loss = model_loss;
        selected_model = id->first;
    }
  } else { // Randomly select
    auto random_it = next(begin(state), rand()%models.size());
    selected_model = random_it->first;
  }
  
  return selected_model;
  
}

vector<PredictTask> EpsilonGreedyPolicy::select_predict_tasks(
                          EpsilonGreedyState state,
                          Query query,
                          long query_id) {
  auto selected_model = select(state, query.candidate_models_);
  auto task = PredictTask(query.input_, selected_model, 1.0,
                          query_id, query.latency_micros_);
  vector<PredictTask> tasks {task};
  return tasks;
}

shared_ptr<Output> EpsilonGreedyPolicy::combine_predictions(
                          EpsilonGreedyState state,
                          Query query,
                          vector<shared_ptr<Output>> predictions) {
  // Don't need to do anything
  UNUSED(state);
  UNUSED(query);
  return predictions.front();
}

pair<vector<PredictTask>, vector<FeedbackTask>>
EpsilonGreedyPolicy::select_feedback_tasks(
                          EpsilonGreedyState state,
                          FeedbackQuery feedback,
                          long query_id) {
  
  // Predict Task
  auto selected_model = select(state, feedback.candidate_models_);
  auto task1 = PredictTask(feedback.feedback_.input_,
                           selected_model, -1, query_id, -1);
  vector<PredictTask> predict_tasks {task1};
  // Feedback Task
  
  auto task2 = FeedbackTask(feedback.feedback_, feedback.feedback_.model_id_, query_id, -1);
  vector<FeedbackTask> feedback_tasks {task2};
  
  return std::make_pair(predict_tasks, feedback_tasks);
}


EpsilonGreedyState EpsilonGreedyPolicy::process_feedback(
                          EpsilonGreedyState state,
                          Feedback feedback,
                          vector<shared_ptr<Output>> predictions) {
  // Update the expected loss of one selected model
  auto model_id = predictions.front()->model_id_.front();
  auto new_loss = abs(feedback.y_ - predictions.front()->y_hat_);
  state[model_id].second.push_back(new_loss);
  state[model_id].first += (new_loss - state[model_id].first) / (state[model_id].second.size());
  
  return state;
}

ByteBuffer EpsilonGreedyPolicy::serialize_state(
                          EpsilonGreedyState state) {
  //TODO
  UNUSED(state);
  std::vector<uint8_t> v;
  return v;
}

EpsilonGreedyState EpsilonGreedyPolicy::deserialize_state(
                          const ByteBuffer& bytes) {
  //TODO
  UNUSED(bytes);
  VersionedModelId model;
  Map map;
  return map;
}

  
//// UCB
UCBState UCBPolicy::initialize(
                        const std::vector<VersionedModelId>& candidate_models_) {
  // Initiate all models' expected reward as 0.0
  Map map(candidate_models_.size(), &versioned_model_hash);
  for (VersionedModelId id: candidate_models_) {
    map.insert({id, 0.0});
  }
  return std::make_pair(candidate_models_.front(), map);
}

UCBState UCBPolicy::add_models(
                         EpsilonGreedyState state,
                         const std::vector<VersionedModelId>& new_models) {
  // Calculate average expected reward from old models
  auto sum = 0.0;
  for (auto it = state.second.begin(); it != state.second.end(); ++it) {
    sum += it->second;
  }
  auto avg = sum / state.second.size();
  // Add new model with average reward
  for (VersionedModelId id: new_models) {
    state.second.insert({id, avg});
  }
  return state;
}

VersionedModelId UCBPolicy::select(
                         UCBState state,
                         std::vector<VersionedModelId>& models) {
  UNUSED(models);
  // Helper function for selecting an arm
  VersionedModelId selected_model;
  auto max_CI_bound = -DBL_MAX;
  for (auto id = state.second.begin(); id != state.second.end(); ++id) {
    if (id->second > max_CI_bound)
      max_CI_bound = id->second;
      selected_model = id->first;
  }
  return selected_model;
  
}

std::vector<PredictTask> UCBPolicy::select_predict_tasks(
                                   UCBState state,
                                   Query query,
                                   long query_id) {
  std::vector<PredictTask> result;
  state.first = select(state, query.candidate_models_);
  auto task = PredictTask(query.input_, state.first, 1.0,
                          query_id, query.latency_micros_);
  result.push_back(task);
  return result;
}

std::shared_ptr<Output> UCBPolicy::combine_predictions(
                               UCBState state,
                               Query query,
                               std::vector<std::shared_ptr<Output>> predictions) {
  // Don't need to do anything
  UNUSED(state);
  UNUSED(query);
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
UCBPolicy::select_feedback_tasks(
             UCBState state,
             FeedbackQuery feedback,
             long query_id) {
  
  // Predict Task
  std::vector<PredictTask> predict_task;
  state.first = select(state, feedback.candidate_models_);
  auto task1 = PredictTask(feedback.feedback_.input_,
                           state.first, -1, query_id, -1);
  predict_task.push_back(task1);
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  auto task2 = FeedbackTask(feedback.feedback_, state.first, query_id, -1);
  feedback_task.push_back(task2);
  
  return std::make_pair(predict_task, feedback_task);
}


UCBState UCBPolicy::process_feedback( // FIXME: Update function
                   UCBState state,
                   Feedback feedback,
                   std::vector<std::shared_ptr<Output>> predictions) {
  // Update the expected reward of selected model
  auto model = (predictions.front()->model_id_).front();
  auto new_reward = 0.0;
  if (feedback.y_ == predictions.front()->y_hat_)
    new_reward = 1.0;
  state.second[feedback.model_id_] += new_reward;
  
  return state;
}

ByteBuffer UCBPolicy::serialize_state(UCBState state) {
  //TODO
  UNUSED(state);
  std::vector<uint8_t> v;
  return v;
}

UCBState UCBPolicy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
  UNUSED(bytes);
  VersionedModelId model;
  Map map;
  return std::make_pair(model, map);
}

}  // namespace clipper
