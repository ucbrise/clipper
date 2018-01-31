#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#include <city.h>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/util.hpp>

namespace clipper {

std::string get_readable_input_type(DataType type) {
  switch (type) {
    case DataType::Bytes: return std::string("bytes");
    case DataType::Ints: return std::string("integers");
    case DataType::Floats: return std::string("floats");
    case DataType::Doubles: return std::string("doubles");
    case DataType::Strings: return std::string("strings");
    case DataType::Invalid:
    default: return std::string("Invalid input type");
  }
}

DataType parse_input_type(std::string type_string) {
  if (type_string == "bytes" || type_string == "byte" || type_string == "b") {
    return DataType::Bytes;
  } else if (type_string == "integers" || type_string == "ints" ||
             type_string == "integer" || type_string == "int" ||
             type_string == "i") {
    return DataType::Ints;
  } else if (type_string == "floats" || type_string == "float" ||
             type_string == "f") {
    return DataType::Floats;
  } else if (type_string == "doubles" || type_string == "double" ||
             type_string == "d") {
    return DataType::Doubles;
  } else if (type_string == "strings" || type_string == "string" ||
             type_string == "str" || type_string == "strs" ||
             type_string == "s") {
    return DataType::Strings;
  } else {
    throw std::invalid_argument(type_string + " is not a valid input string");
  }
}

VersionedModelId::VersionedModelId(const std::string name, const std::string id)
    : name_(name), id_(id) {}

std::string VersionedModelId::get_name() const { return name_; }

std::string VersionedModelId::get_id() const { return id_; }

std::string VersionedModelId::serialize() const {
  std::stringstream ss;
  ss << name_;
  ss << ITEM_PART_CONCATENATOR;
  ss << id_;
  return ss.str();
}

VersionedModelId VersionedModelId::deserialize(std::string str) {
  auto split = str.find(ITEM_PART_CONCATENATOR);
  std::string model_name = str.substr(0, split);
  std::string model_version = str.substr(split + 1, str.size());
  return VersionedModelId(model_name, model_version);
}

bool VersionedModelId::operator==(const VersionedModelId &rhs) const {
  return (name_ == rhs.name_ && id_ == rhs.id_);
}

bool VersionedModelId::operator!=(const VersionedModelId &rhs) const {
  return !(name_ == rhs.name_ && id_ == rhs.id_);
}

Output::Output(const std::shared_ptr<PredictionData> y_hat,
               const std::vector<VersionedModelId> models_used)
    : y_hat_(y_hat), models_used_(models_used) {}


Output::Output(const std::string y_hat, const std::vector<VersionedModelId> models_used) {
  y_hat_ = std::make_shared<SerializableString>(y_hat);
  models_used_ = models_used;
}

bool Output::operator==(const Output &rhs) const {
  return (y_hat_ == rhs.y_hat_ && models_used_ == rhs.models_used_);
}

bool Output::operator!=(const Output &rhs) const {
  return !(y_hat_ == rhs.y_hat_ && models_used_ == rhs.models_used_);
}

ByteVector::ByteVector(UniquePoolPtr<uint8_t> data, size_t size)
    : data_(std::move(data)), start_(0), size_(size) {}

ByteVector::ByteVector(SharedPoolPtr<uint8_t> data, size_t start, size_t size)
    : data_(std::move(data)), start_(start), size_(size) {}

ByteVector::ByteVector(UniquePoolPtr<void> data, size_t byte_size)
    : ByteVector(data.release(), byte_size) {}

ByteVector::ByteVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size)
    : data_(std::static_pointer_cast<uint8_t>(std::move(data))),
      start_(start_byte / sizeof(uint8_t)),
      size_(byte_size / sizeof(uint8_t)) {}

ByteVector::ByteVector(void* data, size_t byte_size)
    : data_(SharedPoolPtr<uint8_t>(static_cast<uint8_t*>(data), free)),
      start_(0),
      size_(byte_size / sizeof(uint8_t)) {}

DataType ByteVector::type() const { return DataType::Bytes; }

PredictionDataHash ByteVector::hash() {
  if (!hash_) {
    hash_ = CityHash64(reinterpret_cast<char *>(data_.get() + start_), size_ * sizeof(uint8_t));
  }
  return hash_.get();
}

size_t ByteVector::start() const { return start_; }

size_t ByteVector::start_byte() const { return start_ * sizeof(uint8_t); }

size_t ByteVector::size() const { return size_; }

size_t ByteVector::byte_size() const { return size_; }

SharedPoolPtr<void> ByteVector::get_data() const {
  return data_;
}

IntVector::IntVector(UniquePoolPtr<int> data, size_t size)
    : data_(std::move(data)), start_(0), size_(size) {}

IntVector::IntVector(SharedPoolPtr<int> data, size_t start, size_t size)
    : data_(std::move(data)), start_(start), size_(size) {}

IntVector::IntVector(UniquePoolPtr<void> data, size_t byte_size)
    : IntVector(data.release(), byte_size) {}

IntVector::IntVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size)
    : data_(std::static_pointer_cast<int>(std::move(data))),
      start_(start_byte / sizeof(int)),
      size_(byte_size / sizeof(int)) {}

IntVector::IntVector(void* data, size_t byte_size)
    : data_(SharedPoolPtr<int>(static_cast<int*>(data), free)),
      start_(0),
      size_(byte_size / sizeof(int)) {}

DataType IntVector::type() const { return DataType::Ints; }

PredictionDataHash IntVector::hash() {
  if (!hash_) {
    hash_ = CityHash64(reinterpret_cast<char *>(data_.get() + start_), size_ * sizeof(int));
  }
  return hash_.get();
}

size_t IntVector::start() const { return start_; }

size_t IntVector::start_byte() const { return start_ * sizeof(int); }

size_t IntVector::size() const { return size_; }

size_t IntVector::byte_size() const { return size_ * sizeof(int); }

SharedPoolPtr<void> IntVector::get_data() const {
  return data_;
}

FloatVector::FloatVector(UniquePoolPtr<float> data, size_t size)
    : data_(std::move(data)), start_(0), size_(size) {}

FloatVector::FloatVector(SharedPoolPtr<float> data, size_t start, size_t size)
    : data_(std::move(data)), start_(start), size_(size) {}

FloatVector::FloatVector(UniquePoolPtr<void> data, size_t byte_size)
    : FloatVector(data.release(), byte_size) {}

FloatVector::FloatVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size)
    : data_(std::static_pointer_cast<float>(std::move(data))),
      start_(start_byte / sizeof(float)),
      size_(byte_size / sizeof(float)) {}

FloatVector::FloatVector(void* data, size_t byte_size)
    : data_(SharedPoolPtr<float>(static_cast<float*>(data), free)),
      start_(0),
      size_(byte_size / sizeof(float)) {}

DataType FloatVector::type() const { return DataType::Floats; }

PredictionDataHash FloatVector::hash() {
  // TODO [CLIPPER-63]: Find an alternative to hashing floats directly, as this
  // is generally a bad idea due to loss of precision from floating point)
  // representations
  if (!hash_) {
    hash_ = CityHash64(reinterpret_cast<char *>(data_.get() + start_), size_ * sizeof(float));
  }
  return hash_.get();
}

size_t FloatVector::start() const { return start_; }

size_t FloatVector::start_byte() const { return start_ * sizeof(float); }

size_t FloatVector::size() const { return size_; }

size_t FloatVector::byte_size() const { return size_ * sizeof(float); }

SharedPoolPtr<void> FloatVector::get_data() const {
  return data_;
}

DoubleVector::DoubleVector(UniquePoolPtr<double> data, size_t size)
    : data_(std::move(data)), start_(0), size_(size) {}

DoubleVector::DoubleVector(SharedPoolPtr<double> data, size_t start, size_t size)
    : data_(std::move(data)), start_(start), size_(size) {}

DoubleVector::DoubleVector(UniquePoolPtr<void> data, size_t byte_size)
    : DoubleVector(data.release(), byte_size) {}

DoubleVector::DoubleVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size)
    : data_(std::static_pointer_cast<double>(std::move(data))),
      start_(start_byte / sizeof(double)),
      size_(byte_size / sizeof(double)) {}

DoubleVector::DoubleVector(void* data, size_t size_bytes)
    : data_(SharedPoolPtr<double>(static_cast<double*>(data), free)),
      start_(0),
      size_(static_cast<size_t>(size_bytes / sizeof(double))) {}

DataType DoubleVector::type() const { return DataType::Doubles; }

PredictionDataHash DoubleVector::hash() {
  // TODO [CLIPPER-63]: Find an alternative to hashing doubles directly, as
  // this is generally a bad idea due to loss of precision from floating point
  // representations
  if (!hash_) {
    hash_ = CityHash64(reinterpret_cast<char *>(data_.get() + start_) + start_, size_ * sizeof(double));
  }
  return hash_.get();
}

size_t DoubleVector::start() const { return start_; }

size_t DoubleVector::start_byte() const { return start_ * sizeof(double); }

size_t DoubleVector::size() const { return size_; }

size_t DoubleVector::byte_size() const { return size_ * sizeof(double); }

SharedPoolPtr<void> DoubleVector::get_data() const {
  return data_;
}

SerializableString::SerializableString(UniquePoolPtr<char> data, size_t size)
    : data_(std::move(data)), start_(0), size_(size) {}

SerializableString::SerializableString(SharedPoolPtr<char> data, size_t start, size_t size)
    : data_(std::move(data)), start_(start), size_(size) {}

SerializableString::SerializableString(UniquePoolPtr<void> data, size_t byte_size)
    : SerializableString(data.release(), byte_size) {}

SerializableString::SerializableString(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size)
    : data_(std::static_pointer_cast<char>(std::move(data))),
      start_(start_byte / sizeof(char)),
      size_(byte_size / sizeof(char)) {}

SerializableString::SerializableString(std::string data) : start_(0), size_(data.size()) {
  size_t byte_size = size_ * sizeof(char);
  data_ = SharedPoolPtr<char>(static_cast<char*>(malloc(byte_size)), free);
  memcpy(data_.get(), data.data(), byte_size);
}

SerializableString::SerializableString(void* data, size_t byte_size)
    : data_(SharedPoolPtr<char>(static_cast<char*>(data), free)),
      start_(0),
      size_(static_cast<size_t>(byte_size / sizeof(char))) {}

DataType SerializableString::type() const { return DataType::Strings; }

PredictionDataHash SerializableString::hash() {
  if (!hash_) {
    hash_ = CityHash64(data_.get() + start_, size_ * sizeof(char));
  }
  return hash_.get();
}

size_t SerializableString::start() const { return start_; }

size_t SerializableString::start_byte() const { return start_ * sizeof(char); }

size_t SerializableString::size() const { return size_; }

size_t SerializableString::byte_size() const {
  return size_ * sizeof(char);
}

SharedPoolPtr<void> SerializableString::get_data() const {
  return data_;
}

rpc::PredictionRequest::PredictionRequest(DataType input_type)
    : input_type_(input_type) {}

rpc::PredictionRequest::PredictionRequest(std::vector<std::unique_ptr<clipper::PredictionData>> inputs,
                                          DataType input_type)
    : input_type_(input_type) {
  for(auto &input : inputs) {
    add_input(std::move(input));
  }
}

rpc::PredictionRequest::PredictionRequest(std::vector<std::shared_ptr<clipper::PredictionData>> &inputs,
                                          DataType input_type)
    : input_type_(input_type) {
  for(auto &input : inputs) {
    add_input(input);
  }
}

void rpc::PredictionRequest::validate_input_type(InputType input_type) const {
  if (input_type != input_type_) {
    std::ostringstream ss;
    ss << "Attempted to add an input of type "
       << get_readable_input_type(input_type)
       << " to a prediction request with input type "
       << get_readable_input_type(input_type_);
    log_error(LOGGING_TAG_CLIPPER, ss.str());
    throw std::invalid_argument(ss.str());
  }
}

void rpc::PredictionRequest::add_input(const std::shared_ptr<PredictionData>& input) {
  validate_input_type(input->type());
  input_data_size_ += input->byte_size();
  SharedPoolPtr<void> input_data = get_data(input);
  ByteBufferPtr<void> buffer_data(input_data);
  input_data_items_.push_back(std::make_tuple(std::move(buffer_data), input->start_byte(), input->byte_size()));
}

void rpc::PredictionRequest::add_input(std::unique_ptr<PredictionData> input) {
  validate_input_type(input->type());
  size_t start_byte = input->start_byte();
  size_t byte_size = input->byte_size();
  input_data_size_ += byte_size;
  UniquePoolPtr<void> input_data = get_data(std::move(input));
  ByteBufferPtr<void> buffer_data(std::move(input_data));
  input_data_items_.push_back(std::make_tuple(std::move(buffer_data), start_byte, byte_size));
}

std::vector<ByteBuffer> rpc::PredictionRequest::serialize() {
  if (input_data_size_ <= 0) {
    throw std::length_error(
        "Attempted to serialize a request with no input data!");
  }

  size_t request_metadata_size = 1 * sizeof(uint32_t);
  UniquePoolPtr<void> request_metadata(malloc(request_metadata_size), free);
  uint32_t *request_metadata_raw = static_cast<uint32_t *>(request_metadata.get());
  request_metadata_raw[0] = static_cast<uint32_t>(RequestType::PredictRequest);

  size_t input_metadata_size = (2 + input_data_items_.size()) * sizeof(uint64_t);
  UniquePoolPtr<void> input_metadata(malloc(input_metadata_size), free);
  uint64_t *input_metadata_raw = static_cast<uint64_t *>(input_metadata.get());
  input_metadata_raw[0] = static_cast<uint64_t>(input_type_);
  input_metadata_raw[1] = static_cast<uint64_t>(input_data_items_.size());

  for (size_t i = 0; i < input_data_items_.size(); i++) {
    input_metadata_raw[i + 2] = std::get<1>(input_data_items_[i]);
  }

  uint64_t input_metadata_size_buf_size = 1 * sizeof(uint64_t);
  UniquePoolPtr<void> input_metadata_size_buf(malloc(input_metadata_size_buf_size), free);
  uint64_t *input_metadata_size_buf_raw =
      reinterpret_cast<uint64_t *>(input_metadata_size_buf.get());
  // Add the size of the input metadata in bytes. This will be
  // sent prior to the input metadata to allow for proactive
  // buffer allocation in the receiving container
  input_metadata_size_buf_raw[0] = input_metadata_size;

  std::vector<ByteBuffer> serialized_request;
  serialized_request.emplace_back(
      std::make_tuple(ByteBufferPtr<void>(std::move(request_metadata)), 0, request_metadata_size));
  serialized_request.emplace_back(
      std::make_tuple(ByteBufferPtr<void>(std::move(input_metadata_size_buf)), 0, input_metadata_size_buf_size));
  serialized_request.emplace_back(
      std::make_tuple(ByteBufferPtr<void>(std::move(input_metadata)), 0, input_metadata_size));
  for (size_t i = 0; i < input_data_items_.size(); i++) {
    serialized_request.emplace_back(std::move(input_data_items_[i]));
  }
  return serialized_request;
}

rpc::PredictionResponse::PredictionResponse(const std::vector<std::shared_ptr<PredictionData>> outputs)
    : outputs_(std::move(outputs)) {}

rpc::PredictionResponse
rpc::PredictionResponse::deserialize_prediction_response(std::vector<ByteBuffer> response) {
  std::vector<std::shared_ptr<PredictionData>> outputs;
  for(auto &output : response) {
    SharedPoolPtr<void> output_data = get_shared(std::move(std::get<0>(output)));
    size_t output_start = std::get<1>(output);
    size_t output_size = std::get<2>(output);
    char* output_str_data = static_cast<char*>(output_data.get());
    std::string output_str(output_str_data + output_start, output_str_data + output_start + output_size);
    std::shared_ptr<PredictionData> parsed_output =
        std::make_shared<SerializableString>(output_data, output_start, output_size);
    outputs.push_back(std::move(parsed_output));
  }
  return PredictionResponse(outputs);
}

Query::Query(std::string label, long user_id, std::shared_ptr<PredictionData> input,
             long latency_budget_micros, std::string selection_policy,
             std::vector<VersionedModelId> candidate_models)
    : label_(std::move(label)),
      user_id_(user_id),
      input_(std::move(input)),
      latency_budget_micros_(latency_budget_micros),
      selection_policy_(std::move(selection_policy)),
      candidate_models_(std::move(candidate_models)),
      create_time_(std::chrono::high_resolution_clock::now()) {}

Response::Response(Query query, QueryId query_id, const long duration_micros,
                   Output output, const bool output_is_default,
                   const boost::optional<std::string> default_explanation)
    : query_(std::move(query)),
      query_id_(query_id),
      duration_micros_(duration_micros),
      output_(std::move(output)),
      output_is_default_(output_is_default),
      default_explanation_(default_explanation) {}

std::string Response::debug_string() const noexcept {
  std::string debug;
  debug.append("Query id: ");
  debug.append(std::to_string(query_id_));
  //debug.append(" Output: ");
  //debug.append(output_.y_hat_);
  return debug;
}

Feedback::Feedback(std::shared_ptr<PredictionData> input, double y)
    : y_(y), input_(std::move(input)) {}

FeedbackQuery::FeedbackQuery(std::string label, long user_id, Feedback feedback,
                             std::string selection_policy,
                             std::vector<VersionedModelId> candidate_models)
    : label_(std::move(label)),
      user_id_(user_id),
      feedback_(std::move(feedback)),
      selection_policy_(std::move(selection_policy)),
      candidate_models_(std::move(candidate_models)) {}

PredictTask::PredictTask(std::shared_ptr<PredictionData> input, VersionedModelId model,
                         float utility, QueryId query_id,
                         long latency_slo_micros)
    : input_(std::move(input)),
      model_(std::move(model)),
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
