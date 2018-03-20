#include <algorithm>
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/constants.hpp>
#include <clipper/containers.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/util.hpp>

#include <boost/circular_buffer.hpp>

namespace clipper {

const std::string LOGGING_TAG_CONTAINERS = "CONTAINERS";

ModelContainer::ModelContainer(VersionedModelId model, int container_id,
                               int replica_id, InputType input_type,
                               int batch_size)
    : model_(model),
      container_id_(container_id),
      replica_id_(replica_id),
      input_type_(input_type),
      batch_size_(batch_size),
      max_batch_size_(1),
      max_latency_(0),
      exploration_distribution_(std::normal_distribution<double>(
          explore_dist_mu_, explore_dist_std_)),
      latency_hist_("container:" + model.serialize() + ":" +
                        std::to_string(replica_id) + ":prediction_latency",
                    "microseconds", HISTOGRAM_SAMPLE_SIZE),
      processing_datapoints_(DATAPOINTS_BUFFER_CAPACITY) {
  std::string model_str = model.serialize();
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Creating new ModelContainer for model {}, id: {}",
                     model_str, std::to_string(container_id));
}

void ModelContainer::add_processing_datapoint(size_t batch_size,
                                              long total_latency_micros) {
  if (batch_size <= 0 || total_latency_micros <= 0) {
    throw std::invalid_argument(
        "Batch size and latency must be positive for throughput updates!");
  }

  latency_hist_.insert(total_latency_micros);

  boost::unique_lock<boost::shared_mutex> lock(datapoints_mutex_);
  processing_datapoints_.push_back(
      std::make_pair(batch_size, total_latency_micros));
  max_latency_ = std::max(total_latency_micros, max_latency_);
}

void ModelContainer::set_batch_size(int batch_size) {
  batch_size_ = batch_size;
}

size_t ModelContainer::get_batch_size(Deadline deadline) {
  long long budget = std::chrono::duration_cast<std::chrono::microseconds>(
                         deadline - std::chrono::system_clock::now())
                         .count();
  if (batch_size_ != DEFAULT_BATCH_SIZE) {
    return batch_size_;
  }

  size_t curr_batch_size;
  if (budget <= max_latency_) {
    curr_batch_size = explore();
  } else {
    curr_batch_size = estimate(deadline);
  }
  max_batch_size_ = std::max(curr_batch_size, max_batch_size_);
  return curr_batch_size;
}

size_t ModelContainer::explore() {
  if (max_batch_size_ < 10) {
    return max_batch_size_ + 1;
  } else {
  }
}

size_t ModelContainer::estimate(Deadline deadline) {}

ActiveContainers::ActiveContainers()
    : containers_(
          std::unordered_map<VersionedModelId,
                             std::map<int, std::shared_ptr<ModelContainer>>>(
              {})),
      batch_sizes_(std::unordered_map<VersionedModelId, int>()) {}

void ActiveContainers::add_container(VersionedModelId model, int connection_id,
                                     int replica_id, InputType input_type) {
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Adding new container - model: {}, version: {}, "
                     "connection ID: {}, replica ID: {}, input_type: {}",
                     model.get_name(), model.get_id(), connection_id,
                     replica_id, get_readable_input_type(input_type));
  boost::unique_lock<boost::shared_mutex> l{m_};

  // Set a default batch size of -1
  int batch_size = DEFAULT_BATCH_SIZE;
  auto batch_size_search = batch_sizes_.find(model);
  if (batch_size_search != batch_sizes_.end()) {
    batch_size = batch_size_search->second;
  }

  auto new_container = std::make_shared<ModelContainer>(
      model, connection_id, replica_id, input_type, batch_size);
  auto entry = containers_[new_container->model_];
  entry.emplace(replica_id, new_container);
  containers_[new_container->model_] = entry;
  assert(containers_[new_container->model_].size() > 0);
  std::stringstream log_msg;
  log_msg << "\nActive containers:\n";
  for (auto model : containers_) {
    log_msg << "\tModel: " << model.first.serialize() << "\n";
    for (auto r : model.second) {
      log_msg << "\t\trep_id: " << r.first
              << ", container_id: " << r.second->container_id_ << "\n";
    }
  }
  log_info(LOGGING_TAG_CONTAINERS, log_msg.str());
}

void ActiveContainers::register_batch_size(VersionedModelId model,
                                           int batch_size) {
  auto batch_size_entry = batch_sizes_.find(model);
  if (batch_size_entry != batch_sizes_.end()) {
    batch_sizes_.erase(model);
  }
  batch_sizes_.emplace(model, batch_size);
  auto matching_containers_entry = containers_.find(model);
  if (matching_containers_entry != containers_.end()) {
    for (auto &container : matching_containers_entry->second) {
      container.second->set_batch_size(batch_size);
    }
  }
}

std::shared_ptr<ModelContainer> ActiveContainers::get_model_replica(
    const VersionedModelId &model, const int replica_id) {
  boost::shared_lock<boost::shared_mutex> l{m_};

  auto replicas_map_entry = containers_.find(model);
  if (replicas_map_entry == containers_.end()) {
    log_error_formatted(LOGGING_TAG_CONTAINERS,
                        "Requested replica {} for model {} NOT FOUND",
                        replica_id, model.serialize());
    return nullptr;
  }

  std::map<int, std::shared_ptr<ModelContainer>> replicas_map =
      replicas_map_entry->second;
  auto replica_entry = replicas_map.find(replica_id);
  if (replica_entry != replicas_map.end()) {
    return replica_entry->second;
  } else {
    log_error_formatted(LOGGING_TAG_CONTAINERS,
                        "Requested replica {} for model {} NOT FOUND",
                        replica_id, model.serialize());
    return nullptr;
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
}  // namespace clipper
