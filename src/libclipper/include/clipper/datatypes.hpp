#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <string>
#include <vector>

#include "rpc_generated.h"

namespace clipper {

using ByteBuffer = std::vector<uint8_t>;
using VersionedModelId = std::pair<std::string, int>;
using QueryId = long;
using FeedbackAck = bool;
template <class T> using SerializableVector = flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<T>>>;

size_t versioned_model_hash(const VersionedModelId &key);

class Output {
 public:
  ~Output() = default;
  explicit Output() = default;
  Output(const Output &) = default;
  Output &operator=(const Output &) = default;

  Output(Output &&) = default;
  Output &operator=(Output &&) = default;
  Output(double y_hat, VersionedModelId versioned_model);
  double y_hat_;
  VersionedModelId versioned_model_;
};

// using Output = std::pair<double;

enum class InputType {
  Ints,
  Floats,
  Doubles,
  Strings,
  Bytes
};

class Input {
 public:
  // TODO: pure virtual or default?
  // virtual ~Input() = default;

  // used by RPC system
  virtual size_t hash() const = 0;
  virtual size_t size() const = 0;
  virtual InputType type() const = 0;

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

  size_t hash() const;
  size_t size() const;
  InputType type() const;

  const double *get_serializable_data() const;

 private:
  std::vector<double> data_;
};

class ByteVector : public Input {
 public:
  explicit ByteVector(std::vector<uint8_t> data);

  //Disallow copy
  ByteVector(ByteVector &other) = delete;
  ByteVector &operator=(ByteVector &other) = delete;

  //move constructors
  ByteVector(ByteVector &&other) = default;
  ByteVector &operator=(ByteVector &&other) = default;

  size_t hash() const;
  size_t size() const;
  InputType type() const;

  const uint8_t *get_serializable_data() const;

 private:
  std::vector<uint8_t> data_;

};

class IntVector : public Input {
 public:
  explicit IntVector(std::vector<int> data);

  //Disallow copy
  IntVector(IntVector &other) = delete;
  IntVector &operator=(IntVector &other) = delete;

  //move constructors
  IntVector(IntVector &&other) = default;
  IntVector &operator=(IntVector &&other) = default;

  size_t hash() const;
  size_t size() const;
  InputType type() const;

  const int *get_serializable_data() const;

 private:
  std::vector<int> data_;

};

class FloatVector : public Input {
 public:
  explicit FloatVector(std::vector<float> data);

  //Disallow copy
  FloatVector(FloatVector &other) = delete;
  FloatVector &operator=(FloatVector &other) = delete;

  //move constructors
  FloatVector(FloatVector &&other) = default;
  FloatVector &operator=(FloatVector &&other) = default;

  size_t hash() const;
  size_t size() const;
  InputType type() const;

  const float *get_serializable_data() const;

 private:
  std::vector<float> data_;

};

class StringVector : public Input {
 public:
  explicit StringVector(std::vector<std::string> data);

  //Disallow copy
  StringVector(StringVector &other) = delete;
  StringVector &operator=(StringVector &other) = delete;

  //move constructors
  StringVector(StringVector &&other) = default;
  StringVector &operator=(StringVector &&other) = default;

  size_t hash() const;
  size_t size() const;
  InputType type() const;

  const std::string *get_serializable_data() const;

 private:
  std::vector<std::string> data_;

};

class BatchPredictionRequest {
 public:
  BatchPredictionRequest() {};
  explicit BatchPredictionRequest(std::vector<std::shared_ptr<const Input>> inputs);

  // Disallow copy
  BatchPredictionRequest(BatchPredictionRequest &other) = delete;
  BatchPredictionRequest &operator=(BatchPredictionRequest &other) = delete;

  // move constructors
  BatchPredictionRequest(BatchPredictionRequest &&other) = default;
  BatchPredictionRequest &operator=(BatchPredictionRequest &&other) = default;

  const ByteBuffer serialize() const;
  void add_input(std::shared_ptr<const Input> input);

 private:
  std::vector<std::shared_ptr<const Input>> int_inputs_;
  std::vector<std::shared_ptr<const Input>> float_inputs_;
  std::vector<std::shared_ptr<const Input>> double_inputs_;
  std::vector<std::shared_ptr<const Input>> byte_inputs_;
  std::vector<std::shared_ptr<const Input>> string_inputs_;

  const SerializableVector<IntVec> get_serializable_ints(flatbuffers::FlatBufferBuilder &fbb) const;
  const SerializableVector<FloatVec> get_serializable_floats(flatbuffers::FlatBufferBuilder &fbb) const;
  const SerializableVector<DoubleVec> get_serializable_doubles(flatbuffers::FlatBufferBuilder &fbb) const;
  const SerializableVector<ByteVec> get_serializable_bytes(flatbuffers::FlatBufferBuilder &fbb) const;
  const SerializableVector<StringVec> get_serializable_strings(flatbuffers::FlatBufferBuilder &fbb) const;
};

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, std::shared_ptr<Input> input,
        long latency_micros, std::string selection_policy,
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
  long latency_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
};

class Response {
 public:
  ~Response() = default;

  Response(Query query, QueryId query_id, long duration_micros, Output output,
           std::vector<VersionedModelId> models_used);

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
  std::vector<VersionedModelId> models_used_;
};

using Feedback = std::pair<std::shared_ptr<Input>, Output>;

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
              float utility, QueryId query_id,
              long latency_slo_micros);

  PredictTask(const PredictTask &other) = default;

  PredictTask &operator=(const PredictTask &other) = default;

  PredictTask(PredictTask &&other) = default;

  PredictTask &operator=(PredictTask &&other) = default;

  std::shared_ptr<Input> input_;
  VersionedModelId model_;
  float utility_;
  QueryId query_id_;
  long latency_slo_micros_;
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

}  // namespace clipper

#endif  // CLIPPER_LIB_DATATYPES_H
