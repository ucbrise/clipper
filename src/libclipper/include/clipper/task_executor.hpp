#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <memory>
#include <mutex>
#include <unordered_map>

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

// class TaskExecutor {
// public:
//  std::vector<boost::future<Output>> schedule_predictions(
//      const std::vector<PredictTask> &tasks);
//
//  std::vector<boost::future<FeedbackAck>> schedule_feedback(
//      const std::vector<FeedbackTask> tasks);
//
//  //  private:
//  // ResourceState resource_state_;
//};

class ModelContainer {
 public:
  ~ModelContainer() = default;
  ModelContainer(VersionedModelId model, std::string address);
  // disallow copy
  ModelContainer ModelContainer(const ModelContainer&) = delete;
  ModelContainer& operator=(const ModelContainer&) = delete;

  ModelContainer ModelContainer(ModelContainer&&) = default;
  ModelContainer& operator=(ModelContainer&&) = default;


  VersionedModelId model_;
  int get_queue_size() const;
  void send_prediction(PredictTask task);
  void send_feedback(PredictTask task);

 private:
  std::string address_;
  bool connected_{true};
  Queue<PredictTask> request_queue_;
  Queue<FeedbackTask> feedback_queue_;
};

class CacheEntry {
 public:
  CacheEntry();
  ~CacheEntry() = default;

  bool completed_ = false;
  boost::promise<Output> value_promise_;
  boost::shared_future<Output> value_;
};

class PredictionCache {
 public:
  boost::shared_future<Output> fetch(const VersionedModelId &model,
                                     const Input &input);
  void put(const VersionedModelId &model, const Input &input,
                            const Output &output);

 private:
  const long hash(const Input &input);
  std::mutex m_;
  // TODO cache needs a promise as well?
  std::unordered_map<long, CacheEntry> cache_;
};

template <typename Scheduler>
class TaskExecutor {
 public:
  std::vector<boost::future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    // TODO this needs to be a shared lock, nearly all
    // accesses on this mutex won't modify the set of active
    // containers
    std::unique_lock<std::mutex> l(active_containers_mutex_);
    std::vector<boost::future<Output>> output_futures(tasks.size());
    while (tasks.size() > 0) {
      auto t = tasks.front();
      tasks.erase(tasks.begin());
      // assign tasks to containers independently
      auto container = scheduler_.assign_container(t, active_containers_);
      container.send_prediction(t);
      output_futures.push_back(cache_.fetch(task.model_, &task.input_));
    }
    return output_futures;
  }

  std::vector<boost::future<FeedbackAck>> schedule_feedback(
      const std::vector<FeedbackTask> tasks) {
    // TODO Implement
    return {};
  }

  // void add_model(VersionedModelId model);
  // void add_container(VersionedModelId model);
  //
  // void remove_model(VersionedModelId model);

 private:
  // Protects the map of task queues. Must acquire an exclusive
  // lock to modify request_queues_ and a shared_lock when accessing
  // the queues. The queues are independently threadsafe.
  std::mutex active_containers_mutex_;

  // Each queue corresponds to a single model container.
  std::unordered_map<VersionedModelId, std::vector<ModelContainer>>
      active_containers_;

  Scheduler scheduler_;
  PredictionCache cache_;
};

class PowerTwoChoicesScheduler {
 public:
  const ModelContainer &assign_container(
      const PredictTask &task, const std::vector<ModelContainer> &containers);
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
