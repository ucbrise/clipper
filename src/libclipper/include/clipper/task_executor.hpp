#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <memory>

#define BOOST_THREAD_VERSION 3
#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "util.hpp"

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

class TaskExecutor {
 public:
  std::vector<boost::future<Output>> schedule_predictions(
      const std::vector<PredictTask>& tasks);
  std::vector<boost::future<FeedbackAck>> schedule_feedback(
      const std::vector<FeedbackTask> tasks);

  //  private:
  // ResourceState resource_state_;
};

class ModelContainer {
  VersionedModelId model_;
  int get_queue_size() const;

 private:
  std::string address;

  bool connected_{true};
  Queue<PredictTask> request_queue_{};
  Queue<FeedbackTask> feedback_queue_{};
};

template <typename Scheduler>
class BatchingTaskExecutor {
 public:
  std::vector<boost::future<Output>> schedule_predictions(
      const std::vector<PredictTask>& tasks) {}
  virtual std::vector<boost::future<FeedbackAck>> schedule_feedback(
      const std::vector<FeedbackTask> tasks);

  void add_model(VersionedModelId model);

  void remove_model(VersionedModelId model);

 private:
  // Protects the map of task queues. Must acquire an exclusive
  // lock to modify request_queues_ and a shared_lock when accessing
  // the queues. The queues are independently threadsafe.
  std::mutex active_containers_mutex_;

  // Each queue corresponds to a single model container.
  std::unordered_map<VersionedModelId, std::vector<ModelContainer>>
      active_containers_;

  Scheduler scheduler_;
};

class PowerTwoChoicesScheduler {
 public:
  const ModelContainer& assign_container(
      const PredictTask& task, const std::vector<ModelContainer>& containers);
};

// class PowerTwoChoicesScheduler {
//   public:
//     const ModelContainer& schedule_prediction(
//
//
// }

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
//   virtual void remove_model() = 0;
//   virtual void remove_replica() = 0;
//
//   // backpressure??
// };

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
