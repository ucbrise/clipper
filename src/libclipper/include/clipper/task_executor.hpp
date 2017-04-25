#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <redox.hpp>

#include <clipper/config.hpp>
#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/threadpool.hpp>
#include <clipper/util.hpp>

namespace clipper {

const std::string LOGGING_TAG_TASK_EXECUTOR = "TASKEXECUTOR";

std::vector<float> deserialize_outputs(std::vector<uint8_t> bytes);

class CacheEntry {
 public:
  CacheEntry();
  ~CacheEntry() = default;

  CacheEntry(const CacheEntry &) = delete;
  CacheEntry &operator=(const CacheEntry &) = delete;

  CacheEntry(CacheEntry &&) = default;
  CacheEntry &operator=(CacheEntry &&) = default;

  bool completed_ = false;
  Output value_;
  std::vector<boost::promise<Output>> value_promises_;
};

class PredictionCache {
 public:
  PredictionCache();
  boost::future<Output> fetch(const VersionedModelId &model,
                              const std::shared_ptr<Input> &input);

  void put(const VersionedModelId &model, const std::shared_ptr<Input> &input,
           const Output &output);

 private:
  std::mutex m_;
  size_t hash(const VersionedModelId &model, size_t input_hash) const;
  // TODO cache needs a promise as well?
  std::unordered_map<long, CacheEntry> cache_;
  std::shared_ptr<metrics::Counter> lookups_counter_;
  std::shared_ptr<metrics::RatioCounter> hit_ratio_;
};

struct DeadlineCompare {
  bool operator()(const std::pair<Deadline, PredictTask> &lhs,
                  const std::pair<Deadline, PredictTask> &rhs) {
    return lhs.first > rhs.first;
  }
};

// thread safe model queue
class ModelQueue {
 public:
  ModelQueue() : queue_(ModelPQueue{}) {}

  // Disallow copy and assign
  ModelQueue(const ModelQueue &) = delete;
  ModelQueue &operator=(const ModelQueue &) = delete;

  ModelQueue(ModelQueue &&) = default;
  ModelQueue &operator=(ModelQueue &&) = default;

  ~ModelQueue() = default;

  void add_task(PredictTask task) {
    std::unique_lock<std::mutex> l(queue_mutex_);
    Deadline deadline = std::chrono::system_clock::now() +
                        std::chrono::microseconds(task.latency_slo_micros_);
    queue_.emplace(deadline, std::move(task));
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
  boost::optional<Deadline> get_earliest_deadline() {
    std::unique_lock<std::mutex> l(queue_mutex_);
    remove_tasks_with_elapsed_deadlines();
    if (!queue_.empty()) {
      return boost::optional<Deadline>(queue_.top().first);
    } else {
      return boost::optional<Deadline>{};
    }
  }

  // Dequeues up to max_batch_size tasks from the queue without blocking
  std::vector<PredictTask> get_batch(int max_batch_size) {
    // NOTE: Because the queue lock is released and reacquired between a
    // call to get_earliest_deadline() (which blocks until the queue is
    // not empty) and the call to this method, it's possible for the queue
    // to be empty, in which case the returned vector will have size 0.
    std::unique_lock<std::mutex> l(queue_mutex_);
    remove_tasks_with_elapsed_deadlines();
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

  // Deletes tasks with deadlines prior or equivalent to the
  // current system time. This method should only be called
  // when a unique lock on the queue_mutex is held.
  void remove_tasks_with_elapsed_deadlines() {
    std::chrono::time_point<std::chrono::system_clock> current_time =
        std::chrono::system_clock::now();
    while (!queue_.empty()) {
      Deadline first_deadline = queue_.top().first;
      if (first_deadline <= current_time) {
        // If a task's deadline has already elapsed,
        // we should not process it
        queue_.pop();
      } else {
        break;
      }
    }
  }
};

class InflightMessage {
 public:
  InflightMessage(
      const std::chrono::time_point<std::chrono::system_clock> send_time,
      const int container_id, const VersionedModelId model,
      const std::shared_ptr<Input> input)
      : send_time_(send_time),
        container_id_(container_id),
        model_(model),
        input_(input) {}

  // Default copy and move constructors
  InflightMessage(const InflightMessage &) = default;
  InflightMessage(InflightMessage &&) = default;

  // Default assignment operators
  InflightMessage &operator=(const InflightMessage &) =
      default InflightMessage & operator=(InflightMessage &&) = default;

  std::chrono::time_point<std::chrono::system_clock> send_time_;
  int container_id_;
  VersionedModelId model_;
  std::shared_ptr<Input> input_;
};

class TaskExecutor {
 public:
  ~TaskExecutor() { active_->store(false); };
  explicit TaskExecutor()
      : active_(std::make_shared<std::atomic_bool>(true)),
        active_containers_(std::make_shared<ActiveContainers>()),
        rpc_(std::make_unique<rpc::RPCService>()),
        model_queues_(std::unordered_map<const VersionedModelId, ModelQueue,
                                         decltype(&versioned_model_hash)>(
            INITIAL_MODEL_QUEUES_MAP_SIZE, &versioned_model_hash)) {
    log_info(LOGGING_TAG_TASK_EXECUTOR, "TaskExecutor started");
    rpc_->start(
        "*", RPC_SERVICE_PORT, [ this, task_executor_valid = active_ ](
                                   VersionedModelId model, int replica_id) {
          if (*task_executor_valid) {
            on_container_ready(model, replica_id);
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_container_ready callback because "
                     "TaskExecutor has been destroyed.");
          }
        },
        [ this, task_executor_valid = active_ ](rpc::RPCResponse response) {
          if (*task_executor_valid) {
            on_response_recv(std::move(response));
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_response_recv callback because "
                     "TaskExecutor has been destroyed.");
          }

        });
    Config &conf = get_config();
    while (!redis_connection_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_TASK_EXECUTOR,
                "TaskExecutor failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(conf.get_redis_address(),
                                      conf.get_redis_port())) {
      log_error(LOGGING_TAG_TASK_EXECUTOR,
                "TaskExecutor subscriber failed to connect to redis",
                "Retrying in 1 second...");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    redis::send_cmd_no_reply<std::string>(
        redis_connection_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
    redis::subscribe_to_container_changes(
        redis_subscriber_,
        // event_type corresponds to one of the Redis event types
        // documented in https://redis.io/topics/notifications.
        [ this, task_executor_valid = active_ ](const std::string &key,
                                                const std::string &event_type) {
          if (event_type == "hset" && *task_executor_valid) {
            auto container_info =
                redis::get_container_by_key(redis_connection_, key);
            VersionedModelId vm =
                std::make_pair(container_info["model_name"],
                               std::stoi(container_info["model_version"]));
            int replica_id = std::stoi(container_info["model_replica_id"]);
            active_containers_->add_container(
                vm, std::stoi(container_info["zmq_connection_id"]), replica_id,
                parse_input_type(container_info["input_type"]));

            TaskExecutionThreadPool::submit_job([this, vm, replica_id]() {
              on_container_ready(vm, replica_id);
            });
            bool created_queue = create_model_queue_if_necessary(vm);
            if (created_queue) {
              log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                                 "Created queue for new model: {} : {}",
                                 vm.first, vm.second);
            }
          } else if (!*task_executor_valid) {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running TaskExecutor's "
                     "subscribe_to_container_changes callback because "
                     "TaskExecutor has been destroyed.");
          }

        });
    throughput_meter = metrics::MetricsRegistry::get_metrics().create_meter(
        "model_throughput");
    predictions_counter =
        metrics::MetricsRegistry::get_metrics().create_counter(
            "num_predictions");
    throughput_meter = metrics::MetricsRegistry::get_metrics().create_meter(
        "prediction_throughput");
    latency_hist = metrics::MetricsRegistry::get_metrics().create_histogram(
        "prediction_latency", "milliseconds", 2056);
  }

  // Disallow copy
  TaskExecutor(const TaskExecutor &other) = delete;
  TaskExecutor &operator=(const TaskExecutor &other) = delete;

  TaskExecutor(TaskExecutor &&other) = default;
  TaskExecutor &operator=(TaskExecutor &&other) = default;

  std::vector<boost::future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    predictions_counter->increment(tasks.size());
    std::vector<boost::future<Output>> output_futures;
    for (auto t : tasks) {
      // add each task to the queue corresponding to its associated model
      auto model_queue_entry = model_queues_.find(t.model_);
      if (model_queue_entry != model_queues_.end()) {
        output_futures.push_back(cache_.fetch(t.model_, t.input_));
        if (!output_futures.back().is_ready()) {
          t.recv_time_ = std::chrono::system_clock::now();
          model_queue_entry->second.add_task(t);
        }
      } else {
        log_error_formatted(LOGGING_TAG_TASK_EXECUTOR,
                            "Received task for unknown model: {} : {}",
                            t.model_.first, t.model_.second);
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
  // active_containers_ is shared with the RPC service so it can add new
  // containers to the collection when they connect
  std::shared_ptr<std::atomic_bool> active_;
  std::shared_ptr<ActiveContainers> active_containers_;
  std::unique_ptr<rpc::RPCService> rpc_;
  PredictionCache cache_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::mutex inflight_messages_mutex_;
  std::unordered_map<int, std::vector<InflightMessage>> inflight_messages_;
  std::shared_ptr<metrics::Counter> predictions_counter;
  std::shared_ptr<metrics::Meter> throughput_meter;
  std::shared_ptr<metrics::Histogram> latency_hist;
  std::unordered_map<const VersionedModelId, ModelQueue,
                     decltype(&versioned_model_hash)>
      model_queues_;
  static constexpr int INITIAL_MODEL_QUEUES_MAP_SIZE = 100;

  bool create_model_queue_if_necessary(const VersionedModelId &model_id) {
    // Adds a new <model_id, task_queue> entry to the queues map, if one
    // does not already exist
    auto queue_added = model_queues_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(model_id),
                                             std::forward_as_tuple());
    return queue_added.second;
  }

  void on_container_ready(VersionedModelId model_id, int replica_id) {
    std::shared_ptr<ModelContainer> container =
        active_containers_->get_model_replica(model_id, replica_id);
    if (!container) {
      throw std::runtime_error(
          "TaskExecutor failed to find previously registered active "
          "container!");
    }
    auto model_queue_entry = model_queues_.find(container->model_);
    if (model_queue_entry == model_queues_.end()) {
      throw std::runtime_error(
          "Failed to find model queue associated with a previously registered "
          "container!");
    }

    boost::optional<Deadline> earliest_deadline =
        model_queue_entry->second.get_earliest_deadline();
    if (earliest_deadline) {
      size_t batch_size = container->get_batch_size(earliest_deadline.get());
      auto batch = model_queue_entry->second.get_batch(batch_size);
      if (batch.size() > 0) {
        // move the lock up here, so that nothing can pull from the
        // inflight_messages_
        // map between the time a message is sent and when it gets inserted
        // into the map
        std::unique_lock<std::mutex> l(inflight_messages_mutex_);
        std::vector<InflightMessage> cur_batch;
        rpc::PredictionRequest prediction_request(container->input_type_);
        for (auto b : batch) {
          prediction_request.add_input(b.input_);
          cur_batch.emplace_back(b.recv_time_, container->container_id_,
                                 b.model_, b.input_);
        }
        int message_id = rpc_->send_message(prediction_request.serialize(),
                                            container->container_id_);
        inflight_messages_.emplace(message_id, std::move(cur_batch));
        return;
      }
    }

    TaskExecutionThreadPool::submit_job(
        [ this, model_id, replica_id, task_executor_valid = active_ ]() {
          if (*task_executor_valid) {
            on_container_ready(model_id, replica_id);
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_container_ready callback because "
                     "TaskExecutor has been destroyed.");
          }
        });
  }

  void on_response_recv(rpc::RPCResponse response) {
    std::unique_lock<std::mutex> l(inflight_messages_mutex_);
    auto keys = inflight_messages_[response.first];

    inflight_messages_.erase(response.first);
    std::vector<float> deserialized_outputs =
        deserialize_outputs(response.second);
    assert(deserialized_outputs.size() == keys.size());
    int batch_size = keys.size();
    predictions_counter->increment(batch_size);
    throughput_meter->mark(batch_size);
    std::chrono::time_point<std::chrono::system_clock> current_time =
        std::chrono::system_clock::now();
    for (int batch_num = 0; batch_num < batch_size; ++batch_num) {
      InflightMessage completed_msg = keys[batch_num];
      float deserialized_output = deserialized_outputs[batch_num];
      process_completed_message(completed_msg, deserialized_output,
                                current_time);
    }
  }

  void process_completed_message(
      InflightMessage &completed_msg, float deserialized_output,
      std::chrono::time_point<std::chrono::system_clock> &current_time) {
    std::shared_ptr<ModelContainer> processing_container =
        active_containers_->get_model_replica(completed_msg.model_,
                                              completed_msg.container_id_);
    auto task_latency = current_time - completed_msg.send_time_;
    long task_latency_micros =
        std::chrono::duration_cast<std::chrono::microseconds>(task_latency)
            .count();
    long task_latency_millis =
        std::chrono::duration_cast<std::chrono::milliseconds>(task_latency)
            .count();
    processing_container->update_throughput(1, task_latency_micros);
    latency_hist->insert(static_cast<int64_t>(task_latency_millis));
    cache_.put(completed_msg.model_, completed_msg.input_,
               Output{deserialized_output, {completed_msg.model_}});
  }
};

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
