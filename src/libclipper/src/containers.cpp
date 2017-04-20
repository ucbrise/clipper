
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

namespace clipper {

ModelContainer::ModelContainer(VersionedModelId model, int container_id,
                               InputType input_type)
    : model_(model), container_id_(container_id), input_type_(input_type) {
  std::shared_ptr<metrics::MeterClock> throughput_clock = std::make_shared<metrics::RealTimeClock>();
  throughput_meter_ = std::make_shared<metrics::Meter>("container_throughput_meter", throughput_clock);
}

size_t ModelContainer::get_batch_size(Deadline deadline) {
  double container_throughput_rate_millis = throughput_meter_->get_one_minute_rate_seconds() / 1000;
  std::chrono::time_point<std::chrono::system_clock> current_time =
      std::chrono::system_clock::now();
  double remaining_time_millis =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline.time_since_epoch() - current_time.time_since_epoch()).count();
  int batch_size = static_cast<int>(container_throughput_rate_millis * remaining_time_millis);
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
