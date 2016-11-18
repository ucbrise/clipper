#ifndef CLIPPER_LIB_DATATYPES_H
#define CLIPPER_LIB_DATATYPES_H

#include <string>
#include <vector>

namespace clipper {

using ByteBuffer = std::vector<uint8_t>;
using VersionedModelId = std::pair<std::string, int>;
using QueryId = long;
using FeedbackAck = bool;

  size_t versioned_model_hash(const VersionedModelId& key);

  
class Output {
 public:
  ~Output() = default;

  Output(double y_hat, std::vector<VersionedModelId> model_id_);
  double y_hat_;
  std::vector<VersionedModelId> model_id_;
};

// using Output = std::pair<double;

class Input {
 public:
  // TODO: pure virtual or default?
  // virtual ~Input() = default;

  // TODO special member functions:
  //    + explicit?
  //    + virtual?

  // used by RPC system
  virtual ByteBuffer serialize() const = 0;
};

// class IntVector : Input {
//   public:
//     IntVector(std::vector<int> data);
//
//     // move constructors
//     IntVector(IntVector&& other) = default;
//     IntVector& operator=(IntVector&& other) = default;
//
//     // copy constructors
//     IntVector(IntVector& other) = default;
//     IntVector& operator=(IntVector& other) = default;
//
//     ByteBuffer serialize() const;
//
//   private:
//     std::vector<int> data_;
// };

class DoubleVector : public Input {
 public:
  explicit DoubleVector(std::vector<double> data);

  // move constructors
  DoubleVector(DoubleVector&& other) = default;
  DoubleVector& operator=(DoubleVector&& other) = default;

  // copy constructors
  DoubleVector(DoubleVector& other) = default;
  DoubleVector& operator=(DoubleVector& other) = default;

  ByteBuffer serialize() const;

 private:
  std::vector<double> data_;
};

class Query {
 public:
  ~Query() = default;

  Query(std::string label, long user_id, std::shared_ptr<Input> input,
        long latency_micros, std::string selection_policy,
        std::vector<VersionedModelId> candidate_models);

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
};

class Response {
 public:
  ~Response() = default;

  Response(Query query, QueryId query_id, long duration_micros,
           std::unique_ptr<Output> output,
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
  std::unique_ptr<Output> output_;
  std::vector<VersionedModelId> models_used_;
};

// using Feedback = std::pair<std::shared_ptr<Input>, std::shared_ptr<Output>>;

  
class Feedback {
public:
  ~Feedback() = default;
  Feedback(std::shared_ptr<Input> input,
           std::shared_ptr<Output> output,
           VersionedModelId model_id); // FIXME: don't need this

  double y_;
  std::shared_ptr<Input> input_;
  VersionedModelId model_id_;
};


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

}  // namespace clipper

#endif  // CLIPPER_LIB_DATATYPES_H
