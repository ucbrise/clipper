#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "util.hpp"
#include "rpc_service.hpp"


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
  ModelContainer(const ModelContainer &) = delete;
  ModelContainer &operator=(const ModelContainer &) = delete;

  ModelContainer(ModelContainer &&) = default;
  ModelContainer &operator=(ModelContainer &&) = default;

  VersionedModelId model_;
  int get_queue_size();
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

  CacheEntry(const CacheEntry &) = delete;
  CacheEntry &operator=(const CacheEntry &) = delete;

  CacheEntry(CacheEntry &&) = default;
  CacheEntry &operator=(CacheEntry &&) = default;

  bool completed_ = false;
  boost::promise<Output> value_promise_;
  boost::shared_future<Output> value_;
};

class PredictionCache {
 public:
  boost::shared_future<Output> fetch(const VersionedModelId &model,
                                     const std::shared_ptr<Input> &input);

  void put(const VersionedModelId &model, const std::shared_ptr<Input> &input,
           const Output &output);

 private:
  std::mutex m_;
  size_t hash(const VersionedModelId &model, size_t input_hash) const;
  // TODO cache needs a promise as well?
  std::unordered_map<long, CacheEntry> cache_;
};

template <typename Scheduler>
class TaskExecutor {
 public:
  ~TaskExecutor() = default;
  TaskExecutor()
      : active_containers_(
            std::unordered_map<VersionedModelId,
                               std::vector<std::shared_ptr<ModelContainer>>,
                               decltype(&versioned_model_hash)>(
                100, &versioned_model_hash)),
                rpc_(std::make_unique<RPCService>()) {
    std::cout << "TaskExecutor started" << std::endl;
    // TODO: Remove hardcoded active model container here
    VersionedModelId mid = std::make_pair("m", 1);
    //        ModelContainer mc {mid, "127.0.0.1:6001"};
    //        auto entry = std::make_pair(mid, ModelContainer{mid,
    //        "127.0.0.1:6001"});


    rpc_->start("0.0.0.0", 7000);
    active_containers_.emplace(
        mid, std::vector<std::shared_ptr<ModelContainer>>{
                 std::make_shared<ModelContainer>(mid, "127.0.0.1:6001")});
  }

  TaskExecutor(const TaskExecutor &other) = delete;
  TaskExecutor &operator=(const TaskExecutor &other) = delete;

  TaskExecutor(TaskExecutor &&other) = default;
  TaskExecutor &operator=(TaskExecutor &&other) = default;

  std::vector<boost::shared_future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    // TODO this needs to be a shared lock, nearly all
    // accesses on this mutex won't modify the set of active
    // containers
      std::shared_lock<std::shared_timed_mutex> l{active_containers_mutex_};
      std::vector<boost::shared_future<Output>> output_futures;
      for (auto t: tasks) {
          // assign tasks to containers independently
          std::cout << "Model: " << &t.model_ << std::endl;
          auto task_model_replicas = active_containers_.find(t.model_);
          if (task_model_replicas != active_containers_.end()) {
              std::shared_ptr<ModelContainer> container =
              scheduler_.assign_container(t, task_model_replicas->second);
              container->send_prediction(t);
              output_futures.push_back(std::move(cache_.fetch(t.model_, t.input_)));
//              output_futures.push_back(boost::make_ready_future(Output{3.3, std::make_pair("rando_fake_model", 7)}));
          }
      }
    return output_futures;
  }

  std::vector<boost::future<FeedbackAck>> schedule_feedback(
      const std::vector<FeedbackTask> tasks) {
    UNUSED(tasks);
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
  std::shared_timed_mutex active_containers_mutex_;

  // Each queue corresponds to a single model container.
  std::unordered_map<VersionedModelId,
                     std::vector<std::shared_ptr<ModelContainer>>,
                     decltype(&versioned_model_hash)>
      active_containers_;

  std::unique_ptr<RPCService> rpc_;
  Scheduler scheduler_;
  PredictionCache cache_;
};

class PowerTwoChoicesScheduler {
 public:
  std::shared_ptr<ModelContainer> assign_container(
      const PredictTask &task,
      std::vector<std::shared_ptr<ModelContainer>> &containers) const;
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
