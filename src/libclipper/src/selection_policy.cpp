
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <stdlib.h>
#include <math.h> 

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>


namespace clipper {
  using Map = std::unordered_map<VersionedModelId, double>;
  using Exp3State = std::pair<double, Map>;

Exp3State Exp3Policy::initialize(
          const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map;
  double sum;
  for (VersionedModelId id: candidate_models_) {
    map.emplace(id, 1.0);
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
    state.second.emplace(id, avg);
  }
  return state;
}

VersionedModelId Exp3Policy::select(Exp3State state, std::vector<VersionedModelId>& models) {
  // Helper function for selecting an arm
  auto sum = state.first;
  auto rand_num = rand()%1;
  VersionedModelId selected_model;
  auto it = models.begin();
  while (rand_num >= 0) {
    rand_num -= state.second[it] / sum;
    selected_model = it;
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
  result.emplace(task);
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
  auto task = PredictTask(feedback.feedback_.input_, selected_model, -1, query_id, -1);
  predict_task.emplace(task);
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  auto task = FeedbackTask(feedback.feedback_, selected_model, query_id, -1)
  feedback_task.emplace(task);

  return std::make_pair(predict_task, feedback_task);
}


Exp3State Exp3Policy::process_feedback(Exp3State state, 
                                       Feedback feedback,
                                       std::vector<std::shared_ptr<Output>> predictions) {
  
  auto y_hat = predictions.front().y_hat_;
  auto y = feedback.y_;
  auto s_i = state.second[feedback.model_id_];
  state.second[feedback.model_id_] = s_i * exp (-miu * loss / (s_i / state.first)); //FIXME
  return state;
}

ByteBuffer Exp3Policy::serialize_state(State Exp3State) {
  //TODO
      // std::vector<uint8_t> v;
      // return v;
}

Exp3State Exp3Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
}


// Exp4
Exp4State Exp4Policy::initialize(
          const std::vector<VersionedModelId>& candidate_models_) {
  // Construct State
  Map map;
  double sum;
  for (VersionedModelId id: candidate_models_) {
    map.emplace(id, 1.0);
    sum += 1.0;
  }
  return std::make_pair(sum, map);
}

Exp4State Exp4Policy::add_models(Exp4State state,
                                 const std::vector<VersionedModelId>& new_models) {
  // Give new models average weight from old models
  auto avg = state.first / state.second.size();
  for (VersionedModelId id: new_models) {
    state.second.emplace(id, avg);
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
      tasks.emplace(task);
  }
  return tasks;
}

std::shared_ptr<Output> Exp4Policy::combine_predictions(Exp4State state, 
                                                        Query query,
                                                        std::vector<std::shared_ptr<Output>> predictions) {
  // Weighted Combination of All predictions
  auto result = 0;
  vector::iterator id_it;
  Map::iterator output_it;
  for (id_it = state.second.begin(), output_it = predictions.begin(); 
        (id_it != state.second.end()) && (output_it != predictions.end()); 
          ++id_it, output_it) {
    result += (id_it.second / state.first) * output_it.y_hat_;
  }
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
      predict_task.emplace(task);
  }
  // Feedback Task
  std::vector<FeedbackTask> feedback_task;
  for (VersionedModelId id: feedback.candidate_models_) {
      auto task = FeedbackTask(feedback.feedback_, id, query_id, -1);
      feedback_task.emplace(task);
  }

  return std::make_pair(predict_task, feedback_task);
}


Exp4State Exp4Policy::process_feedback(Exp4State state, 
                                       Feedback feedback,
                                       std::vector<std::shared_ptr<Output>> predictions) {
  // Update individual model distribution
  vector::iterator id_it;
  Map::iterator output_it;
  for (id_it = state.second.begin(), output_it = predictions.begin(); 
        (id_it != state.second.end()) && (output_it != predictions.end()); 
          ++id_it, output_it) {
    // FIXME
    id_it.second = id_it.second * exp (-miu * (feedback.y_ - output_it.y_hat_) / (id_it.second / state.first));
  }
  return state;
}

ByteBuffer Exp4Policy::serialize_state(State Exp4State) {
  //TODO
      // std::vector<uint8_t> v;
      // return v;
}

Exp4State Exp4Policy::deserialize_state(const ByteBuffer& bytes) {
  //TODO
}

}  // namespace clipper
