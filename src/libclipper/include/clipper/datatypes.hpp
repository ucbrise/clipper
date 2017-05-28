#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <boost/optional.hpp>

namespace clipper {

using ByteBuffer = std::vector<uint8_t>;
using VersionedModelId = std::pair<std::string, int>;
using QueryId = long;
using FeedbackAck = bool;

enum class InputType {
  Bytes = 0,
  Ints = 1,
  Floats = 2,
  Doubles = 3,
  Strings = 4,
};

enum class RequestType {
  PredictRequest = 0,
  FeedbackRequest = 1,
};

size_t versioned_model_hash(const VersionedModelId &key);
std::string versioned_model_to_str(const VersionedModelId &model);
std::string get_readable_input_type(InputType type);
InputType parse_input_type(std::string type_string);

class Output {
 public:
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

  std::string y_hat_;
  std::vector<VersionedModelId> models_used_;
};

class Input {
 public:
  // TODO: pure virtual or default?
  // virtual ~Input() = default;

  virtual InputType type() const = 0;

  /**
   * Serializes input and writes resulting data to provided buffer.
   *
   * The serialization methods are used for RPC.
   */
  virtual size_t serialize(uint8_t *buf) const = 0;

  virtual size_t hash() const = 0;

  /**
   * @return The number of elements in the input
   */
  virtual size_t size() const = 0;
  /**
   * @return The size of the input data in bytes
   */
  virtual size_t byte_size() const = 0;
};

class ByteVector : public Input {
 public:
  explicit ByteVector(std::vector<uint8_t> data);

  // Disallow copy
  ByteVector(ByteVector &other) = delete;
  ByteVector &operator=(ByteVector &other) = delete;

  // move constructors
  ByteVector(ByteVector &&other) = default;
  ByteVector &operator=(ByteVector &&other) = default;

  InputType type() const override;
  size_t serialize(uint8_t *buf) const override;
  size_t hash() const override;
  size_t size() const override;
  size_t byte_size() const override;
  const std::vector<uint8_t> &get_data() const;

 private:
  std::vector<uint8_t> data_;
};

class IntVector : public Input {
 public:
  explicit IntVector(std::vector<int> data);

  // Disallow copy
  IntVector(IntVector &other) = delete;
  IntVector &operator=(IntVector &other) = delete;

  // move constructors
  IntVector(IntVector &&other) = default;
  IntVector &operator=(IntVector &&other) = default;

  InputType type() const override;
  size_t serialize(uint8_t *buf) const override;
  size_t hash() const override;
  size_t size() const override;
  size_t byte_size() const override;

  const std::vector<int> &get_data() const;

 private:
  std::vector<int> data_;
};

class FloatVector : public Input {
 public:
  explicit FloatVector(std::vector<float> data);

  // Disallow copy
  FloatVector(FloatVector &other) = delete;
  FloatVector &operator=(FloatVector &other) = delete;

  // move constructors
  FloatVector(FloatVector &&other) = default;
  FloatVector &operator=(FloatVector &&other) = default;

  InputType type() const override;
  size_t serialize(uint8_t *buf) const override;
  size_t hash() const override;
  size_t size() const override;
  size_t byte_size() const override;
  const std::vector<float> &get_data() const;

 private:
  std::vector<float> data_;
};

class DoubleVector : public Input {
 public:
  explicit DoubleVector(std::vector<double> data);

  // Disallow copy
  DoubleVector(DoubleVector &other) = delete;
  DoubleVector &operator=(DoubleVector &other) = delete;

  // move constructors
  DoubleVector(DoubleVector &&other) = default;
  DoubleVector &operator=(DoubleVector &&other) = default;

  InputType type() const override;
  size_t serialize(uint8_t *buf) const override;
  size_t hash() const override;
  size_t size() const override;
  size_t byte_size() const override;
  const std::vector<double> &get_data() const;

 private:
  std::vector<double> data_;
};

class SerializableString : public Input {
 public:
  explicit SerializableString(std::string data);

  // Disallow copy
  SerializableString(SerializableString &other) = delete;
  SerializableString &operator=(SerializableString &other) = delete;

  // move constructors
  SerializableString(SerializableString &&other) = default;
  SerializableString &operator=(SerializableString &&other) = default;

  InputType type() const override;
  size_t serialize(uint8_t *buf) const override;
  size_t hash() const override;
  size_t size() const override;
  size_t byte_size() const override;
  const std::string &get_data() const;

 private:
  std::string data_;
};

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, std::shared_ptr<Input> input,
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
  std::shared_ptr<Input> input_;
  // TODO change this to a deadline instead of a duration
  long latency_budget_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
  std::chrono::time_point<std::chrono::high_resolution_clock> create_time_;
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
  Feedback(std::shared_ptr<Input> input, double y);

  Feedback(const Feedback &) = default;
  Feedback &operator=(const Feedback &) = default;

  Feedback(Feedback &&) = default;
  Feedback &operator=(Feedback &&) = default;

  double y_;
  std::shared_ptr<Input> input_;
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

  PredictTask(std::shared_ptr<Input> input, VersionedModelId model,
              float utility, QueryId query_id, long latency_slo_micros);

  PredictTask(const PredictTask &other) = default;

  PredictTask &operator=(const PredictTask &other) = default;

  PredictTask(PredictTask &&other) = default;

  PredictTask &operator=(PredictTask &&other) = default;

  std::shared_ptr<Input> input_;
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
  explicit PredictionRequest(InputType input_type);
  explicit PredictionRequest(std::vector<std::shared_ptr<Input>> inputs,
                             InputType input_type);

  // Disallow copy
  PredictionRequest(PredictionRequest &other) = delete;
  PredictionRequest &operator=(PredictionRequest &other) = delete;

  // move constructors
  PredictionRequest(PredictionRequest &&other) = default;
  PredictionRequest &operator=(PredictionRequest &&other) = default;

  void add_input(std::shared_ptr<Input> input);
  std::vector<ByteBuffer> serialize();

 private:
  void validate_input_type(std::shared_ptr<Input> &input) const;

  std::vector<std::shared_ptr<Input>> inputs_;
  InputType input_type_;
  size_t input_data_size_ = 0;
};

class PredictionResponse {
 public:
  PredictionResponse(const std::vector<std::string> outputs);

  // Disallow copy
  PredictionResponse(PredictionResponse &other) = delete;
  PredictionResponse &operator=(PredictionResponse &other) = delete;

  // move constructors
  PredictionResponse(PredictionResponse &&other) = default;
  PredictionResponse &operator=(PredictionResponse &&other) = default;

  static PredictionResponse deserialize_prediction_response(ByteBuffer bytes);

  std::vector<std::string> outputs_;
};

}  // namespace rpc

}  // namespace clipper

#endif  // CLIPPER_LIB_DATATYPES_H
