#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

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

class ModelMetrics {
 public:
  explicit ModelMetrics(VersionedModelId model)
      : model_(model),
        latency_(metrics::MetricsRegistry::get_metrics().create_histogram(
            versioned_model_to_str(model) + ":prediction_latency",
            "microseconds", 4096)),
        throughput_(metrics::MetricsRegistry::get_metrics().create_meter(
            versioned_model_to_str(model) + ":prediction_throughput")),
        num_predictions_(metrics::MetricsRegistry::get_metrics().create_counter(
            versioned_model_to_str(model) + ":num_predictions")),
        cache_hit_ratio_(
            metrics::MetricsRegistry::get_metrics().create_ratio_counter(
                versioned_model_to_str(model) + ":cache_hit_ratio")),
        batch_size_(metrics::MetricsRegistry::get_metrics().create_histogram(
            versioned_model_to_str(model) + ":batch_size", "queries", 4096)) {}
  ~ModelMetrics() = default;
  ModelMetrics(const ModelMetrics &) = default;
  ModelMetrics &operator=(const ModelMetrics &) = default;

  ModelMetrics(ModelMetrics &&) = default;
  ModelMetrics &operator=(ModelMetrics &&) = default;

  VersionedModelId model_;
  std::shared_ptr<metrics::Histogram> latency_;
  std::shared_ptr<metrics::Meter> throughput_;
  std::shared_ptr<metrics::Counter> num_predictions_;
  std::shared_ptr<metrics::RatioCounter> cache_hit_ratio_;
  std::shared_ptr<metrics::Histogram> batch_size_;
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
    Deadline deadline = std::chrono::high_resolution_clock::now() +
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
    if (queue_.size() == 0) {
      return boost::optional<Deadline>{};
    } else {
      Deadline earliest_deadline = queue_.top().first;
      return boost::optional<Deadline>(earliest_deadline);
    }
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
    throughput_meter_ = metrics::MetricsRegistry::get_metrics().create_meter(
        "internal:aggregate_model_throughput");
    predictions_counter =
        metrics::MetricsRegistry::get_metrics().create_counter(
            "internal:aggregate_num_predictions");
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
          t.send_time_micros_ =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
          model_queue_entry->second.add_task(t);
          boost::shared_lock<boost::shared_mutex> model_metrics_lock(
              model_metrics_mutex_);
          model_metrics_[t.model_].cache_hit_ratio_->increment(0, 1);
        } else {
          boost::shared_lock<boost::shared_mutex> model_metrics_lock(
              model_metrics_mutex_);
          model_metrics_[t.model_].cache_hit_ratio_->increment(1, 1);
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
  // inflight_messages_ value is vector of <message_id, model, input>
  std::unordered_map<int, std::vector<std::tuple<const long, VersionedModelId,
                                                 std::shared_ptr<Input>>>>
      inflight_messages_;
  std::shared_ptr<metrics::Counter> predictions_counter;
  std::shared_ptr<metrics::Meter> throughput_meter_;
  boost::shared_mutex model_queues_mutex_;
  std::unordered_map<const VersionedModelId, ModelQueue,
                     decltype(&versioned_model_hash)>
      model_queues_;
  boost::shared_mutex model_metrics_mutex_;
  std::unordered_map<const VersionedModelId, ModelMetrics,
                     decltype(&versioned_model_hash)>
      model_metrics_;
  static constexpr int INITIAL_MODEL_QUEUES_MAP_SIZE = 100;

  bool create_model_queue_if_necessary(const VersionedModelId &model_id) {
    // Adds a new <model_id, task_queue> entry to the queues map, if one
    // does not already exist
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    auto queue_added = model_queues_.emplace(std::piecewise_construct,
                                             std::forward_as_tuple(model_id),
                                             std::forward_as_tuple());
    bool queue_created = queue_added.second;
    if (queue_created) {
      boost::unique_lock<boost::shared_mutex> l(model_metrics_mutex_);
      model_metrics_.emplace(std::piecewise_construct,
                             std::forward_as_tuple(model_id),
                             std::forward_as_tuple(model_id));
    }
    return queue_created;
  }

  void on_container_ready(VersionedModelId model_id, int replica_id) {
    std::shared_ptr<ModelContainer> container =
        active_containers_->get_model_replica(model_id, replica_id);
    if (!container) {
      throw std::runtime_error(
          "TaskExecutor failed to find previously registered active "
          "container!");
    }
    boost::shared_lock<boost::shared_mutex> l(model_queues_mutex_);
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
        std::vector<
            std::tuple<const long, VersionedModelId, std::shared_ptr<Input>>>
            cur_batch;
        rpc::PredictionRequest prediction_request(container->input_type_);
        for (auto b : batch) {
          prediction_request.add_input(b.input_);
          cur_batch.emplace_back(b.send_time_micros_, b.model_, b.input_);
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
    boost::shared_lock<boost::shared_mutex> metrics_lock(model_metrics_mutex_);

    inflight_messages_.erase(response.first);
    std::vector<float> deserialized_outputs =
        deserialize_outputs(response.second);
    assert(deserialized_outputs.size() == keys.size());
    int batch_size = keys.size();
    predictions_counter->increment(batch_size);
    throughput_meter_->mark(batch_size);
    long current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    if (keys.size() > 0) {
      const VersionedModelId &cur_model = std::get<1>(keys[0]);
      auto cur_model_metric = model_metrics_[cur_model];
      cur_model_metric.throughput_->mark(keys.size());
      cur_model_metric.num_predictions_->increment(keys.size());
      cur_model_metric.batch_size_->insert(keys.size());
      for (int batch_num = 0; batch_num < batch_size; ++batch_num) {
        long send_time = std::get<0>(keys[batch_num]);
        cur_model_metric.latency_->insert(
            static_cast<int64_t>(current_time - send_time));
        cache_.put(std::get<1>(keys[batch_num]), std::get<2>(keys[batch_num]),
                   Output{deserialized_outputs[batch_num],
                          {std::get<1>(keys[batch_num])}});
      }
    }
  }
};

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
