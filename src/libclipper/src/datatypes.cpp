
#include <iostream>
#include <string>
#include <vector>

#include <clipper/datatypes.hpp>

namespace clipper {

size_t versioned_model_hash(const VersionedModelId& key) {
  return std::hash<std::string>()(key.first) ^ std::hash<int>()(key.second);
}

template <typename T>
ByteBuffer get_byte_buffer(std::vector<T> vector) {
  uint8_t* data = reinterpret_cast<uint8_t*>(vector.data());
  ByteBuffer bytes(data, data + vector.size() * (sizeof(T) / sizeof(uint8_t)));
  return bytes;
}

template <typename T>
size_t serialize_to_buffer(const std::vector<T> &vector, uint8_t* buf) {
  const uint8_t* byte_data = reinterpret_cast<const uint8_t*>(vector.data());
  size_t amt_to_write = vector.size() * (sizeof(T) / sizeof(uint8_t));
  memcpy(buf, byte_data, amt_to_write);
  return amt_to_write;
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

ByteVector::ByteVector(std::vector<uint8_t> data) : data_(std::move(data)) {}

size_t ByteVector::serialize(uint8_t* buf) const {
  return serialize_to_buffer(data_, buf);
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

size_t ByteVector::byte_size() const {
  return data_.size() * sizeof(uint8_t);
}

IntVector::IntVector(std::vector<int> data) : data_(std::move(data)) {}

size_t IntVector::serialize(uint8_t* buf) const {
  return serialize_to_buffer(data_, buf);
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

size_t IntVector::byte_size() const {
  return data_.size() * sizeof(int);
}

FloatVector::FloatVector(std::vector<float> data) : data_(std::move(data)) {}

size_t FloatVector::serialize(uint8_t* buf) const {
  return serialize_to_buffer(data_, buf);
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

size_t FloatVector::byte_size() const {
  return data_.size() * sizeof(float);
}

DoubleVector::DoubleVector(std::vector<double> data) : data_(std::move(data)) {}

size_t DoubleVector::serialize(uint8_t* buf) const {
  return serialize_to_buffer(data_, buf);
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

size_t DoubleVector::byte_size() const {
  return data_.size() * sizeof(double);
}

StringVector::StringVector(std::vector<std::string> data) : data_(std::move(data)) {}

size_t StringVector::serialize(uint8_t* buf) const {
  size_t amt_written = 0;
  for(int i = 0; i < (int) data_.size(); i++) {
    size_t length = data_[i].length() + 1;
    memcpy(buf, data_[i].c_str(), length);
    buf += length;
    amt_written += length;
  }
  return amt_written;
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

size_t StringVector::byte_size() const {
  size_t size = 0;
  for(int i = 0; i < (int) data_.size(); i++) {
    size += data_[i].length() + 1;
  }
  return size;
}


PredictionRequest::PredictionRequest(InputType input_type) : input_type_(input_type) {

}

PredictionRequest::PredictionRequest(std::vector<std::shared_ptr<Input>> inputs, InputType input_type)
    : inputs_(inputs), input_type_(input_type) {
  for(int i = 0; i < (int) inputs.size(); i++) {
    input_data_size_ += inputs[i]->byte_size();
  }
}

void PredictionRequest::add_input(std::shared_ptr<Input> input) {
  inputs_.push_back(input);
  input_data_size_ += input->byte_size();
}

std::vector<ByteBuffer> PredictionRequest::serialize() {

  long start = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch())
      .count();

  std::vector<uint32_t> request_metadata;
  request_metadata.emplace_back(RequestType::PredictRequest);

  std::vector<uint32_t> input_metadata;
  input_metadata.emplace_back(input_type_);

  uint32_t index = 0;
  uint8_t* input_buf = (uint8_t*) malloc(input_data_size_);
  uint8_t* input_buf_start = input_buf;

  for(int i = 0; i < (int) inputs_.size(); i++) {
    size_t amt_written = inputs_[i]->serialize(input_buf);
    input_buf += amt_written;
    index += inputs_[i]->size();
    input_metadata.push_back(index);
  }
  // Remove the final separation index because it results in the
  // creation of an empty data array when deserializing
  input_metadata.pop_back();

  std::vector<ByteBuffer> serialized_request;
  ByteBuffer serialized_input_metadata = get_byte_buffer(input_metadata);
  ByteBuffer serialized_request_metadata = get_byte_buffer(request_metadata);
  ByteBuffer serialized_inputs = ByteBuffer(input_buf_start, input_buf_start + input_data_size_);
  free(input_buf_start);
  serialized_request.push_back(serialized_request_metadata);
  serialized_request.push_back(serialized_input_metadata);
  serialized_request.push_back(serialized_inputs);

  long stop = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch())
      .count();

  std::cout << stop - start << std::endl;

  return serialized_request;
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
