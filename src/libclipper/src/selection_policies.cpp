
#include <iostream>
#include <functional>
#include <utility>
#include <string>
#include <vector>
#include <math.h> 

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>


namespace clipper {
  using Map = std::unordered_map<VersionedModelId, double>;
  using Exp3State = std::pair<double, Map>;
  using Exp4State = std::pair<double, Map>;

Exp3State Exp3Policy::initialize(
          const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map;
  double sum;
  for (VersionedModelId id: candidate_models_) {
    map.insert({id, 1.0});
    sum += 1.0;
  }
  return std::make_pair(sum, map);
}

Exp3State Exp3Policy::add_models(
          Exp3State state,
          const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id: new_models) {
    state.second.insert({id, avg});
  }
  return state;
}

VersionedModelId Exp3Policy::select(Exp3State state, 
                                    std::vector<VersionedModelId>& models) {
  // Helper function for selecting an arm
  auto sum = state.first;
  auto rand_num = rand()%1;
  VersionedModelId selected_model;
  auto it = models.begin();
  while (rand_num >= 0) {
    rand_num -= state.second[it] / sum;
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
  auto task = PredictTask(query.input_, selected_model, 1.0, query_id, query.latency_micros_);
  result.push_back(task);
  return result;
}

std::shared_ptr<Output> Exp3Policy::combine_predictions(
                        Exp3State state, 
                        Query query,
                        std::vector<std::shared_ptr<Output>> predictions) {
  // Don't need to do anything
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp3Policy::select_feedback_tasks(Exp3State state, 
                                  FeedbackQuery feedback,
                                  long query_id) {
  
  // Predict Task
  std::vector<PredictTask> predict_task;
  auto selected_model = select(state, feedback.candidate_models_);
  auto task1 = PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  predict_task.push_back(task1);
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  auto task2 = FeedbackTask(feedback.feedback_, selected_model, query_id, -1);
  feedback_task.push_back(task2);

  return std::make_pair(predict_task, feedback_task);
}


Exp3State Exp3Policy::process_feedback(Exp3State state, 
                                       Feedback feedback,
                                       std::vector<std::shared_ptr<Output>> predictions) {
  
  auto eta = 0.01;
  auto y_hat = predictions.front()->y_hat_;
  auto y = feedback.y_;
  auto s_i = state.second[feedback.model_id_];
  auto loss = 1;
  if (y != y_hat) loss = 0;
  state.second[feedback.model_id_] = s_i * exp (-eta * loss / (s_i / state.first)); //FIXME
  return state;
}

ByteBuffer Exp3Policy::serialize_state(Exp3State state) {
  //TODO
  std::vector<uint8_t> v;
  return v;
}

Exp3State Exp3Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
  Map map;
  return std::make_pair(0, map);
}


// Exp4
Exp4State Exp4Policy::initialize(
          const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map;
  double sum;
  for (VersionedModelId id: candidate_models_) {
    map.insert({id, 1.0});
    sum += 1.0;
  }
  return std::make_pair(sum, map);
}

Exp4State Exp4Policy::add_models(Exp4State state,
                                 const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id: new_models) {
    state.second.insert({id, avg});
  }
  return state;
}

std::vector<PredictTask> Exp4Policy::select_predict_tasks(
                         Exp4State state, 
                         Query query, 
                         long query_id) {
  // Pass along all models selected
  std::vector<PredictTask> tasks;
  for (VersionedModelId id: query.candidate_models_) {
      auto task = PredictTask(query.input_, id, 1.0, query_id, query.latency_micros_);
      tasks.push_back(task);
  }
  return tasks;
}

std::shared_ptr<Output> Exp4Policy::combine_predictions(Exp4State state, 
                                                        Query query,
                                                        std::vector<std::shared_ptr<Output>> predictions) {
  // Weighted Combination of All predictions
  auto y_hat = 0;
  std::vector<VersionedModelId> models;
  for (auto id_it = state.second.begin(), output_it = predictions.begin(); 
        (id_it != state.second.end()) && (output_it != predictions.end()); 
          ++id_it, output_it) {
    y_hat += (id_it.second / state.first) * (*output_it)->y_hat_;
    models.push_back(id_it.first);
  }
  auto result = std::make_shared<Output>(Output(y_hat, models));
  return result;
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
Exp4Policy::select_feedback_tasks(Exp4State state, 
                                  FeedbackQuery feedback,
                                  long query_id) {
  
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


Exp4State Exp4Policy::process_feedback(Exp4State state, 
                                       Feedback feedback,
                                       std::vector<std::shared_ptr<Output>> predictions) {
  // Update individual model distribution
  auto y = feedback.y_;
  auto eta = 0.01;
  for (auto id_it = state.second.begin(), output_it = predictions.begin(); 
        (id_it != state.second.end()) && (output_it != predictions.end()); 
          ++id_it, ++output_it) {
    // FIXME
    auto loss = 1;
    if (y != (*output_it)->y_hat_) loss = 0;
    id_it.second = id_it.second * exp (-eta * loss / (id_it.second / state.first));
  }
  return state;
}

ByteBuffer Exp4Policy::serialize_state(Exp4State state) {
  //TODO
  std::vector<uint8_t> v;
  return v;
}

Exp4State Exp4Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
  Map map;
  return std::make_pair(0, map);
}


}  // namespace clipper
