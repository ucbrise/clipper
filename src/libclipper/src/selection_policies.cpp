// #include <float.h>
// #include <math.h>
// #include <time.h>
#include <functional>
#include <iostream>
// #include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// #include <boost/archive/binary_iarchive.hpp>
// #include <boost/archive/binary_oarchive.hpp>
// #include <boost/serialization/string.hpp>
// #include <boost/serialization/unordered_map.hpp>
// #include <boost/serialization/utility.hpp>

#include <rapidjson/document.h>

#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/util.hpp>

namespace clipper {

// DefaultOutputSelectionState

DefaultOutputSelectionState::DefaultOutputSelectionState(Output default_output)
    : default_output_(default_output) {}

DefaultOutputSelectionState::DefaultOutputSelectionState(
    std::string serialized_state)
    : default_output_(deserialize(serialized_state)) {}

std::string DefaultOutputSelectionState::serialize() const {
  rapidjson::Document d;
  d.SetObject();
  json::add_double(d, "y_hat", default_output_.y_hat_);
  return json::to_json_string(d);
}
std::string DefaultOutputSelectionState::get_debug_string() const {
  rapidjson::Document d;
  d.SetObject();
  json::add_double(d, "y_hat", default_output_.y_hat_);
  std::vector<std::string> empty_vec;
  json::add_string_array(d, "models_used", empty_vec);
  return json::to_json_string(d);
}

Output DefaultOutputSelectionState::deserialize(std::string serialized_state) {
  rapidjson::Document d;
  json::parse_json(serialized_state, d);
  return Output(json::get_double(d, "y_hat"), {});
}

///////////////////// DefaultOutputSelectionPolicy ////////////////////

SelectionState DefaultOutputSelectionPolicy::init_state(
    Output default_output) const {
  return DefaultOutputSelectionState(default_output);
}

SelectionState DefaultOutputSelectionPolicy::update_candidate_models(
    SelectionState state,
    const std::vector<VersionedModelId>& candidate_models) const {
  return state;
}

std::vector<PredictTask> DefaultOutputSelectionPolicy::select_predict_tasks(
    SelectionState /*state*/, Query query, long query_id) const {
  std::vector<PredictTask> tasks;
  int num_candidate_models = query.candidate_models_.size();
  if (num_candidate_models == 0) {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "No candidate models for query with label {}",
                        query.label_);
  } else if (num_candidate_models == 1) {
    tasks.emplace_back(query.input_, query.candidate_models_.front(), 1.0,
                       query_id, query.deadline_);
  } else {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "{} candidate models provided for query with label "
                        "{}. Picking the first one.",
                        num_candidate_models, query.label_);
    tasks.emplace_back(query.input_, query.candidate_models_.front(), 1.0,
                       query_id, query.deadline_);
  }
  return tasks;
}

Output DefaultOutputSelectionPolicy::combine_predictions(
    const SelectionState& state, Query query,
    std::vector<Output> predictions) const {
  if (predictions.size() == 1) {
    return predictions.front();
  } else if (predictions.empty()) {
    return dynamic_cast<const DefaultOutputSelectionState&>(state)
        .default_output_;
  } else {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "DefaultOutputSelectionPolicy only expecting 1 "
                        "output but found {}. Returning the first one.",
                        predictions.size());
    return predictions.front();
  }
}

std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
DefaultOutputSelectionPolicy::select_feedback_tasks(
    const SelectionState& /*state*/, FeedbackQuery /*query*/,
    long /*query_id*/) const {
  return std::make_pair<std::vector<PredictTask>, std::vector<FeedbackTask>>(
      {}, {});
}

SelectionState DefaultOutputSelectionPolicy::process_feedback(
    SelectionState state, Feedback /*feedback*/,
    std::vector<Output> /*predictions*/) const {
  return state;
}

SelectionState DefaultOutputSelectionPolicy::deserialize(
    std::string serialized_state) const {
  return DefaultOutputSelectionState(serialized_state);
}

std::string DefaultOutputSelectionPolicy::serialize(
    SelectionState state) const {
  return dynamic_cast<const DefaultOutputSelectionState&>(state).serialize();
}

}  // namespace clipper
