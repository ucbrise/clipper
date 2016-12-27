
#include <chrono>
#include <cstring>
#include <iostream>
#include <vector>

#include <clipper/datatypes.hpp>

namespace clipper {

size_t versioned_model_hash(const VersionedModelId &key) {
  return std::hash<std::string>()(key.first) ^ std::hash<int>()(key.second);
}

template <typename T>
ByteBuffer get_byte_buffer(std::vector<T> vector) {
  uint8_t *data = reinterpret_cast<uint8_t *>(vector.data());
  ByteBuffer bytes(data, data + vector.size() * (sizeof(T) / sizeof(uint8_t)));
  return bytes;
}

template <typename T>
size_t serialize_to_buffer(const std::vector<T> &vector, uint8_t *buf) {
  const uint8_t *byte_data = reinterpret_cast<const uint8_t *>(vector.data());
  size_t amt_to_write = vector.size() * (sizeof(T) / sizeof(uint8_t));
  memcpy(buf, byte_data, amt_to_write);
  return amt_to_write;
}

std::string get_readable_input_type(InputType type) {
  switch (type) {
    case InputType::Bytes:
      return std::string("bytes");
    case InputType::Ints:
      return std::string("integers");
    case InputType::Floats:
      return std::string("floats");
    case InputType::Doubles:
      return std::string("doubles");
    case InputType::Strings:
      return std::string("strings");
  }
  return std::string("Invalid input type");
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

InputType ByteVector::type() const { return InputType::Bytes; }

size_t ByteVector::serialize(uint8_t *buf) const {
  return serialize_to_buffer(data_, buf);
}

size_t ByteVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<uint8_t>()(d);
  }
  return cur_hash;
}

size_t ByteVector::size() const { return data_.size(); }

size_t ByteVector::byte_size() const { return data_.size() * sizeof(uint8_t); }

const std::vector<uint8_t> &ByteVector::get_data() const { return data_; }

IntVector::IntVector(std::vector<int> data) : data_(std::move(data)) {}

InputType IntVector::type() const { return InputType::Ints; }

size_t IntVector::serialize(uint8_t *buf) const {
  return serialize_to_buffer(data_, buf);
}

size_t IntVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<int>()(d);
  }
  return cur_hash;
}

size_t IntVector::size() const { return data_.size(); }

size_t IntVector::byte_size() const { return data_.size() * sizeof(int); }

const std::vector<int> &IntVector::get_data() const { return data_; }

FloatVector::FloatVector(std::vector<float> data) : data_(std::move(data)) {}

size_t FloatVector::serialize(uint8_t *buf) const {
  return serialize_to_buffer(data_, buf);
}

InputType FloatVector::type() const { return InputType::Floats; }

size_t FloatVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<float>()(d);
  }
  return cur_hash;
}

size_t FloatVector::size() const { return data_.size(); }

size_t FloatVector::byte_size() const { return data_.size() * sizeof(float); }

const std::vector<float> &FloatVector::get_data() const { return data_; }

DoubleVector::DoubleVector(std::vector<double> data) : data_(std::move(data)) {}

InputType DoubleVector::type() const { return InputType::Doubles; }

size_t DoubleVector::serialize(uint8_t *buf) const {
  return serialize_to_buffer(data_, buf);
}

size_t DoubleVector::hash() const {
  size_t cur_hash = 0;
  for (const auto d : data_) {
    cur_hash ^= std::hash<double>()(d);
  }
  return cur_hash;
}

size_t DoubleVector::size() const { return data_.size(); }

size_t DoubleVector::byte_size() const { return data_.size() * sizeof(double); }

const std::vector<double> &DoubleVector::get_data() const { return data_; }

SerializableString::SerializableString(std::string data)
    : data_(std::move(data)) {}

InputType SerializableString::type() const { return InputType::Strings; }

size_t SerializableString::serialize(uint8_t *buf) const {
  size_t amt_to_write = data_.length() + 1;
  memcpy(buf, data_.c_str(), amt_to_write);
  return amt_to_write;
}

size_t SerializableString::hash() const {
  return std::hash<std::string>()(data_);
}

size_t SerializableString::size() const { return 1; }

size_t SerializableString::byte_size() const {
  // The length of the string with an extra byte for the null terminator
  return data_.length() + 1;
}

const std::string &SerializableString::get_data() const { return data_; }

rpc::PredictionRequest::PredictionRequest(InputType input_type)
    : input_type_(input_type) {}

rpc::PredictionRequest::PredictionRequest(
    std::vector<std::shared_ptr<Input>> inputs, InputType input_type)
    : inputs_(inputs), input_type_(input_type) {
  for (int i = 0; i < (int)inputs.size(); i++) {
    if (inputs[i]->type() != input_type) {
      std::cout << "Attempted to add an input of type "
                << get_readable_input_type(inputs[i]->type())
                << "to a prediction request with input type "
                << get_readable_input_type(input_type) << std::endl;
      throw std::invalid_argument("");
    }
    input_data_size_ += inputs[i]->byte_size();
  }
}

void rpc::PredictionRequest::add_input(std::shared_ptr<Input> input) {
  inputs_.push_back(input);
  input_data_size_ += input->byte_size();
}

std::vector<ByteBuffer> rpc::PredictionRequest::serialize() {
  if (input_data_size_ <= 0) {
    throw std::length_error(
        "Attempted to serialize a request with no input data!");
  }

  long start = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();

  std::vector<uint32_t> request_metadata;
  request_metadata.emplace_back(
      static_cast<uint32_t>(RequestType::PredictRequest));

  std::vector<uint32_t> input_metadata;
  input_metadata.emplace_back(static_cast<uint32_t>(input_type_));
  input_metadata.emplace_back(static_cast<uint32_t>(inputs_.size()));

  uint32_t index = 0;
  uint8_t *input_buf = (uint8_t *)malloc(input_data_size_);
  uint8_t *input_buf_start = input_buf;

  for (int i = 0; i < (int)inputs_.size(); i++) {
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
  ByteBuffer serialized_inputs =
      ByteBuffer(input_buf_start, input_buf_start + input_data_size_);
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
