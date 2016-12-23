#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <memory>
#include <string>
#include <vector>
#include <chrono>

namespace clipper {

using ByteBuffer = std::vector<uint8_t>;
using VersionedModelId = std::pair<std::string, int>;
using QueryId = long;
using FeedbackAck = bool;

size_t versioned_model_hash(const VersionedModelId& key);

class Output {
 public:
  ~Output() = default;
  explicit Output() = default;
  Output(const Output&) = default;
  Output& operator=(const Output&) = default;

  Output(Output&&) = default;
  Output& operator=(Output&&) = default;
  Output(double y_hat, VersionedModelId versioned_model);
  double y_hat_;
  VersionedModelId versioned_model_;
};

// using Output = std::pair<double;

class Input {
 public:
  // TODO: pure virtual or default?
  // virtual ~Input() = default;

  // used by RPC system
  virtual const ByteBuffer serialize() const = 0;
  virtual size_t hash() const = 0;
};

class DoubleVector : public Input {
 public:
  explicit DoubleVector(std::vector<double> data);

  // Disallow copy
  DoubleVector(DoubleVector& other) = delete;
  DoubleVector& operator=(DoubleVector& other) = delete;

  // move constructors
  DoubleVector(DoubleVector&& other) = default;
  DoubleVector& operator=(DoubleVector&& other) = default;

  const ByteBuffer serialize() const;

  size_t hash() const;

 private:
  std::vector<double> data_;
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
  Query(const Query&) = default;
  Query& operator=(const Query&) = default;

  // move constructors
  Query(Query&&) = default;
  Query& operator=(Query&&) = default;

  // Used to provide a namespace for queries. The expected
  // use is to distinguish queries coming from different
  // REST endpoints.
  std::string label_;
  long user_id_;
  std::shared_ptr<Input> input_;
  long latency_micros_;
  std::string selection_policy_;
  std::vector<VersionedModelId> candidate_models_;
  std::chrono::time_point<std::chrono::high_resolution_clock> create_time_;
};

class Response {
 public:
  ~Response() = default;

  Response(Query query, QueryId query_id, long duration_micros, Output output,
           std::vector<VersionedModelId> models_used);

  // default copy constructors
  Response(const Response&) = default;
  Response& operator=(const Response&) = default;

  // default move constructors
  Response(Response&&) = default;
  Response& operator=(Response&&) = default;

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

  FeedbackQuery(const FeedbackQuery&) = default;
  FeedbackQuery& operator=(const FeedbackQuery&) = default;

  FeedbackQuery(FeedbackQuery&&) = default;
  FeedbackQuery& operator=(FeedbackQuery&&) = default;

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

  PredictTask(const PredictTask& other) = default;

  PredictTask& operator=(const PredictTask& other) = default;

  PredictTask(PredictTask&& other) = default;

  PredictTask& operator=(PredictTask&& other) = default;

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

  FeedbackTask(const FeedbackTask& other) = default;

  FeedbackTask& operator=(const FeedbackTask& other) = default;

  FeedbackTask(FeedbackTask&& other) = default;

  FeedbackTask& operator=(FeedbackTask&& other) = default;

  Feedback feedback_;
  VersionedModelId model_;
  QueryId query_id_;
  long latency_slo_micros_;
};

}  // namespace clipper

#endif  // CLIPPER_LIB_DATATYPES_H
