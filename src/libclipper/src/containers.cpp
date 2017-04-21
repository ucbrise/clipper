
#include <iostream>
#include <memory>
#include <random>
#include <chrono>
// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include <clipper/constants.hpp>
#include <clipper/containers.hpp>
#include <clipper/logging.hpp>
#include <clipper/util.hpp>
#include <clipper/metrics.hpp>

#include <boost/thread.hpp>
#include <boost/circular_buffer.hpp>

namespace clipper {

ModelContainer::ModelContainer(VersionedModelId model, int container_id,
                               InputType input_type)
    : model_(model), container_id_(container_id), input_type_(input_type), avg_throughput_per_milli_(0),
      throughput_buffer_(THROUGHPUT_BUFFER_CAPACITY){}

void ModelContainer::update_throughput(size_t batch_size, long total_latency_micros) {
  boost::unique_lock<boost::shared_mutex> lock(throughput_mutex_);
  double new_throughput = 1000 * (static_cast<double>(batch_size) / static_cast<double>(total_latency_micros));
  double old_total_throughput = avg_throughput_per_milli_ * throughput_buffer_.size();
  if(throughput_buffer_.size() == throughput_buffer_.capacity()) {
    double oldest_throughput = throughput_buffer_.front();
    double new_total_throughput = (old_total_throughput - oldest_throughput + new_throughput);
    avg_throughput_per_milli_ = new_total_throughput / static_cast<double>(throughput_buffer_.size());
  } else {
    avg_throughput_per_milli_ =
        (old_total_throughput + new_throughput) / static_cast<double>(throughput_buffer_.size() + 1);
  }
  throughput_buffer_.push_back(new_throughput);
}

double ModelContainer::get_average_throughput_per_millisecond() {
  boost::shared_lock<boost::shared_mutex> lock(throughput_mutex_);
  return avg_throughput_per_milli_;
}

size_t ModelContainer::get_batch_size(Deadline deadline) {
  double current_time_millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  double deadline_millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(deadline.time_since_epoch()).count();
  double remaining_time_millis = deadline_millis - current_time_millis;
  boost::shared_lock<boost::shared_mutex> lock(throughput_mutex_);
  int batch_size = static_cast<int>(avg_throughput_per_milli_ * remaining_time_millis);
  if(batch_size < 1) {
    batch_size = 1;
  }
  return batch_size;
}

ActiveContainers::ActiveContainers()
    : containers_(
          std::unordered_map<VersionedModelId,
                             std::map<int, std::shared_ptr<ModelContainer>>,
                             decltype(&versioned_model_hash)>(
              100, &versioned_model_hash)) {}

void ActiveContainers::add_container(VersionedModelId model, int connection_id,
                                     int replica_id, InputType input_type) {
  log_info_formatted(
      LOGGING_TAG_CLIPPER,
      "Adding new container - model: {}, version: {}, ID: {}, input_type: {}",
      model.first, model.second, connection_id,
      get_readable_input_type(input_type));
  boost::unique_lock<boost::shared_mutex> l{m_};
  auto new_container =
      std::make_shared<ModelContainer>(model, connection_id, input_type);
  auto entry = containers_[new_container->model_];
  entry.emplace(replica_id, new_container);
  containers_[new_container->model_] = entry;
  assert(containers_[new_container->model_].size() > 0);
}

std::shared_ptr<ModelContainer> ActiveContainers::get_model_replica(
    const VersionedModelId &model, const int replica_id) {
  auto replicas_map_entry = containers_.find(model);
  if (replicas_map_entry == containers_.end()) {
    return nullptr;
  }

  std::map<int, std::shared_ptr<ModelContainer>> replicas_map =
      replicas_map_entry->second;
  auto replica_entry = replicas_map.find(replica_id);
  if (replica_entry != replicas_map.end()) {
    return replica_entry->second;
  } else {
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
}
