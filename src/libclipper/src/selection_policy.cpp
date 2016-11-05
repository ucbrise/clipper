
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>
#include <clipper/util.hpp>
#include <clipper/state.hpp>


namespace clipper {

std::vector<PredictTask> EpsilonGreedyPolicy::select_predict_tasks(
    EpsilonGreedyState state, Query query, long query_id) {
  std::vector<PredictTask> task_vec;
  // construct the task and put in the vector
  task_vec.emplace_back(query.input_, state, 1.0, query_id,
                        query.latency_micros_);
  return task_vec;
}

std::shared_ptr<Output> EpsilonGreedyPolicy::combine_predictions(
    EpsilonGreedyState state, Query query,
    std::vector<std::shared_ptr<Output>> predictions) {
  // just return the first prediction
  return predictions.front();
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
EpsilonGreedyPolicy::select_feedback_tasks(EpsilonGreedyState state,
                                                  Query query) {

  return std::make_pair(std::vector<PredictTask>(),
                        std::vector<FeedbackTask>());
}

VersionedModelId EpsilonGreedyPolicy::process_feedback(
    EpsilonGreedyState state, Feedback feedback,
    std::vector<std::shared_ptr<Output>> predictions) {
  return state;
}


}  // namespace clipper
