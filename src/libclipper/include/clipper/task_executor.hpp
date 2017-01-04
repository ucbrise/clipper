#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include <boost/thread.hpp>
#include <redox.hpp>

#include <clipper/constants.hpp>
#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/redis.hpp>
#include <clipper/rpc_service.hpp>
#include <clipper/util.hpp>

namespace clipper {

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
  ~TaskExecutor() { active_ = false; };
  explicit TaskExecutor()
      : active_containers_(std::make_shared<ActiveContainers>()),
        rpc_(std::make_unique<rpc::RPCService>()) {
    std::cout << "TaskExecutor started" << std::endl;
    rpc_->start("*", RPC_SERVICE_PORT);
    active_ = true;
    while (!redis_connection_.connect(REDIS_IP, REDIS_PORT)) {
      std::cout << "ERROR connecting to Redis" << std::endl;
      std::cout << "Sleeping 1 second..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    while (!redis_subscriber_.connect(REDIS_IP, REDIS_PORT)) {
      std::cout << "ERROR connecting to Redis" << std::endl;
      std::cout << "Sleeping 1 second..." << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    redis::send_cmd_no_reply<std::string>(
        redis_connection_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
    redis::subscribe_to_container_changes(
        redis_subscriber_,
        // event_type corresponds to one of the Redis event types
        // documented in https://redis.io/topics/notifications.
        [this](const std::string &key, const std::string &event_type) {
          if (event_type == "hset") {
            auto container_info =
                redis::get_container_by_key(redis_connection_, key);
            VersionedModelId vm =
                std::make_pair(container_info["model_name"],
                               std::stoi(container_info["model_version"]));
            active_containers_->add_container(
                vm, std::stoi(container_info["zmq_connection_id"]),
                parse_input_type(container_info["input_type"]));
          }

        });
    boost::thread(&TaskExecutor::send_messages, this).detach();
    boost::thread(&TaskExecutor::recv_messages, this).detach();
  }

  // Disallow copy
  TaskExecutor(const TaskExecutor &other) = delete;
  TaskExecutor &operator=(const TaskExecutor &other) = delete;

  TaskExecutor(TaskExecutor &&other) = default;
  TaskExecutor &operator=(TaskExecutor &&other) = default;

  std::vector<boost::shared_future<Output>> schedule_predictions(
      std::vector<PredictTask> tasks) {
    std::vector<boost::shared_future<Output>> output_futures;
    for (auto t : tasks) {
      // assign tasks to containers independently
      auto replicas = active_containers_->get_model_replicas_snapshot(t.model_);
      if (replicas.size() > 0) {
        std::shared_ptr<ModelContainer> container =
            scheduler_.assign_container(t, replicas);
        container->send_prediction(t);
        output_futures.push_back(std::move(cache_.fetch(t.model_, t.input_)));
      } else {
        std::cout << "No active containers found for model " << t.model_.first
                  << ":" << t.model_.second << std::endl;
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
  // active_containers_ is shared with the RPC service so it can add new
  // containers
  // to the collection when they connect
  std::shared_ptr<ActiveContainers> active_containers_;
  std::unique_ptr<rpc::RPCService> rpc_;
  Scheduler scheduler_;
  PredictionCache cache_;
  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;
  bool active_ = false;
  std::mutex inflight_messages_mutex_;
  std::unordered_map<
      int, std::vector<std::pair<VersionedModelId, std::shared_ptr<Input>>>>
      inflight_messages_;

  /// TODO: The task executor needs executors to schedule tasks (assign
  /// tasks to containers), batch and serialize tasks to be sent via the
  /// RPC service by dequeing from individual container queues, and receiving
  /// responses from the RPC service and putting the results in the prediction
  /// cache. Finally, when results are placed in the prediction
  /// cache and a promise is completed, we need to determine what thread any
  /// continuations attached to the corresponding future will be executed on.
  /// In the meantime, the TaskExecutor will spawn a sending thread and a
  /// receiving
  /// thread that will each continuously try to send/receive.

  // Note: This method gets run in a separate thread, so all operations
  // on member variables must be thread safe.
  void send_messages() {
    int max_batch_size = 5;
    while (active_) {
      //      std::vector<std::pair<
      //          int,
      //          std::vector<std::pair<VersionedModelId,
      //          std::shared_ptr<Input>>>>>
      //          new_messages;
      auto current_active_models = active_containers_->get_known_models();
      for (auto model : current_active_models) {
        auto containers =
            active_containers_->get_model_replicas_snapshot(model);
        for (auto c : containers) {
          auto batch = c->dequeue_predictions(max_batch_size);
          if (batch.size() > 0) {
            // move the lock up here, so that nothing can pull from the
            // inflight_messages_
            // map between the time a message is sent and when it gets inserted
            // into the map
            std::unique_lock<std::mutex> l(inflight_messages_mutex_);
            // std::vector<const std::vector<uint8_t>> serialized_inputs;

            std::vector<std::pair<VersionedModelId, std::shared_ptr<Input>>>
                cur_batch;
            rpc::PredictionRequest prediction_request(c->input_type_);
            for (auto b : batch) {
              prediction_request.add_input(b.input_);
              cur_batch.emplace_back(b.model_, b.input_);
            }
            int message_id = rpc_->send_message(prediction_request.serialize(),
                                                c->container_id_);
            inflight_messages_.emplace(message_id, std::move(cur_batch));
          }
        }
      }
    }
  }

  // Note: This method gets run in a separate thread, so all operations
  // on member variables must be thread safe.
  void recv_messages() {
    int max_responses = 10;
    while (active_) {
      auto responses = rpc_->try_get_responses(max_responses);
      if (responses.empty()) {
        continue;
      }
      std::unique_lock<std::mutex> l(inflight_messages_mutex_);
      for (auto r : responses) {
        auto keys = inflight_messages_[r.first];

        inflight_messages_.erase(r.first);
        std::vector<float> deserialized_outputs = deserialize_outputs(r.second);
        assert(deserialized_outputs.size() == keys.size());
        int batch_size = keys.size();
        for (int batch_num = 0; batch_num < batch_size; ++batch_num) {
          cache_.put(
              keys[batch_num].first, keys[batch_num].second,
              Output{deserialized_outputs[batch_num], keys[batch_num].first});
        }
      }
    }
  }
};

class PowerTwoChoicesScheduler {
 public:
  std::shared_ptr<ModelContainer> assign_container(
      const PredictTask &task,
      std::vector<std::shared_ptr<ModelContainer>> &containers) const;
};

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
