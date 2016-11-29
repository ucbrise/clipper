
#include <iostream>
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>
#include "rpc.pb.h"

namespace clipper {

size_t versioned_model_hash(const VersionedModelId& key) {
  return std::hash<std::string>()(key.first) ^ std::hash<int>()(key.second);
}
//
//    struct VersionedModelHash {
//        std::size_t operator()(const VersionedModelId& k) const
//        {
//            return std::hash<std::string>()(k.first) ^
//            (std::hash<std::string>()(k.second) << 1);
//        }
//    };
//
//    struct VersionedModelEqual {
//        bool operator()(const Key& lhs, const Key& rhs) const
//        {
//            return lhs.first == rhs.first && lhs.second == rhs.second;
//        }
//    };

Output::Output(double y_hat, VersionedModelId versioned_model)
    : y_hat_(y_hat), versioned_model_(versioned_model) {}

DoubleVector::DoubleVector(std::vector<double> data) : data_(std::move(data)) {}

const ByteBuffer DoubleVector::serialize() const {
  std::vector<uint8_t> bytes;
  for (auto&& i : data_) {
    auto cur_bytes = reinterpret_cast<const uint8_t*>(&i);
    for (unsigned long j = 0; j < sizeof(double); ++j) {
      bytes.push_back(cur_bytes[j]);
    }
  }
  return bytes;
}

size_t DoubleVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<double>()(d);
  }
  return cur_hash;
}

void PredictionRequest::set_input_type(InputType input_type) {
  input_type_ = input_type;
}

void PredictionRequest::add_input(ByteBuffer serialized_input) {
  serialized_inputs_.push_back(std::move(serialized_input));
}

const ByteBuffer PredictionRequest::serialize() const {
  Request request;
  for(auto input : serialized_inputs_) {
    RequestData *requestData = request.add_request_data();
    requestData->set_data(input.data(), input.size());
  }
  switch(input_type_) {
    case InputType::Doubles:
      request.set_data_type(clipper::DataType::DOUBLES);
      break;
    case InputType::Strings:
      request.set_data_type(clipper::DataType::STRINGS);
      break;
  }
  request.set_request_type(clipper::RequestType::PREDICT);
  int size = request.ByteSize();
  void *data = malloc(size);
  request.SerializeToArray(data, size);
  ByteBuffer serialize((uint8_t *) data, (uint8_t *) data + size);
  free(data);
  return serialize;
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
                   Output output, std::vector<VersionedModelId> models_used)
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
  debug.append(std::to_string(output_.y_hat_));
  return debug;
}

FeedbackQuery::FeedbackQuery(std::string label, long user_id, Feedback feedback,
                             std::string selection_policy,
                             std::vector<VersionedModelId> candidate_models)
    : label_(label),
      user_id_(user_id),
      feedback_(feedback),
      selection_policy_(selection_policy),
      candidate_models_(candidate_models) {}

PredictTask::PredictTask(std::shared_ptr<Input> input, VersionedModelId model,
                         float utility, QueryId query_id,
                         long latency_slo_micros)
    : input_(std::move(input)),
      model_(model),
      utility_(utility),
      query_id_(query_id),
      latency_slo_micros_(latency_slo_micros) {}

FeedbackTask::FeedbackTask(Feedback feedback, VersionedModelId model,
                           QueryId query_id, long latency_slo_micros)
    : feedback_(feedback),
      model_(model),
      query_id_(query_id),
      latency_slo_micros_(latency_slo_micros) {}

}  // namespace clipper
