
#include <iostream>
#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/containers.hpp>
#include <clipper/util.hpp>

#include <boost/thread.hpp>

namespace clipper {

ModelContainer::ModelContainer(VersionedModelId model, int container_id, InputType input_type)
    : model_(model), container_id_(container_id), input_type_(input_type) {}

int ModelContainer::get_queue_size() { return request_queue_.size(); }

void ModelContainer::send_prediction(PredictTask task) {
  request_queue_.push(task);
}

ActiveContainers::ActiveContainers()
    : containers_(
          std::unordered_map<VersionedModelId,
                             std::vector<std::shared_ptr<ModelContainer>>,
                             decltype(&versioned_model_hash)>(
              100, &versioned_model_hash)) {}

void ActiveContainers::add_container(VersionedModelId model, int id, InputType input_type) {
  std::cout << "Adding new container: "
            << "model: " << model.first << ", version: " << model.second
            << ", ID: " << id << ", INPUT_TYPE: " << input_type << std::endl;
  boost::unique_lock<boost::shared_mutex> l{m_};
  auto new_container = std::make_shared<ModelContainer>(model, id, input_type);
  auto entry = containers_[new_container->model_];
  entry.push_back(new_container);
  containers_[new_container->model_] = entry;
  assert(containers_[new_container->model_].size() > 0);
}

std::vector<std::shared_ptr<ModelContainer>>
ActiveContainers::get_model_replicas_snapshot(const VersionedModelId &model) {
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
