#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <clipper/datatypes.hpp>
#include <clipper/json_util.hpp>
#include <clipper/logging.hpp>
#include <clipper/selection_policies.hpp>
#include <clipper/util.hpp>

namespace clipper {

DefaultOutputSelectionState::DefaultOutputSelectionState(Output default_output)
    : default_output_(default_output) {}

DefaultOutputSelectionState::DefaultOutputSelectionState(
    std::string serialized_state)
    : default_output_(deserialize(serialized_state)) {}

std::string DefaultOutputSelectionState::parse_y_hat(
    const std::shared_ptr<PredictionData>& default_y_hat) {
  auto default_data = get_data<char>(default_y_hat);
  std::string default_str(
      default_data.get() + default_y_hat->start(),
      default_data.get() + default_y_hat->start() + default_y_hat->size());
  return default_str;
}

std::string DefaultOutputSelectionState::serialize() const {
  rapidjson::Document d;
  d.SetObject();
  std::string default_str = parse_y_hat(default_output_.y_hat_);
  json::add_string(d, "y_hat", default_str);
  return json::to_json_string(d);
}
std::string DefaultOutputSelectionState::get_debug_string() const {
  rapidjson::Document d;
  d.SetObject();
  std::string default_str = parse_y_hat(default_output_.y_hat_);
  json::add_string(d, "y_hat", default_str);
  std::vector<std::string> empty_vec;
  json::add_string_array(d, "models_used", empty_vec);
  return json::to_json_string(d);
}

Output DefaultOutputSelectionState::deserialize(std::string serialized_state) {
  rapidjson::Document d;
  json::parse_json(serialized_state, d);
  return Output(json::get_string(d, "y_hat"), {});
}

std::string DefaultOutputSelectionPolicy::get_name() {
  return "DefaultOutputSelectionPolicy";
}

std::shared_ptr<SelectionState> DefaultOutputSelectionPolicy::init_state(
    Output default_output) const {
  return std::make_shared<DefaultOutputSelectionState>(default_output);
}

std::pair<std::vector<PredictTask>, std::vector<VersionedModelId>>
DefaultOutputSelectionPolicy::select_predict_tasks(
    const std::shared_ptr<SelectionState>& /*state*/, const Query& query,
    long first_subquery_id) const {
  std::vector<VersionedModelId> models;
  size_t num_candidate_models = query.candidate_models_.size();
  if (num_candidate_models == (size_t)0) {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "No candidate models for query with label {}",
                        query.label_);
  } else {
    if (num_candidate_models > 1) {
      log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                          "{} candidate models provided for query with label "
                          "{}. Picking the first one.",
                          num_candidate_models, query.label_);
    }
    models.emplace_back(query.candidate_models_.front());
  }

  std::vector<PredictTask> tasks;
  tasks.reserve(query.input_batch_.size());
  for (const auto& input : query.input_batch_) {
    tasks.emplace_back(input, first_subquery_id++,
                       query.latency_budget_micros_);
  }
  return std::make_pair(std::move(tasks), std::move(models));
}

std::vector<std::pair<Output, bool>>
DefaultOutputSelectionPolicy::combine_predictions(
    const std::shared_ptr<SelectionState>& state, const Query& query,
    std::vector<Output> predictions) const {
  std::vector<std::pair<Output, bool>> outputs;
  outputs.reserve(query.input_batch_.size());
  for (auto& p : predictions) {
    if (outputs.size() >= query.input_batch_.size()) {
      break;
    }

    if (p.y_hat_) {
      outputs.emplace_back(std::move(p), false);
    } else {
      outputs.emplace_back(std::dynamic_pointer_cast<DefaultOutputSelectionState>(state)
              ->default_output_, true);
    }
  }

  if (outputs.size() < query.input_batch_.size()) {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "DefaultOutputSelectionPolicy expecting {} "
                        "outputs but found {}. Filling with default.",
                        query.input_batch_.size(), predictions.size());
    Output default_output =
        std::dynamic_pointer_cast<DefaultOutputSelectionState>(state)
            ->default_output_;
    outputs.resize(query.input_batch_.size(),
                   std::make_pair(default_output, true));
  } else if (predictions.size() > query.input_batch_.size()) {
    log_error_formatted(LOGGING_TAG_SELECTION_POLICY,
                        "DefaultOutputSelectionPolicy only expecting {} "
                        "outputs but found {}. Returning the first {}.",
                        query.input_batch_.size(), predictions.size(),
                        query.input_batch_.size());
  }

  return outputs;
}

std::tuple<std::vector<PredictTask>, std::vector<FeedbackTask>,
           std::vector<VersionedModelId>>
DefaultOutputSelectionPolicy::select_feedback_tasks(
    const std::shared_ptr<SelectionState>& /*state*/,
    const FeedbackQuery& /*query*/, long /*query_id*/) const {
  return std::make_tuple<std::vector<PredictTask>, std::vector<FeedbackTask>,
                         std::vector<VersionedModelId>>({}, {}, {});
}

std::shared_ptr<SelectionState> DefaultOutputSelectionPolicy::process_feedback(
    std::shared_ptr<SelectionState> state, Feedback /*feedback*/,
    std::vector<Output> /*predictions*/) const {
  return state;
}

std::shared_ptr<SelectionState> DefaultOutputSelectionPolicy::deserialize(
    std::string serialized_state) const {
  return std::make_shared<DefaultOutputSelectionState>(serialized_state);
}

std::string DefaultOutputSelectionPolicy::serialize(
    std::shared_ptr<SelectionState> state) const {
  return std::dynamic_pointer_cast<DefaultOutputSelectionState>(state)
      ->serialize();
}

}  // namespace clipper
