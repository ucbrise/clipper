#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <memory>

#include "datatypes.hpp"

namespace clipper {

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

// class TaskExecutor {
//  public:
//   virtual std::vector<future<Output>> schedule_prediction(
//       const std::vector<PredictTask>& tasks) = 0;
//   virtual future<FeedbackAck> schedule_feedback(
//       const std::vector<FeedbackTask> tasks) = 0;
//
//  private:
//   ResourceState resource_state_;
// };
//
// class FakeTaskExecutor : TaskExecutor {
//  public:
//   virtual std::vector<boost::future<Output>> schedule_prediction(
//       const std::vector<PredictTask> t);
//   virtual std::vector<boost::future<FeedbackAck>> schedule_feedback(
//       const std::vector<FeedbackTask> t);
// };
//
// class Scheduler {
//  public:
//   virtual void add_model() = 0;
//   virtual void add_replica() = 0;
//
//   virtual void add_model() = 0;
//   virtual void add_replica() = 0;
//
//   // backpressure??
// };

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
