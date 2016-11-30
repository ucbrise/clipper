
#ifndef CLIPPER_LIB_CONTAINERS_H
#define CLIPPER_LIB_CONTAINERS_H

#include <memory>
#include <unordered_map>

#include <boost/thread.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/util.hpp>

namespace clipper {

class ModelContainer {
 public:
  ~ModelContainer() = default;
  ModelContainer(VersionedModelId model, int id);
  // disallow copy
  ModelContainer(const ModelContainer &) = delete;
  ModelContainer &operator=(const ModelContainer &) = delete;

  ModelContainer(ModelContainer &&) = default;
  ModelContainer &operator=(ModelContainer &&) = default;

  std::vector<PredictTask> dequeue_predictions(int batch_size) {
    return request_queue_.try_pop_batch(batch_size);
  }

  int get_queue_size();
  void send_prediction(PredictTask task);
  void send_feedback(PredictTask task);

  VersionedModelId model_;
  int container_id_;

 private:
  bool connected_{true};
  Queue<PredictTask> request_queue_;
  Queue<FeedbackTask> feedback_queue_;
};

/// This is a lightweight wrapper around the map of active containers
/// to make it threadsafe so it can be safely shared between threads between
/// with a shared_ptr.
class ActiveContainers {
 public:
  explicit ActiveContainers();

  // Disallow copy
  ActiveContainers(const ActiveContainers &) = delete;
  ActiveContainers &operator=(const ActiveContainers &) = delete;

  ActiveContainers(ActiveContainers &&) = default;
  ActiveContainers &operator=(ActiveContainers &&) = default;

  void add_container(VersionedModelId model, int id);

  /// This method returns a vector of all the active containers (replicas)
  /// of the specified model. This is threadsafe because each individual
  /// ModelContainer object is threadsafe, and this method returns
  /// shared_ptrs to the ModelContainer objects. This ensures that
  /// even if one of these ModelContainers gets deleted from the set of
  /// active containers, the object itself won't get destroyed until
  /// the last shared_ptr copy goes out of scope.
  std::vector<std::shared_ptr<ModelContainer>> get_model_replicas_snapshot(
      const VersionedModelId &model);

  /// Get list of all models that have at least one active replica.
  std::vector<VersionedModelId> get_known_models();

 private:
  // Protects the map of task queues. Must acquire an exclusive
  // lock to modify request_queues_ and a shared_lock when accessing
  // the queues. The queues are independently threadsafe.
  boost::shared_mutex m_;

  // Each queue corresponds to a single model container.
  std::unordered_map<VersionedModelId,
                     std::vector<std::shared_ptr<ModelContainer>>,
                     decltype(&versioned_model_hash)>
      containers_;
};
}

#endif
