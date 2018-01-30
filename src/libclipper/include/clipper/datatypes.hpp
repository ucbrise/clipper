#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>
#include <boost/functional/hash.hpp>

namespace clipper {

template <typename T>
using UniquePoolPtr = std::unique_ptr<T, void(*)(void*)>;
template <typename T>
using SharedPoolPtr = std::shared_ptr<T>;

typedef uint64_t PredictionDataHash;

using QueryId = long;
using FeedbackAck = bool;

enum class DataType {
  Invalid = -1,
  Bytes = 0,
  Ints = 1,
  Floats = 2,
  Doubles = 3,
  Strings = 4,
};

typedef DataType InputType;
typedef DataType OutputType;

enum class RequestType {
  PredictRequest = 0,
  FeedbackRequest = 1,
};

std::string get_readable_input_type(DataType type);
DataType parse_input_type(std::string type_string);

template <class T>
class ByteBufferPtr {
 public:
  explicit ByteBufferPtr(UniquePoolPtr<T> unique_ptr) :
      unique_ptr_(std::move(unique_ptr)), shared_ptr_(NULL, free) {}
  explicit ByteBufferPtr(SharedPoolPtr<T> shared_ptr) :
      unique_ptr_(NULL, free), shared_ptr_(std::move(shared_ptr)) {};

  bool unique() const {
    if(unique_ptr_) {
      return true;
    } else {
      return false;
    }
  }

  friend T* get_raw(ByteBufferPtr<T> ptr) {
    return ptr.get_raw();
  }

  friend SharedPoolPtr<T> get_shared(ByteBufferPtr<T> ptr) {
    return ptr.get_shared();
  }

 private:
  T* get_raw() {
    if(unique_ptr_) {
      return unique_ptr_.release();
    } else {
      return shared_ptr_.get();
    }
  }

  SharedPoolPtr<T> get_shared() {
    if (unique_ptr_) {
      return SharedPoolPtr<T>(unique_ptr_.release(), unique_ptr_.get_deleter());
    } else {
      return shared_ptr_;
    }
  }

  UniquePoolPtr<T> unique_ptr_;
  SharedPoolPtr<T> shared_ptr_;
};

// Pair of input data, data start index, data size in bytes
typedef std::tuple<ByteBufferPtr<void>, size_t, size_t> ByteBuffer;

class VersionedModelId {
 public:
  VersionedModelId(const std::string name, const std::string id);

  std::string get_name() const;
  std::string get_id() const;
  std::string serialize() const;
  static VersionedModelId deserialize(std::string);

  VersionedModelId(const VersionedModelId &) = default;
  VersionedModelId &operator=(const VersionedModelId &) = default;

  VersionedModelId(VersionedModelId &&) = default;
  VersionedModelId &operator=(VersionedModelId &&) = default;

  bool operator==(const VersionedModelId &rhs) const;
  bool operator!=(const VersionedModelId &rhs) const;

 private:
  std::string name_;
  std::string id_;
};

class PredictionData {
 public:
  // TODO: pure virtual or default?
  // virtual ~PredictionData() = default;

  virtual DataType type() const = 0;

  virtual PredictionDataHash hash() = 0;

  /**
   * The index marking the beginning of the input data
   * content, relative to the input's data pointer
   */
  virtual size_t start() const = 0;

  /**
   * The byte index marking the beginning of the input data
   * content, relative to a byte representation of the
   * input's data pointer
   */
  virtual size_t start_byte() const = 0;

  /**
   * @return The number of elements in the input
   */
  virtual size_t size() const = 0;
  /**
   * @return The size of the input data in bytes
   */
  virtual size_t byte_size() const = 0;

  friend SharedPoolPtr<void> get_data(const std::shared_ptr<PredictionData>& data_item) {
    return data_item->get_data();
  }

  friend UniquePoolPtr<void> get_data(std::unique_ptr<PredictionData> data_item) {
    void* raw_data = data_item->get_data().get();
    return UniquePoolPtr<void>(raw_data, free);
  }

 private:
  virtual SharedPoolPtr<void> get_data() const = 0;
};

class ByteVector : public PredictionData {
 public:
  explicit ByteVector(UniquePoolPtr<uint8_t> data, size_t size);
  explicit ByteVector(SharedPoolPtr<uint8_t> data, size_t start, size_t size);
  explicit ByteVector(UniquePoolPtr<void> data, size_t byte_size);
  explicit ByteVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size);
  explicit ByteVector(void* data, size_t byte_size);

  DataType type() const override;
  PredictionDataHash hash() override;
  size_t start() const override;
  size_t start_byte() const override;
  size_t size() const override;
  size_t byte_size() const override;

 private:
  SharedPoolPtr<void> get_data() const override;

  SharedPoolPtr<uint8_t> data_;
  size_t start_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class IntVector : public PredictionData {
 public:
  explicit IntVector(UniquePoolPtr<int> data, size_t size);
  explicit IntVector(SharedPoolPtr<int> data, size_t start, size_t size);
  explicit IntVector(UniquePoolPtr<void> data, size_t byte_size);
  explicit IntVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size);
  explicit IntVector(void* data, size_t byte_size);

  DataType type() const override;
  PredictionDataHash hash() override;
  size_t start() const override;
  size_t start_byte() const override;
  size_t size() const override;
  size_t byte_size() const override;

 private:
  SharedPoolPtr<void> get_data() const override;

  SharedPoolPtr<int> data_;
  size_t start_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class FloatVector : public PredictionData {
 public:
  explicit FloatVector(UniquePoolPtr<float> data, size_t size);
  explicit FloatVector(SharedPoolPtr<float> data, size_t start, size_t size);
  explicit FloatVector(UniquePoolPtr<void> data, size_t byte_size);
  explicit FloatVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size);
  explicit FloatVector(void* data, size_t byte_size);

  DataType type() const override;
  PredictionDataHash hash() override;
  size_t start() const override;
  size_t start_byte() const override;
  size_t size() const override;
  size_t byte_size() const override;

 private:
  SharedPoolPtr<void> get_data() const override;

  SharedPoolPtr<float> data_;
  size_t start_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class DoubleVector : public PredictionData {
 public:
  explicit DoubleVector(UniquePoolPtr<double> data, size_t size);
  explicit DoubleVector(SharedPoolPtr<double> data, size_t start, size_t size);
  explicit DoubleVector(UniquePoolPtr<void> data, size_t byte_size);
  explicit DoubleVector(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size);
  explicit DoubleVector(void* data, size_t byte_size);

  DataType type() const override;
  PredictionDataHash hash() override;
  size_t start() const override ;
  size_t start_byte() const override;
  size_t size() const override;
  size_t byte_size() const override;

 private:
  SharedPoolPtr<void> get_data() const override;

  SharedPoolPtr<double> data_;
  size_t start_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class SerializableString : public PredictionData {
 public:
  explicit SerializableString(UniquePoolPtr<char> data, size_t size);
  explicit SerializableString(SharedPoolPtr<char> data, size_t start, size_t size);
  explicit SerializableString(UniquePoolPtr<void> data, size_t byte_size);
  explicit SerializableString(SharedPoolPtr<void> data, size_t start_byte, size_t byte_size);
  explicit SerializableString(void* data, size_t byte_size);
  explicit SerializableString(std::string data);

  DataType type() const override;
  PredictionDataHash hash() override;
  size_t start() const override;
  size_t start_byte() const override;
  size_t size() const override;
  size_t byte_size() const override;

 private:
  SharedPoolPtr<void> get_data() const override;

  SharedPoolPtr<char> data_;
  size_t start_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, std::shared_ptr<PredictionData> input,
        long latency_budget_micros, std::string selection_policy,
        std::vector<VersionedModelId> candidate_models);

  // Note that it should be relatively cheap to copy queries because
  // the actual input won't be copied
  // copy constructors
  Query(const Query &) = default;
  Query &operator=(const Query &) = default;

  // move constructors
  Query(Query &&) = default;
  Query &operator=(Query &&) = default;

  // Used to provide a namespace for queries. The expected
  // use is to distinguish queries coming from different
  // REST endpoints.
  std::string label_;
  long user_id_;
  std::shared_ptr<PredictionData> input_;
  // TODO change this to a deadline instead of a duration
  long latency_budget_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
  std::chrono::time_point<std::chrono::high_resolution_clock> create_time_;
};

class Output {
 public:
  Output(const std::shared_ptr<PredictionData> y_hat,
         const std::vector<VersionedModelId> models_used);

  Output(const std::string y_hat,
         const std::vector<VersionedModelId> models_used);

  ~Output() = default;

  explicit Output() = default;
  Output(const Output &) = default;
  Output &operator=(const Output &) = default;

  Output(Output &&) = default;
  Output &operator=(Output &&) = default;

  bool operator==(const Output &rhs) const;
  bool operator!=(const Output &rhs) const;

  std::shared_ptr<PredictionData> y_hat_;
  std::vector<VersionedModelId> models_used_;
};

class Response {
 public:
  ~Response() = default;

  Response(Query query, QueryId query_id, const long duration_micros,
           Output output, const bool is_default,
           const boost::optional<std::string> default_explanation);

  // default copy constructors
  Response(const Response &) = default;
  Response &operator=(const Response &) = default;

  // default move constructors
  Response(Response &&) = default;
  Response &operator=(Response &&) = default;

  std::string debug_string() const noexcept;

  Query query_;
  QueryId query_id_;
  long duration_micros_;
  Output output_;
  bool output_is_default_;
  boost::optional<std::string> default_explanation_;
};

class Feedback {
 public:
  ~Feedback() = default;
  Feedback(std::shared_ptr<PredictionData> input, double y);

  Feedback(const Feedback &) = default;
  Feedback &operator=(const Feedback &) = default;

  Feedback(Feedback &&) = default;
  Feedback &operator=(Feedback &&) = default;

  double y_;
  std::shared_ptr<PredictionData> input_;
};

class FeedbackQuery {
 public:
  ~FeedbackQuery() = default;
  FeedbackQuery(std::string label, long user_id, Feedback feedback,
                std::string selection_policy,
                std::vector<VersionedModelId> candidate_models);

  FeedbackQuery(const FeedbackQuery &) = default;
  FeedbackQuery &operator=(const FeedbackQuery &) = default;

  FeedbackQuery(FeedbackQuery &&) = default;
  FeedbackQuery &operator=(FeedbackQuery &&) = default;

  // Used to provide a namespace for queries. The expected
  // use is to distinguish queries coming from different
  // REST endpoints.
  std::string label_;
  long user_id_;
  Feedback feedback_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
};

class PredictTask {
 public:
  ~PredictTask() = default;

  PredictTask(std::shared_ptr<PredictionData> input, VersionedModelId model,
              float utility, QueryId query_id, long latency_slo_micros);

  PredictTask(const PredictTask &other) = default;

  PredictTask &operator=(const PredictTask &other) = default;

  PredictTask(PredictTask &&other) = default;

  PredictTask &operator=(PredictTask &&other) = default;

  std::shared_ptr<PredictionData> input_;
  VersionedModelId model_;
  float utility_;
  QueryId query_id_;
  long latency_slo_micros_;
  std::chrono::time_point<std::chrono::system_clock> recv_time_;
};

/// NOTE: If a feedback task is scheduled, the task scheduler
/// must send it to ALL replicas of the VersionedModelId.
class FeedbackTask {
 public:
  ~FeedbackTask() = default;

  FeedbackTask(Feedback feedback, VersionedModelId model, QueryId query_id,
               long latency_slo_micros);

  FeedbackTask(const FeedbackTask &other) = default;

  FeedbackTask &operator=(const FeedbackTask &other) = default;

  FeedbackTask(FeedbackTask &&other) = default;

  FeedbackTask &operator=(FeedbackTask &&other) = default;

  Feedback feedback_;
  VersionedModelId model_;
  QueryId query_id_;
  long latency_slo_micros_;
};

namespace rpc {

class PredictionRequest {
 public:
  explicit PredictionRequest(DataType input_type);
  explicit PredictionRequest(std::vector<std::shared_ptr<PredictionData>>& inputs, DataType input_type);
  explicit PredictionRequest(std::vector<std::unique_ptr<PredictionData>> inputs, DataType input_type);

  // Disallow copy
  PredictionRequest(PredictionRequest &other) = delete;
  PredictionRequest &operator=(PredictionRequest &other) = delete;

  // move constructors
  PredictionRequest(PredictionRequest &&other) = default;
  PredictionRequest &operator=(PredictionRequest &&other) = default;

  void add_input(const std::shared_ptr<PredictionData>& input);
  void add_input(std::unique_ptr<PredictionData> input);
  std::vector<ByteBuffer> serialize();

 private:
  void validate_input_type(InputType input_type) const;

  std::vector<ByteBuffer> input_data_items_;
  DataType input_type_;
  size_t input_data_size_ = 0;
};

class PredictionResponse {
 public:
  PredictionResponse(const std::vector<std::shared_ptr<PredictionData>> outputs);

  // Disallow copy
  PredictionResponse(PredictionResponse &other) = delete;
  PredictionResponse &operator=(PredictionResponse &other) = delete;

  // move constructors
  PredictionResponse(PredictionResponse &&other) = default;
  PredictionResponse &operator=(PredictionResponse &&other) = default;

  static PredictionResponse deserialize_prediction_response(std::vector<ByteBuffer> response);

  std::vector<std::shared_ptr<PredictionData>> outputs_;
};

}  // namespace rpc

}  // namespace clipper
namespace std {
template <>
struct hash<clipper::VersionedModelId> {
  typedef std::size_t result_type;
  std::size_t operator()(const clipper::VersionedModelId &vm) const {
    std::size_t seed = 0;
    boost::hash_combine(seed, vm.get_name());
    boost::hash_combine(seed, vm.get_id());
    return seed;
  }
};
}  // namespace std
#endif  // CLIPPER_LIB_DATATYPES_H
