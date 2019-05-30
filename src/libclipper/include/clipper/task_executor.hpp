#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <boost/optional.hpp>

#include <redox.hpp>

#include <folly/futures/Future.h>

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

class ModelMetrics {
 public:
  explicit ModelMetrics(VersionedModelId model)
      : model_(model),
        latency_(metrics::MetricsRegistry::get_metrics().create_histogram(
            "model:" + model.serialize() + ":prediction_latency",
            "microseconds", 4096)),
        throughput_(metrics::MetricsRegistry::get_metrics().create_meter(
            "model:" + model.serialize() + ":prediction_throughput")),
        num_predictions_(metrics::MetricsRegistry::get_metrics().create_counter(
            "model:" + model.serialize() + ":num_predictions")),
        cache_hit_ratio_(
            metrics::MetricsRegistry::get_metrics().create_ratio_counter(
                "model:" + model.serialize() + ":cache_hit_ratio")),
        batch_size_(metrics::MetricsRegistry::get_metrics().create_histogram(
            "model:" + model.serialize() + ":batch_size", "queries", 4096)) {}
  ~ModelMetrics() = default;
  ModelMetrics(const ModelMetrics &) = default;
  ModelMetrics &operator=(const ModelMetrics &) = default;

  ModelMetrics(ModelMetrics &&) = default;
  ModelMetrics &operator=(ModelMetrics &&) = default;

  void invalidate() {
    metrics::MetricsRegistry::get_metrics().delete_metric(latency_);
    metrics::MetricsRegistry::get_metrics().delete_metric(throughput_);
    metrics::MetricsRegistry::get_metrics().delete_metric(num_predictions_);
    metrics::MetricsRegistry::get_metrics().delete_metric(cache_hit_ratio_);
    metrics::MetricsRegistry::get_metrics().delete_metric(batch_size_);
  }

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
  bool used_ = true;
  Output value_;
  std::vector<folly::Promise<Output>> value_promises_;
};

// A cache page is a pair of <hash, entry_size>
using CachePage = std::pair<long, long>;

class PredictionCache {
 public:
  PredictionCache(size_t size_bytes);
  folly::Future<Output> fetch(const VersionedModelId &model,
                              std::shared_ptr<PredictionData> &input);

  void put(const VersionedModelId &model,
           std::shared_ptr<PredictionData> &input, const Output &output);

 private:
  size_t hash(const VersionedModelId &model, size_t input_hash) const;
  void insert_entry(const long key, CacheEntry &value);
  void evict_entries(long space_needed_bytes);

  std::mutex m_;
  const size_t max_size_bytes_;
  size_t size_bytes_ = 0;
  // TODO cache needs a promise as well?
  std::unordered_map<long, CacheEntry> entries_;
  std::vector<long> page_buffer_;
  size_t page_buffer_index_ = 0;
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
  ModelQueue() : queue_(ModelPQueue{}), valid_(true) {}

  // Disallow copy and assign
  ModelQueue(const ModelQueue &) = delete;
  ModelQueue &operator=(const ModelQueue &) = delete;

  ModelQueue(ModelQueue &&) = default;
  ModelQueue &operator=(ModelQueue &&) = default;

  ~ModelQueue() = default;

  void add_task(PredictTask task) {
    if (!valid_) return;
    std::lock_guard<std::mutex> lock(queue_mutex_);
    Deadline deadline = std::chrono::system_clock::now() +
                        std::chrono::microseconds(task.latency_slo_micros_);
    queue_.emplace(deadline, std::move(task));
    queue_not_empty_condition_.notify_one();
  }

  int get_size() {
    std::unique_lock<std::mutex> l(queue_mutex_);
    return queue_.size();
  }

  std::vector<PredictTask> get_batch(
      std::shared_ptr<ModelContainer> requesting_container,
      std::function<BatchSizeInfo(Deadline)> &&get_batch_size) {
    std::unique_lock<std::mutex> lock(queue_mutex_);
    remove_tasks_with_elapsed_deadlines();
    queue_not_empty_condition_.wait(lock, [this, requesting_container]() {
      return !valid_ ||
             !queue_.empty() ||
             !get_container_registration(requesting_container);
      });
    remove_tasks_with_elapsed_deadlines();

    std::vector<PredictTask> batch;
    if (requesting_container->is_active() && valid_) {
      Deadline deadline = queue_.top().first;

      size_t max_batch_size;
      BatchSizeDeterminationMethod method;
      std::tie(max_batch_size, method) = get_batch_size(deadline);

      while (batch.size() < max_batch_size && queue_.size() > 0) {
        batch.push_back(queue_.top().second);
        queue_.pop();
      }
      size_t batch_differential = max_batch_size - batch.size();
      if (batch_differential > 0 &&
          method == BatchSizeDeterminationMethod::Exploration) {
        // Artificially inject queries to create
        // a full batch
        //
        // Create a copy of the last task in the batch and make it artifical
        PredictTask last_task = batch.back();
        last_task.artificial_ = true;

        std::fill_n(std::back_inserter(batch), batch_differential, last_task);
      }
    }

    lock.unlock();
    if (!queue_.empty()) {
      queue_not_empty_condition_.notify_one();
    }
    return batch;
  }

  void invalidate() {
    std::lock_guard<std::mutex> l(queue_mutex_);
    valid_ = false;
    queue_ = ModelPQueue();
    queue_not_empty_condition_.notify_all();
  }

  void wakeup_all_forcely() {
    std::lock_guard<std::mutex> l(queue_mutex_);
    queue_not_empty_condition_.notify_all();
  }

  void register_container(const VersionedModelId &model_id, int model_replica_id) {
    boost::unique_lock<boost::shared_mutex>l(registered_containers_mutex_);
    registered_containers_.push_back(std::make_pair(model_id, model_replica_id));
  }

  void unregister_container(const VersionedModelId &model_id, int model_replica_id) {
    boost::unique_lock<boost::shared_mutex> l(registered_containers_mutex_);
    auto it = find(registered_containers_.begin(), registered_containers_.end(),
      std::make_pair(model_id, model_replica_id));
    if (it != registered_containers_.end()) {
      registered_containers_.erase(it);
    }
  }

  bool get_container_registration(std::shared_ptr<ModelContainer> requesting_container) {
    if (requesting_container) {
      boost::shared_lock<boost::shared_mutex> l(registered_containers_mutex_);
      auto it = find(registered_containers_.begin(), registered_containers_.end(),
        std::make_pair(requesting_container->model_, requesting_container->replica_id_));
      if (it != registered_containers_.end()) {
        return true;
      }
    }
    return false;
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
  std::condition_variable queue_not_empty_condition_;
  bool valid_;
  boost::shared_mutex registered_containers_mutex_;
  // <VersionedModelId, replica_id(int)>
  std::vector<std::pair<VersionedModelId, int>> registered_containers_;

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
      const int replica_id, const std::shared_ptr<PredictionData> input,
      const bool discard_result)
      : send_time_(std::move(send_time)),
        container_id_(container_id),
        model_(std::move(model)),
        replica_id_(replica_id),
        input_(std::move(input)),
        discard_result_(discard_result) {}

  // Default copy and move constructors
  InflightMessage(const InflightMessage &) = default;
  InflightMessage(InflightMessage &&) = default;

  // Default assignment operators
  InflightMessage &operator=(const InflightMessage &) = default;
  InflightMessage &operator=(InflightMessage &&) = default;

  std::chrono::time_point<std::chrono::system_clock> send_time_;
  int container_id_;
  VersionedModelId model_;
  int replica_id_;
  std::shared_ptr<PredictionData> input_;
  bool discard_result_;
};

class TaskExecutor {
 public:
  ~TaskExecutor() { active_->store(false); };
  explicit TaskExecutor()
      : active_(std::make_shared<std::atomic_bool>(true)),
        active_containers_(std::make_shared<ActiveContainers>()),
        rpc_(std::make_unique<rpc::RPCService>()),
        cache_(std::make_unique<PredictionCache>(
            get_config().get_prediction_cache_size())),
        model_queues_({}),
        model_metrics_({}) {
    log_info(LOGGING_TAG_TASK_EXECUTOR, "TaskExecutor started");
    Config &conf = get_config();
    rpc_->start(
        "*", conf.get_rpc_service_port(), [ this, task_executor_valid = active_ ](
                                   VersionedModelId model, int replica_id) {
          if (*task_executor_valid) {
            on_container_ready(model, replica_id);
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_container_ready callback because "
                     "TaskExecutor has been destroyed.");
          }
        },
        [ this, task_executor_valid = active_ ](rpc::RPCResponse & response) {
          if (*task_executor_valid) {
            on_response_recv(std::move(response));
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_response_recv callback because "
                     "TaskExecutor has been destroyed.");
          }

        },
        [ this, task_executor_valid = active_ ](VersionedModelId model,
                                                int replica_id) {
          if (*task_executor_valid) {
            on_remove_container(model, replica_id);
          } else {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running on_remove_container callback because "
                     "TaskExecutor has been destroyed.");
          }
        });
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

    redis::subscribe_to_model_changes(redis_subscriber_, [
      this, task_executor_valid = active_
    ](const std::string &key, const std::string &event_type) {
      if (event_type == "hset" && *task_executor_valid) {
        auto model_info =
            clipper::redis::get_model_by_key(redis_connection_, key);
        VersionedModelId model_id = VersionedModelId(
            model_info["model_name"], model_info["model_version"]);
        int batch_size = DEFAULT_BATCH_SIZE;
        auto batch_size_search = model_info.find("batch_size");
        if (batch_size_search != model_info.end()) {
          batch_size = std::stoi(model_info["batch_size"]);
        }
        log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                           "Registered batch size of {} for model {}:{}",
                           batch_size, model_id.get_name(), model_id.get_id());
        active_containers_->register_batch_size(model_id, batch_size);

      } else if (event_type == "hdel" && *task_executor_valid) {
        std::vector<VersionedModelId> parsed_model_info = clipper::redis::str_to_models(key);
        VersionedModelId model_id = parsed_model_info.front();
        auto container_list = clipper::redis::get_all_containers(redis_connection_);

        // clean up containers and interrupt working threads
        for (auto c : container_list) {
          VersionedModelId vm = std::get<0>(c);
          int model_replica_id = std::get<1>(c);
          if (vm == model_id) {
            clean_up_specific_container(vm, model_replica_id);
            TaskExecutionThreadPool::interrupt_thread(vm, model_replica_id);
          }
        }

        // Clean up versioned model
        clean_up_specific_model(model_id);

        // Clean up working threads
        for (auto c : container_list) {
          VersionedModelId vm = std::get<0>(c);
          int model_replica_id = std::get<1>(c);
          if (vm == model_id) {
            TaskExecutionThreadPool::delete_queue(vm, model_replica_id);
            EstimatorFittingThreadPool::delete_queue(vm, model_replica_id);
          }
        }

      } else if (!*task_executor_valid) {
        log_info(LOGGING_TAG_TASK_EXECUTOR,
                  "Not running TaskExecutor's "
                  "subscribe_to_model_changes callback because "
                  "TaskExecutor has been destroyed.");
      }
    });

    redis::subscribe_to_container_changes(
        redis_subscriber_,
        // event_type corresponds to one of the Redis event types
        // documented in https://redis.io/topics/notifications.
        [ this, task_executor_valid = active_ ](const std::string &key,
                                                const std::string &event_type) {
          if (event_type == "hset" && *task_executor_valid) {
            auto container_info =
                redis::get_container_by_key(redis_connection_, key);
            VersionedModelId vm = VersionedModelId(
                container_info["model_name"], container_info["model_version"]);
            int replica_id = std::stoi(container_info["model_replica_id"]);
            active_containers_->add_container(
                vm, std::stoi(container_info["zmq_connection_id"]), replica_id,
                parse_input_type(container_info["input_type"]));

            auto model_info = redis::get_model(redis_connection_, vm);

            int batch_size = DEFAULT_BATCH_SIZE;
            auto batch_size_search = model_info.find("batch_size");
            if (batch_size_search != model_info.end()) {
              batch_size = std::stoi(batch_size_search->second);
            }
            active_containers_->register_batch_size(vm, batch_size);

            bool created_queue = create_model_queue_if_necessary(vm);
            if (created_queue) {
              log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                                 "Created queue for new model: {} : {}",
                                 vm.get_name(), vm.get_id());
            }
            register_container_to_model_queue(vm, replica_id);

            EstimatorFittingThreadPool::create_queue(vm, replica_id);
            TaskExecutionThreadPool::create_queue(vm, replica_id);
            TaskExecutionThreadPool::submit_job(
                vm, replica_id, [this, vm, replica_id]() {
                  on_container_ready(vm, replica_id);
                });

          } else if (!*task_executor_valid) {
            log_info(LOGGING_TAG_TASK_EXECUTOR,
                     "Not running TaskExecutor's "
                     "subscribe_to_container_changes callback because "
                     "TaskExecutor has been destroyed.");
          }
        });
    throughput_meter_ = metrics::MetricsRegistry::get_metrics().create_meter(
        "internal:aggregate_model_throughput");
    predictions_counter_ =
        metrics::MetricsRegistry::get_metrics().create_counter(
            "internal:aggregate_num_predictions");
  }

  // Disallow copy
  TaskExecutor(const TaskExecutor &other) = delete;
  TaskExecutor &operator=(const TaskExecutor &other) = delete;

  TaskExecutor(TaskExecutor &&other) = default;
  TaskExecutor &operator=(TaskExecutor &&other) = default;

  std::vector<folly::Future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    predictions_counter_->increment(tasks.size());
    std::vector<folly::Future<Output>> output_futures;
    for (auto t : tasks) {
      // add each task to the queue corresponding to its associated model
      boost::shared_lock<boost::shared_mutex> lock(model_queues_mutex_);
      auto model_queue_entry = model_queues_.find(t.model_);

      if (model_queue_entry != model_queues_.end()) {
        auto cache_result = cache_->fetch(t.model_, t.input_);

        if (cache_result.isReady()) {
          output_futures.push_back(std::move(cache_result));
          boost::shared_lock<boost::shared_mutex> model_metrics_lock(
              model_metrics_mutex_);
          auto cur_model_metric_entry = model_metrics_.find(t.model_);
          if (cur_model_metric_entry != model_metrics_.end()) {
            auto cur_model_metric = cur_model_metric_entry->second;
            cur_model_metric.cache_hit_ratio_->increment(1, 1);
          }
        }

        else if (active_containers_->get_replicas_for_model(t.model_).size() ==
                 0) {
          log_error_formatted(LOGGING_TAG_TASK_EXECUTOR,
                              "No active model containers for model: {} : {}",
                              t.model_.get_name(), t.model_.get_id());
        } else {
          output_futures.push_back(std::move(cache_result));
          t.recv_time_ = std::chrono::system_clock::now();
          model_queue_entry->second->add_task(t);
          log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                             "Adding task to queue. QueryID: {}, model: {}",
                             t.query_id_, t.model_.serialize());
          boost::shared_lock<boost::shared_mutex> model_metrics_lock(
              model_metrics_mutex_);
          auto cur_model_metric_entry = model_metrics_.find(t.model_);
          if (cur_model_metric_entry != model_metrics_.end()) {
            auto cur_model_metric = cur_model_metric_entry->second;
            cur_model_metric.cache_hit_ratio_->increment(0, 1);
          }
        }
      } else {
        log_error_formatted(LOGGING_TAG_TASK_EXECUTOR,
                            "Received task for unknown model: {} : {}",
                            t.model_.get_name(), t.model_.get_id());
      }
    }
    return output_futures;
  }

  std::vector<folly::Future<FeedbackAck>> schedule_feedback(
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
  std::unique_ptr<PredictionCache> cache_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  std::mutex inflight_messages_mutex_;
  std::unordered_map<int, std::vector<InflightMessage>> inflight_messages_;
  std::shared_ptr<metrics::Counter> predictions_counter_;
  std::shared_ptr<metrics::Meter> throughput_meter_;
  boost::shared_mutex model_queues_mutex_;
  std::unordered_map<VersionedModelId, std::shared_ptr<ModelQueue>>
      model_queues_;
  boost::shared_mutex model_metrics_mutex_;
  std::unordered_map<VersionedModelId, ModelMetrics> model_metrics_;

  static constexpr int INITIAL_MODEL_QUEUES_MAP_SIZE = 100;

  bool create_model_queue_if_necessary(const VersionedModelId &model_id) {
    // Adds a new <model_id, task_queue> entry to the queues map, if one
    // does not already exist
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    auto queue_added = model_queues_.emplace(
        std::make_pair(model_id, std::make_shared<ModelQueue>()));
    bool queue_created = queue_added.second;
    if (queue_created) {
      boost::unique_lock<boost::shared_mutex> l(model_metrics_mutex_);
      model_metrics_.insert(std::make_pair(model_id, ModelMetrics(model_id)));
      // model_metrics_.emplace(std::piecewise_construct,
      //                        std::forward_as_tuple(model_id),
      //                        std::forward_as_tuple(model_id));
    }
    return queue_created;
  }

  bool wakeup_model_queue_if_necessary(const VersionedModelId &model_id) {
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    if (model_queues_.count(model_id)) {
      model_queues_[model_id]->wakeup_all_forcely();
      return true;
    }
    return false;
  }

  bool delete_model_metric_if_necessary(const VersionedModelId &model_id) {
    // Deletes an entry from the metric map, if one exists
    boost::unique_lock<boost::shared_mutex> l(model_metrics_mutex_);
    auto cur_model_metric_entry = model_metrics_.find(model_id);
    if (cur_model_metric_entry != model_metrics_.end()) {
      auto cur_model_metric = cur_model_metric_entry->second;
      cur_model_metric.invalidate();
      model_metrics_.erase(model_id);
      return true;
    }
    return false;
  }

  bool delete_model_queue_if_necessary(const VersionedModelId &model_id) {
    // Deletes an entry from the queues map, if one exists
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    if (model_queues_.count(model_id)) {
      model_queues_[model_id]->invalidate();
      model_queues_.erase(model_id);
      return true;
    }
    return false;
  }

  void register_container_to_model_queue(const VersionedModelId &model_id, int replica_id) {
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    auto model_queue_entry = model_queues_.find(model_id);
    if (model_queue_entry == model_queues_.end()) {
      std::stringstream ss;
      ss << "Error registering container to model queue. name "
         << "'" << model_id.get_name() << "'"
         << " and version "
         << "'" << model_id.get_id() << "'";
      throw std::runtime_error(ss.str());
    }
    model_queue_entry->second->register_container(model_id, replica_id);
  }

  void unregister_container_from_model_queue(const VersionedModelId &model_id, int replica_id) {
    boost::unique_lock<boost::shared_mutex> l(model_queues_mutex_);
    auto model_queue_entry = model_queues_.find(model_id);
    if (model_queue_entry == model_queues_.end()) {
      std::stringstream ss;
      ss << "Error unregistering container to model queue. name "
         << "'" << model_id.get_name() << "'"
         << " and version "
         << "'" << model_id.get_id() << "'";
      throw std::runtime_error(ss.str());
    }
    model_queue_entry->second->unregister_container(model_id, replica_id);
  }

  void clean_up_specific_container(const VersionedModelId &model_id, int replica_id) {
    if (clipper::redis::delete_container(redis_connection_, model_id, replica_id)) {
      std::stringstream ss;
      ss << "Successfully deleted container with name "
         << "'" << model_id.get_name() << "'"
         << " and version "
         << "'" << model_id.get_id() << "'";
      log_info_formatted(LOGGING_TAG_TASK_EXECUTOR, "{}", ss.str());

      active_containers_->remove_container(model_id, replica_id);
      unregister_container_from_model_queue(model_id, replica_id);
    } else {
      std::stringstream ss;
      ss << "Error deleting container with name "
         << "'" << model_id.get_name() << "'"
         << " and version "
         << "'" << model_id.get_id() << "'"
         << " from Redis";
      throw std::runtime_error(ss.str());
    }
  }

  void clean_up_specific_model(const VersionedModelId &model_id) {
    if (clipper::redis::delete_versioned_model(redis_connection_, model_id)) {
      std::stringstream ss;
      ss << "Successfully deleted versioned model with name "
         << "'" << model_id.get_name() << "'";
      log_info_formatted(LOGGING_TAG_TASK_EXECUTOR, "{}", ss.str());

      if (delete_model_metric_if_necessary(model_id)) {
        log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                           "Deleted metric for model: {} : {}",
                           model_id.get_name(), model_id.get_id());
      }

      if (delete_model_queue_if_necessary(model_id)) {
        log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                           "Deleted queue for model: {} : {}",
                           model_id.get_name(), model_id.get_id());
      }

      active_containers_->unregister_batch_size(model_id);

    } else {
      std::stringstream ss;
      ss << "Error deleting versioned model with name "
         << "'" << model_id.get_name() << "'"
         << " from Redis";
      throw std::runtime_error(ss.str());
    }
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
    std::shared_ptr<ModelQueue> current_model_queue = model_queue_entry->second;
    // NOTE: It is safe to unlock here because we copy the shared_ptr to
    // the ModelQueue object so even if that entry in the map gets deleted,
    // the ModelQueue object won't be destroyed until our copy of the pointer
    // goes out of scope.
    l.unlock();

    std::vector<PredictTask> batch = current_model_queue->get_batch(
        container, [container](Deadline deadline) {
          return container->get_batch_size(deadline);
        });

    if (batch.size() > 0) {
      // move the lock up here, so that nothing can pull from the
      // inflight_messages_
      // map between the time a message is sent and when it gets inserted
      // into the map
      std::unique_lock<std::mutex> l(inflight_messages_mutex_);
      std::vector<InflightMessage> cur_batch;
      rpc::PredictionRequest prediction_request(container->input_type_);
      std::stringstream query_ids_in_batch;
      std::chrono::time_point<std::chrono::system_clock> current_time =
          std::chrono::system_clock::now();
      for (auto b : batch) {
        prediction_request.add_input(b.input_);
        cur_batch.emplace_back(current_time, container->container_id_, b.model_,
                               container->replica_id_, b.input_, b.artificial_);
        query_ids_in_batch << b.query_id_ << " ";
      }
      int message_id = rpc_->send_message(prediction_request.serialize(),
                                          container->container_id_);
      log_info_formatted(LOGGING_TAG_TASK_EXECUTOR,
                         "Sending batch to model: {} replica {}."
                         "Batch size: {}. Query IDs: {}",
                         model_id.serialize(), std::to_string(replica_id),
                         std::to_string(batch.size()),
                         query_ids_in_batch.str());
      inflight_messages_.emplace(message_id, std::move(cur_batch));

    } else {
      log_error_formatted(
          LOGGING_TAG_TASK_EXECUTOR,
          "ModelQueue returned empty batch for model {}, replica {}",
          model_id.serialize(), std::to_string(replica_id));
    }
  }

  void on_response_recv(rpc::RPCResponse response) {
    std::unique_lock<std::mutex> l(inflight_messages_mutex_);
    auto keys = inflight_messages_[response.first];
    boost::shared_lock<boost::shared_mutex> metrics_lock(model_metrics_mutex_);

    inflight_messages_.erase(response.first);
    rpc::PredictionResponse parsed_response =
        rpc::PredictionResponse::deserialize_prediction_response(
            std::move(response.second));
    assert(parsed_response.outputs_.size() == keys.size());
    size_t batch_size = keys.size();
    throughput_meter_->mark(batch_size);
    std::chrono::time_point<std::chrono::system_clock> current_time =
        std::chrono::system_clock::now();
    if (batch_size > 0) {
      InflightMessage &first_message = keys[0];
      const VersionedModelId &cur_model = first_message.model_;
      const int cur_replica_id = first_message.replica_id_;
      auto batch_latency = current_time - first_message.send_time_;
      long long batch_latency_micros =
          std::chrono::duration_cast<std::chrono::microseconds>(batch_latency)
              .count();

      // Because an RPCResponse is guaranteed to contain data received from
      // a single model container, the processing container for the first
      // InflightMessage in the batch is the same processing container
      // for all InflightMessage objects in the batch
      std::shared_ptr<ModelContainer> processing_container =
          active_containers_->get_model_replica(cur_model, cur_replica_id);

      processing_container->add_processing_datapoint(batch_size,
                                                     batch_latency_micros);

      boost::optional<ModelMetrics> cur_model_metric;
      auto cur_model_metric_entry = model_metrics_.find(cur_model);
      if (cur_model_metric_entry != model_metrics_.end()) {
        cur_model_metric = cur_model_metric_entry->second;
      }
      if (cur_model_metric) {
        (*cur_model_metric).throughput_->mark(batch_size);
        (*cur_model_metric).num_predictions_->increment(batch_size);
        (*cur_model_metric).batch_size_->insert(batch_size);
      }
      for (size_t batch_num = 0; batch_num < batch_size; ++batch_num) {
        InflightMessage completed_msg = keys[batch_num];
        if (!completed_msg.discard_result_) {
          cache_->put(completed_msg.model_, completed_msg.input_,
                      Output{parsed_response.outputs_[batch_num],
                             {completed_msg.model_}});
        }
        auto task_latency = current_time - completed_msg.send_time_;
        long task_latency_micros =
            std::chrono::duration_cast<std::chrono::microseconds>(task_latency)
                .count();
        if (cur_model_metric) {
          (*cur_model_metric)
              .latency_->insert(static_cast<int64_t>(task_latency_micros));
        }
      }
    }
  }

  void on_remove_container(VersionedModelId model_id, int replica_id) {
    std::shared_ptr<ModelContainer> container =
        active_containers_->get_model_replica(model_id, replica_id);
    if (!container)
      return;

    clean_up_specific_container(model_id, replica_id);
    TaskExecutionThreadPool::interrupt_thread(model_id, replica_id);

    wakeup_model_queue_if_necessary(model_id);

    TaskExecutionThreadPool::delete_queue(model_id, replica_id);
    EstimatorFittingThreadPool::delete_queue(model_id, replica_id);
  }
};

}  // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
