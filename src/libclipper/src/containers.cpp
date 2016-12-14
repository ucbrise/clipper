
#include <iostream>
#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/concurrency.hpp>
#include <clipper/containers.hpp>
#include <clipper/timers.hpp>

#include <boost/thread.hpp>

namespace clipper {

// ModelContainer::ModelContainer(VersionedModelId model, int container_id)
//     : model_(model), container_id_(container_id) {}

// int ModelContainer::get_queue_size() { return request_queue_.size(); }

// void ModelContainer::send_prediction(PredictTask task) {
//   request_queue_.push(task);
// }

ActiveContainers::ActiveContainers()
    : containers_(
          std::unordered_map<VersionedModelId,
                             std::vector<std::shared_ptr<ModelContainer>>,
                             decltype(&versioned_model_hash)>(
              100, &versioned_model_hash)) {}

void ActiveContainers::add_container(VersionedModelId model, int id) {
  std::cout << "Adding new container: "
            << "model: " << model.first << ", version: " << model.second
            << ", ID: " << id << std::endl;
  boost::unique_lock<boost::shared_mutex> l{m_};
  auto new_container = std::make_shared<ModelContainer>(model, id);
  auto entry = containers_[new_container->model_];
  entry.push_back(new_container);
  containers_[new_container->model_] = entry;
  assert(containers_[new_container->model_].size() > 0);
  id_map_[id] = new_container;
}

boost::optional<std::shared_ptr<ModelContainer>>
ActiveContainers::get_container_by_id(int id) {
  auto container = id_map_.find(id);
  if (container != id_map_.end()) {
    return container->second;
  } else {
    return boost::none;
  }
}

std::vector<std::shared_ptr<ModelContainer>>
ActiveContainers::get_model_replicas_snapshot(const VersionedModelId& model) {
  boost::shared_lock<boost::shared_mutex> l{m_};
  auto replicas = containers_.find(model);
  if (replicas != containers_.end()) {
    return replicas->second;
  } else {
    return {};
  }
}

std::vector<VersionedModelId> ActiveContainers::get_known_models() {
  boost::shared_lock<boost::shared_mutex> l{m_};
  std::vector<VersionedModelId> keys;
  for (auto m : containers_) {
    keys.push_back(m.first);
  }
  return keys;
}
}
