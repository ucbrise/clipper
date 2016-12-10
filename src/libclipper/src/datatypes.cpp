
#include <iostream>
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>

namespace clipper {

size_t versioned_model_hash(const VersionedModelId &key) {
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

const double *DoubleVector::get_serializable_data() const {
  // Revise this to be ownership safe
  return data_.data();
}

size_t DoubleVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<double>()(d);
  }
  return cur_hash;
}

size_t DoubleVector::size() const {
  return data_.size();
}

InputType DoubleVector::type() const {
  return InputType::Doubles;
}

ByteVector::ByteVector(std::vector<uint8_t> data) : data_(std::move(data)) {}

const uint8_t *ByteVector::get_serializable_data() const {
  // Revise this to be ownership safe
  return data_.data();
}

size_t ByteVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<uint8_t>()(d);
  }
  return cur_hash;
}

size_t ByteVector::size() const {
  return data_.size();
}

InputType ByteVector::type() const {
  return InputType::Bytes;
}

IntVector::IntVector(std::vector<int> data) : data_(std::move(data)) {}

const int *IntVector::get_serializable_data() const {
  // Revise this to be ownership safe
  return data_.data();
}

size_t IntVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<int>()(d);
  }
  return cur_hash;
}

size_t IntVector::size() const {
  return data_.size();
}

InputType IntVector::type() const {
  return InputType::Ints;
}

FloatVector::FloatVector(std::vector<float> data) : data_(std::move(data)) {}

const float *FloatVector::get_serializable_data() const {
  // Revise this to be ownership safe
  return data_.data();
}

size_t FloatVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<float>()(d);
  }
  return cur_hash;
}

size_t FloatVector::size() const {
  return data_.size();
}

InputType FloatVector::type() const {
  return InputType::Floats;
}

StringVector::StringVector(std::vector<std::string> data) : data_(std::move(data)) {}

const std::string *StringVector::get_serializable_data() const {
  // Revise this to be ownership safe
  return data_.data();
}

size_t StringVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<std::string>()(d);
  }
  return cur_hash;
}

size_t StringVector::size() const {
  return data_.size();
}

InputType StringVector::type() const {
  return InputType::Strings;
}

BatchPredictionRequest::BatchPredictionRequest(std::vector<std::shared_ptr<const Input>> inputs) {
  for(auto input : inputs) {
    add_input(input);
  }
}

void BatchPredictionRequest::add_input(std::shared_ptr<const Input> input) {
  switch(input->type()) {
    case InputType::Ints:
      int_inputs_.push_back(input);
      break;
    case InputType::Floats:
      float_inputs_.push_back(input);
      break;
    case InputType::Doubles:
      double_inputs_.push_back(input);
      break;
    case InputType::Bytes:
      byte_inputs_.push_back(input);
      break;
    case InputType::Strings:
      string_inputs_.push_back(input);
      break;
  }
}

const ByteBuffer BatchPredictionRequest::serialize() const {
  flatbuffers::FlatBufferBuilder fbb;

  SerializableVector<IntVec> serializable_ints = get_serializable_ints(fbb);
  SerializableVector<FloatVec> serializable_floats = get_serializable_floats(fbb);
  SerializableVector<DoubleVec> serializable_doubles = get_serializable_doubles(fbb);
  SerializableVector<ByteVec> serializable_bytes = get_serializable_bytes(fbb);
  SerializableVector<StringVec> serializable_strings = get_serializable_strings(fbb);

  PredictRequestBuilder predict_request_builder(fbb);
  predict_request_builder.add_integer_data(serializable_ints);
  predict_request_builder.add_float_data(serializable_floats);
  predict_request_builder.add_double_data(serializable_doubles);
  predict_request_builder.add_byte_data(serializable_bytes);
  predict_request_builder.add_string_data(serializable_strings);
  flatbuffers::Offset<PredictRequest> predict_request = predict_request_builder.Finish();

  RequestBuilder request_builder(fbb);
  request_builder.add_request_type(RequestType::RequestType_Predict);
  request_builder.add_prediction_request(predict_request);
  flatbuffers::Offset<Request> request = request_builder.Finish();

  FinishRequestBuffer(fbb, request);
  return std::vector<uint8_t>(fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize());
}

const SerializableVector<IntVec> BatchPredictionRequest::get_serializable_ints(
    flatbuffers::FlatBufferBuilder &fbb) const {
  std::vector<flatbuffers::Offset<IntVec>> raw_request_data;
  std::vector<int> buffer;

  for (int i = 0; i < (int) int_inputs_.size(); i++) {
    const IntVector *int_input = dynamic_cast<const IntVector *>(int_inputs_[i].get());
    size_t input_size = int_input->size();
    if (input_size > buffer.size()) {
      buffer.resize(input_size);
    }
    int *raw_buffer = buffer.data();
    auto raw_vec = fbb.CreateUninitializedVector(input_size, &raw_buffer);
    memcpy(raw_buffer, int_input->get_serializable_data(), input_size * sizeof(int));
    flatbuffers::Offset<IntVec> int_vec = CreateIntVec(fbb, raw_vec);
    raw_request_data.push_back(int_vec); //Copying an offset should be fine - check performance!!!
  }

  SerializableVector<IntVec> request_data = fbb.CreateVector(raw_request_data);
  return request_data;
}

const SerializableVector<FloatVec> BatchPredictionRequest::get_serializable_floats(
    flatbuffers::FlatBufferBuilder &fbb) const {
  std::vector<flatbuffers::Offset<FloatVec>> raw_request_data;
  std::vector<float> buffer;

  for (int i = 0; i < (int) float_inputs_.size(); i++) {
    const FloatVector *float_input = dynamic_cast<const FloatVector *>(float_inputs_[i].get());
    size_t input_size = float_input->size();
    if (input_size > buffer.size()) {
      buffer.resize(input_size);
    }
    float *raw_buffer = buffer.data();
    auto raw_vec = fbb.CreateUninitializedVector(input_size, &raw_buffer);
    memcpy(raw_buffer, float_input->get_serializable_data(), input_size * sizeof(float));
    flatbuffers::Offset<FloatVec> float_vec = CreateFloatVec(fbb, raw_vec);
    raw_request_data.push_back(float_vec); //Copying an offset should be fine - check performance!!!
  }

  SerializableVector<FloatVec> request_data = fbb.CreateVector(raw_request_data);
  return request_data;
}

const SerializableVector<DoubleVec> BatchPredictionRequest::get_serializable_doubles(
    flatbuffers::FlatBufferBuilder &fbb) const {
  std::vector<flatbuffers::Offset<DoubleVec>> raw_request_data;
  std::vector<double> buffer;

  for (int i = 0; i < (int) double_inputs_.size(); i++) {
    const DoubleVector *double_input = dynamic_cast<const DoubleVector *>(double_inputs_[i].get());
    size_t input_size = double_input->size();
    if (input_size > buffer.size()) {
      buffer.resize(input_size);
    }
    double *raw_buffer = buffer.data();
    auto raw_vec = fbb.CreateUninitializedVector(input_size, &raw_buffer);
    memcpy(raw_buffer, double_input->get_serializable_data(), input_size * sizeof(double));
    flatbuffers::Offset<DoubleVec> double_vec = CreateDoubleVec(fbb, raw_vec);
    raw_request_data.push_back(double_vec); //Copying an offset should be fine - check performance!!!
  }

  SerializableVector<DoubleVec> request_data = fbb.CreateVector(raw_request_data);
  return request_data;
}

const SerializableVector<ByteVec> BatchPredictionRequest::get_serializable_bytes(
    flatbuffers::FlatBufferBuilder &fbb) const {
  std::vector<flatbuffers::Offset<ByteVec>> raw_request_data;
  std::vector<uint8_t> buffer;

  for (int i = 0; i < (int) byte_inputs_.size(); i++) {
    const ByteVector *byte_input = dynamic_cast<const ByteVector *>(byte_inputs_[i].get());
    size_t input_size = byte_input->size();
    if (input_size > buffer.size()) {
      buffer.resize(input_size);
    }
    uint8_t *raw_buffer = buffer.data();
    auto raw_vec = fbb.CreateUninitializedVector(input_size, &raw_buffer);
    memcpy(raw_buffer, byte_input->get_serializable_data(), input_size * sizeof(uint8_t));
    flatbuffers::Offset<ByteVec> byte_vec = CreateByteVec(fbb, raw_vec);
    raw_request_data.push_back(byte_vec); //Copying an offset should be fine - check performance!!!
  }

  SerializableVector<ByteVec> request_data = fbb.CreateVector(raw_request_data);
  return request_data;
}

const SerializableVector<StringVec> BatchPredictionRequest::get_serializable_strings(
    flatbuffers::FlatBufferBuilder &fbb) const {
  std::vector<flatbuffers::Offset<StringVec>> raw_request_data;

  for (int i = 0; i < (int) string_inputs_.size(); i++) {
    const StringVector *string_input = dynamic_cast<const StringVector *>(string_inputs_[i].get());
    const std::string *strs = string_input->get_serializable_data();
    std::vector<flatbuffers::Offset<flatbuffers::String>> raw_vec;
    for(int j = 0; j < (int) string_input->size(); j++) {
      flatbuffers::Offset<flatbuffers::String> serializable_string = fbb.CreateString(strs[j]);
      raw_vec.push_back(serializable_string);
    }
    flatbuffers::Offset<StringVec> string_vec = CreateStringVecDirect(fbb, &raw_vec);
    raw_request_data.push_back(string_vec);
  }

  SerializableVector<StringVec> request_data = fbb.CreateVector(raw_request_data);
  return request_data;
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
