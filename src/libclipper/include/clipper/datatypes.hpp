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

typedef std::pair<SharedPoolPtr<void>, size_t> ByteBuffer;
typedef uint32_t PredictionDataHash;

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
   * @return The number of elements in the input
   */
  virtual size_t size() const = 0;
  /**
   * @return The size of the input data in bytes
   */
  virtual size_t byte_size() const = 0;

  virtual SharedPoolPtr<void> get_data() const = 0;
};

template <class ...Args>
SharedPoolPtr<PredictionData> create_prediction_data(Args&& ...args);

class ByteVector : public PredictionData {
 public:
  DataType type() const override;
  PredictionDataHash hash() override;
  size_t size() const override;
  size_t byte_size() const override;
  SharedPoolPtr<void> get_data() const override;

 private:
  explicit ByteVector(UniquePoolPtr<void> data, size_t byte_size);
  explicit ByteVector(UniquePoolPtr<uint8_t> data, size_t size);

  template <class ...Args>
  friend SharedPoolPtr<ByteVector> create_prediction_data(Args&& ...args)
  {
    return SharedPoolPtr<ByteVector>(new ByteVector(std::forward<Args>(args)...));
  }

  SharedPoolPtr<uint8_t> data_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class IntVector : public PredictionData {
 public:
  DataType type() const override;
  PredictionDataHash hash() override;
  size_t size() const override;
  size_t byte_size() const override;
  SharedPoolPtr<void> get_data() const override;

 private:
  explicit IntVector(UniquePoolPtr<int> data, size_t size);
  explicit IntVector(UniquePoolPtr<void> data, size_t byte_size);

  template <class ...Args>
  friend SharedPoolPtr<IntVector> create_prediction_data(Args&& ...args)
  {
    return SharedPoolPtr<IntVector>(new IntVector(std::forward<Args>(args)...));
  }

  SharedPoolPtr<int> data_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class FloatVector : public PredictionData {
 public:
  DataType type() const override;
  PredictionDataHash hash() override;
  size_t size() const override;
  size_t byte_size() const override;
  SharedPoolPtr<void> get_data() const override;

 private:
  explicit FloatVector(UniquePoolPtr<float> data, size_t size);
  explicit FloatVector(UniquePoolPtr<void> data, size_t byte_size);

  template <class ...Args>
  friend SharedPoolPtr<FloatVector> create_prediction_data(Args&& ...args)
  {
    return SharedPoolPtr<FloatVector>(new FloatVector(std::forward<Args>(args)...));
  }

  SharedPoolPtr<float> data_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class DoubleVector : public PredictionData {
 public:
  DataType type() const override;
  PredictionDataHash hash() override;
  size_t size() const override;
  size_t byte_size() const override;
  SharedPoolPtr<void> get_data() const override;

 private:
  explicit DoubleVector(UniquePoolPtr<double> data, size_t size);
  explicit DoubleVector(UniquePoolPtr<void> data, size_t byte_size);

  template <class ...Args>
  friend SharedPoolPtr<DoubleVector> create_prediction_data(Args&& ...args)
  {
    return SharedPoolPtr<DoubleVector>(new DoubleVector(std::forward<Args>(args)...));
  }

  SharedPoolPtr<double> data_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class SerializableString : public PredictionData {
 public:
  DataType type() const override;
  PredictionDataHash hash() override;
  size_t size() const override;
  size_t byte_size() const override;
  SharedPoolPtr<void> get_data() const override;

 private:
  explicit SerializableString(UniquePoolPtr<char> data, size_t size);
  explicit SerializableString(UniquePoolPtr<void> data, size_t byte_size);

  template <class ...Args>
  friend SharedPoolPtr<SerializableString> create_prediction_data(Args&& ...args)
  {
    return SharedPoolPtr<SerializableString>(new SerializableString(std::forward<Args>(args)...));
  }

  SharedPoolPtr<char> data_;
  size_t size_;
  boost::optional<PredictionDataHash> hash_;
};

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, SharedPoolPtr<PredictionData> input,
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
  SharedPoolPtr<PredictionData> input_;
  // TODO change this to a deadline instead of a duration
  long latency_budget_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
  std::chrono::time_point<std::chrono::high_resolution_clock> create_time_;
};

class Output {
 public:
  Output(const SharedPoolPtr<PredictionData> y_hat,
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

  SharedPoolPtr<PredictionData> y_hat_;
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
  Feedback(SharedPoolPtr<PredictionData> input, double y);

  Feedback(const Feedback &) = default;
  Feedback &operator=(const Feedback &) = default;

  Feedback(Feedback &&) = default;
  Feedback &operator=(Feedback &&) = default;

  double y_;
  SharedPoolPtr<PredictionData> input_;
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

  PredictTask(SharedPoolPtr<PredictionData> input, VersionedModelId model,
              float utility, QueryId query_id, long latency_slo_micros);

  PredictTask(const PredictTask &other) = default;

  PredictTask &operator=(const PredictTask &other) = default;

  PredictTask(PredictTask &&other) = default;

  PredictTask &operator=(PredictTask &&other) = default;

  SharedPoolPtr<PredictionData> input_;
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
  explicit PredictionRequest(std::vector<SharedPoolPtr<PredictionData>>& inputs,
                             DataType input_type);

  // Disallow copy
  PredictionRequest(PredictionRequest &other) = delete;
  PredictionRequest &operator=(PredictionRequest &other) = delete;

  // move constructors
  PredictionRequest(PredictionRequest &&other) = default;
  PredictionRequest &operator=(PredictionRequest &&other) = default;

  void add_input(SharedPoolPtr<PredictionData>& input);
  std::vector<ByteBuffer> serialize();

 private:
  void validate_input_type(SharedPoolPtr<PredictionData> &input) const;

  std::vector<SharedPoolPtr<PredictionData>> inputs_;
  DataType input_type_;
  size_t input_data_size_ = 0;
};

class PredictionResponse {
 public:
  PredictionResponse(const std::vector<SharedPoolPtr<PredictionData>> outputs);

  // Disallow copy
  PredictionResponse(PredictionResponse &other) = delete;
  PredictionResponse &operator=(PredictionResponse &other) = delete;

  // move constructors
  PredictionResponse(PredictionResponse &&other) = default;
  PredictionResponse &operator=(PredictionResponse &&other) = default;

  static PredictionResponse deserialize_prediction_response(std::vector<UniquePoolPtr<void>>& response);

  std::vector<SharedPoolPtr<PredictionData>> outputs_;
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
