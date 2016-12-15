#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/thread.hpp>

#include <clipper/concurrency.hpp>
#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/rpc_service.hpp>

namespace clipper {

std::vector<float> deserialize_outputs(std::vector<uint8_t> bytes);

class CacheEntry {
 public:
  CacheEntry();
  ~CacheEntry() = default;

  CacheEntry(const CacheEntry&) = delete;
  CacheEntry& operator=(const CacheEntry&) = delete;

  CacheEntry(CacheEntry&&) = default;
  CacheEntry& operator=(CacheEntry&&) = default;

  bool completed_ = false;
  boost::promise<Output> value_promise_;
  boost::shared_future<Output> value_;
};

class PredictionCache {
 public:
  boost::shared_future<Output> fetch(const VersionedModelId& model,
                                     const std::shared_ptr<Input>& input);

  void put(const VersionedModelId& model, const std::shared_ptr<Input>& input,
           const Output& output);

 private:
  std::mutex m_;
  size_t hash(const VersionedModelId& model, size_t input_hash) const;
  // TODO cache needs a promise as well?
  std::unordered_map<long, CacheEntry> cache_;
};

struct DeadlineCompare {
  bool operator()(const std::pair<Deadline, PredictTask>& lhs,
                  const std::pair<Deadline, PredictTask>& rhs) {
    return lhs.first > rhs.first;
  }
};

// thread safe model queue
class ModelQueue {
 public:
  ModelQueue() : queue_(ModelPQueue{}) {}

  // Disallow copy and assign
  ModelQueue(const ModelQueue&) = delete;
  ModelQueue& operator=(const ModelQueue&) = delete;

  ModelQueue(ModelQueue&&) = default;
  ModelQueue& operator=(ModelQueue&&) = default;

  ~ModelQueue() = default;

  void add_task(PredictTask task) {
    std::unique_lock<std::mutex> l(queue_mutex_);
    Deadline deadline = std::chrono::high_resolution_clock::now() +
                        std::chrono::microseconds(task.latency_slo_micros_);
    queue_.emplace(deadline, std::move(task));
    queue_not_empty_.notify_all();
  }

  int get_size() {
    std::unique_lock<std::mutex> l(queue_mutex_);
    return queue_.size();
  }

  // TODO: If we implement the scheduler so that it calls
  // get_earliest_deadline(), then uses that to compute the batch size,
  // then calls dequeue with that batch size, it's possible for a new
  // task with an earlier deadline to be submitted. I'm not sure what the
  // correct behavior here should be.
  Deadline get_earliest_deadline() {
    std::unique_lock<std::mutex> l(queue_mutex_);
    if (queue_.size() == 0) {
      queue_not_empty_.wait(l, [this] { return queue_.size() > 0; });
    }
    return queue_.top().first;
  }

  // Dequeues up to max_batch_size tasks from the queue without blocking
  std::vector<PredictTask> get_batch(int max_batch_size) {
    // NOTE: Because the queue lock is released and reacquired between a
    // call to get_earliest_deadline() (which blocks until the queue is
    // not empty) and the call to this method, it's possible for the queue
    // to be empty, in which case the returned vector will have size 0.

    std::unique_lock<std::mutex> l(queue_mutex_);
    std::vector<PredictTask> batch;
    while (batch.size() < (size_t)max_batch_size && queue_.size() > 0) {
      batch.push_back(queue_.top().second);
      queue_.pop();
    }
    return batch;
  }

 private:
  // Min PriorityQueue so that the task with the earliest
  // deadline is at the front of the queue
  using ModelPQueue =
      std::priority_queue<std::pair<Deadline, PredictTask>,
                          std::vector<std::pair<Deadline, PredictTask>>,
                          DeadlineCompare>;
  ModelPQueue queue_;
  std::mutex queue_mutex_;
  std::condition_variable_any queue_not_empty_;
};

// template <typename Scheduler>
class TaskExecutor {
 public:
  ~TaskExecutor() { active_ = false; };
  explicit TaskExecutor()
      : active_containers_(std::make_shared<ActiveContainers>()),
        rpc_(std::make_unique<RPCService>(active_containers_)),
        model_queues_(std::unordered_map<VersionedModelId, ModelQueue,
                                         decltype(&versioned_model_hash)>(
            100, &versioned_model_hash)) {
    std::cout << "TaskExecutor started" << std::endl;
    rpc_->start(
        "*", 7000,
        [this](int container_id) { this->container_ready(container_id); },
        [this](int container_id) { this->new_container(container_id); },
        [this](RPCResponse response) { this->process_response(response); });
    active_ = true;
    // boost::thread(&TaskExecutor::send_messages, this).detach();
    // boost::thread(&TaskExecutor::recv_messages, this).detach();
  }

  // Disallow copy
  TaskExecutor(const TaskExecutor& other) = delete;
  TaskExecutor& operator=(const TaskExecutor& other) = delete;

  TaskExecutor(TaskExecutor&& other) = default;
  TaskExecutor& operator=(TaskExecutor&& other) = default;

  std::vector<boost::shared_future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    std::vector<boost::shared_future<Output>> output_futures;
    for (auto t : tasks) {
      auto queue = model_queues_.find(t.model_);
      if (queue != model_queues_.end()) {
        output_futures.push_back(cache_.fetch(t.model_, t.input_));
        // only send to scheduler if the value is not in the cache yet
        if (!output_futures.back().is_ready()) {
          queue->second.add_task(t);
        }
      } else {
        std::cerr << "Received task for unknown model: {" << t.model_.first
                  << ", " << t.model_.second << "}" << std::endl;
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

 private:
  void new_container(int container_id) {
    // look up container
    // see if container represents new model
    // if it does, create new model queue
    auto c = active_containers_->get_container_by_id(container_id);
    if (c) {
      std::cout << "Found new container with id: " << container_id << std::endl;
      std::shared_ptr<ModelContainer> container = *c;
      auto model_id = container->model_;

      // NOTE: emplace only inserts if the key doesn't exist.
      auto queued_added = model_queues_.emplace(std::piecewise_construct,
                                                std::forward_as_tuple(model_id),
                                                std::forward_as_tuple());
      if (queued_added.second) {
        std::cout << "Container belonged to new model: " << model_id.first
                  << ":" << model_id.second << std::endl;
      }

    } else {
      std::cerr << "Container with unknown ID: " << container_id
                << " signalled newly active." << std::endl;
    }
  }

  // These should be executed by a threadpool
  void container_ready(int container_id) {
    // look up container
    auto c = active_containers_->get_container_by_id(container_id);
    if (c) {
      std::shared_ptr<ModelContainer> container = *c;
      // get associated model queue
      auto model_id = container->model_;
      auto queue = model_queues_.find(model_id);
      assert(queue != model_queues_.end());
      std::vector<PredictTask> batch;

      // Even though get_earliest_deadline() will block until the queue is not
      // empty, it's possible for a competing container to steal the work
      // from the queue between the call to get_earliest_deadline() and
      // get_batch(). For this reason, we use a while loop to keep trying
      // to get work from the queue until we are successful.
      while (batch.size() == 0) {
        // This blocks to ensure that queue has something in it
        auto earliest_deadline = queue->second.get_earliest_deadline();
        int max_batch_size = container->get_batch_size(earliest_deadline);
        batch = queue->second.get_batch(max_batch_size);
      }
      std::unique_lock<std::mutex> l(inflight_messages_mutex_);
      std::vector<std::vector<uint8_t>> serialized_inputs;
      std::vector<std::pair<VersionedModelId, std::shared_ptr<Input>>>
          cur_batch;
      for (auto b : batch) {
        serialized_inputs.push_back(b.input_->serialize());
        cur_batch.emplace_back(b.model_, b.input_);
      }
      int message_id =
          rpc_->send_message(serialized_inputs, container->container_id_);
      inflight_messages_.emplace(message_id, std::move(cur_batch));
    } else {
      std::cerr << "Container with unknown ID: " << container_id
                << " signalled ready for processing" << std::endl;
    }
  }

  // TODO TODO TODO: track latency and update latency estimates
  void process_response(RPCResponse response) {
    std::unique_lock<std::mutex> l(inflight_messages_mutex_);
    auto keys = inflight_messages_[response.first];
    inflight_messages_.erase(response.first);
    std::vector<float> deserialized_outputs =
        deserialize_outputs(response.second);
    assert(deserialized_outputs.size() == keys.size());
    int batch_size = keys.size();
    for (int batch_num = 0; batch_num < batch_size; ++batch_num) {
      cache_.put(
          keys[batch_num].first, keys[batch_num].second,
          Output{deserialized_outputs[batch_num], keys[batch_num].first});
    }
  }

  // active_containers_ is shared with the RPC service so it can add new
  // containers
  // to the collection when they connect
  std::shared_ptr<ActiveContainers> active_containers_;
  std::unique_ptr<RPCService> rpc_;
  // Scheduler scheduler_;
  std::unordered_map<VersionedModelId, ModelQueue,
                     decltype(&versioned_model_hash)>
      model_queues_;
  PredictionCache cache_;
  bool active_ = false;
  std::mutex inflight_messages_mutex_;
  std::unordered_map<
      int, std::vector<std::pair<VersionedModelId, std::shared_ptr<Input>>>>
      inflight_messages_;
};

// class PowerTwoChoicesScheduler {
//  public:
//   std::shared_ptr<ModelContainer> assign_container(
//       const PredictTask& task,
//       std::vector<std::shared_ptr<ModelContainer>>& containers) const;
// };

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
