
#include <iostream>
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>
#include "rpc_generated.h"

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
  flatbuffers::FlatBufferBuilder fbb;

  std::vector<flatbuffers::Offset<ByteVec>> request_data;
  for(auto input : serialized_inputs_) {
    auto input_bytes = CreateByteVecDirect(fbb, &input);
    request_data.push_back(input_bytes);
  }
  auto table_vector = fbb.CreateVector(request_data);

  PredictRequestBuilder prediction_builder(fbb);
  switch(input_type_) {
    case InputType::Doubles:
      prediction_builder.add_data_type(DataType_Bytes);
      break;
    case InputType::Strings:
      prediction_builder.add_data_type(DataType_Strings);
      break;
  }
  prediction_builder.add_byte_data(table_vector);
  auto prediction_request = prediction_builder.Finish();

  RequestBuilder request_builder(fbb);
  request_builder.add_request_type(RequestType_Predict);
  request_builder.add_prediction_request(prediction_request);
  auto request = request_builder.Finish();

  FinishRequestBuffer(fbb, request);

  return std::vector<uint8_t>(fbb.GetBufferPointer(), fbb.GetCurrentBufferPointer() + fbb.GetSize());
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
