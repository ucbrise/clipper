
#include <iostream>
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>

namespace clipper {

Output::Output(double y_hat, std::vector<VersionedModelId> model_id)
    : y_hat_(y_hat), model_id_(model_id) {}

DoubleVector::DoubleVector(std::vector<double> data) : data_(std::move(data)) {}

ByteBuffer DoubleVector::serialize() const {
  std::vector<uint8_t> bytes;
  for (auto&& i : data_) {
    auto cur_bytes = reinterpret_cast<const uint8_t*>(&i);
    for (unsigned long j = 0; j < sizeof(double); ++j) {
      bytes.push_back(cur_bytes[j]);
    }
  }
  return bytes;
}

Query::Query(std::string label, long user_id, std::shared_ptr<Input> input,
             long latency_micros, std::string selection_policy,
             std::vector<VersionedModelId> candidate_models)
    : label_(label),
      user_id_(user_id),
      input_(input),
      latency_micros_(latency_micros),
      selection_policy_(selection_policy),
      candidate_models_(candidate_models) {}

Response::Response(Query query, QueryId query_id, long duration_micros,
                   std::unique_ptr<Output> output,
                   std::vector<VersionedModelId> models_used)
    : query_(std::move(query)),
      query_id_(query_id),
      duration_micros_(duration_micros),
      output_(std::move(output)),
      models_used_(models_used) {}

std::string Response::debug_string() const noexcept {
  std::string debug;
  debug.append("Query id: ");
  debug.append(std::to_string(query_id_));
  debug.append(" Output: ");
  debug.append(std::to_string(output_->y_hat_));
  return debug;
}

Feedback::Feedback(std::shared_ptr<Input> input, 
                   std::shared_ptr<Output> output, 
                   VersionedModelId model_id)
    : y_(output.y_hat_),
      input_(input),
      model_id_(model_id) {}


FeedbackQuery::FeedbackQuery(std::string label, long user_id,
                             Feedback feedback,
                             std::string selection_policy,
                             std::vector<VersionedModelId> candidate_models)
    : label_(label),
      user_id_(user_id),
      feedback_(feedback),
      selection_policy_(selection_policy),
      candidate_models_(candidate_models) {}

}  // namespace clipper
