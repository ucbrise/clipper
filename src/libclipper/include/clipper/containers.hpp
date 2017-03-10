
#ifndef CLIPPER_LIB_CONTAINERS_HPP
#define CLIPPER_LIB_CONTAINERS_HPP

#include <memory>
#include <unordered_map>

#include <boost/thread.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/util.hpp>

namespace clipper {

class ModelContainer {
 public:
  ~ModelContainer() = default;
  ModelContainer(VersionedModelId model, int id, InputType input_type);
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
  InputType input_type_;

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

  void add_container(VersionedModelId model, int connection_id, int replica_id, InputType input_type);

  /// TODO: this method should be deprecated / removed when per-model
  /// queueing is implemented, as it currently functions as an efficiency
  /// method in the context of assigning tasks to containers directly
  ///
  /// This method returns a vector of all the active containers (replicas)
  /// of the specified model. This is threadsafe because each individual
  /// ModelContainer object is threadsafe, and this method returns
  /// shared_ptrs to the ModelContainer objects. This ensures that
  /// even if one of these ModelContainers gets deleted from the set of
  /// active containers, the object itself won't get destroyed until
  /// the last shared_ptr copy goes out of scope.
  std::vector<std::shared_ptr<ModelContainer>> get_model_replicas_snapshot(
      const VersionedModelId &model);

  /// This method returns the active container specified by the
  /// provided model id and replica id. This is threadsafe because each
  /// individual ModelContainer object is threadsafe, and this method returns
  /// a shared_ptr to a ModelContainer object.
  std::shared_ptr<ModelContainer> get_model_replica(
      const VersionedModelId &model, const int replica_id);

  /// Get list of all models that have at least one active replica.
  std::vector<VersionedModelId> get_known_models();

 private:
  // Protects the map of task queues. Must acquire an exclusive
  // lock to modify request_queues_ and a shared_lock when accessing
  // the queues. The queues are independently threadsafe.
  boost::shared_mutex m_;

  // A mapping of models to their replica task queues. The replicas
  // for each model are represented as a map keyed on replica id.
  std::unordered_map<VersionedModelId,
                     std::map<int, std::shared_ptr<ModelContainer>>,
                     decltype(&versioned_model_hash)>
      containers_;
};
}

#endif
