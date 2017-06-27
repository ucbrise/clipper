
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
#include <boost/thread.hpp>

namespace clipper {

const std::string LOGGING_TAG_CONTAINERS = "CONTAINERS";

ModelContainer::ModelContainer(VersionedModelId model, int container_id,
                               int replica_id, InputType input_type)
    : model_(model),
      container_id_(container_id),
      replica_id_(replica_id),
      input_type_(input_type),
      latency_hist_("container_prediction_latency", "microseconds",
                    HISTOGRAM_SAMPLE_SIZE),
      avg_throughput_per_milli_(0),
      throughput_buffer_(THROUGHPUT_BUFFER_CAPACITY) {
  std::string model_str = model.serialize();
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Creating new ModelContainer for model {}, id: {}",
                     model_str, std::to_string(container_id));
}

void ModelContainer::update_throughput(size_t batch_size,
                                       long total_latency_micros) {
  if (batch_size <= 0 || total_latency_micros <= 0) {
    throw std::invalid_argument(
        "Batch size and latency must be positive for throughput updates!");
  }

  latency_hist_.insert(total_latency_micros);

  boost::unique_lock<boost::shared_mutex> lock(throughput_mutex_);
  double new_throughput = 1000 * (static_cast<double>(batch_size) /
                                  static_cast<double>(total_latency_micros));
  double old_total_throughput =
      avg_throughput_per_milli_ * throughput_buffer_.size();
  if (throughput_buffer_.size() == throughput_buffer_.capacity()) {
    // If the throughput buffer is already at maximum capacity,
    // we replace the oldest throughput sample with
    // the latest throughput and recalculate the average
    double oldest_throughput = throughput_buffer_.front();
    double new_total_throughput =
        (old_total_throughput - oldest_throughput + new_throughput);
    avg_throughput_per_milli_ =
        new_total_throughput / static_cast<double>(throughput_buffer_.size());
  } else {
    // If the throughput buffer is not yet at capacity,
    // we add the latest throughput sample to the buffer
    // and incorporate it into the average
    avg_throughput_per_milli_ =
        (old_total_throughput + new_throughput) /
        static_cast<double>(throughput_buffer_.size() + 1);
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
      std::chrono::duration_cast<std::chrono::milliseconds>(
          deadline.time_since_epoch())
          .count();
  double remaining_time_millis = deadline_millis - current_time_millis;
  boost::shared_lock<boost::shared_mutex> lock(throughput_mutex_);
  int batch_size =
      static_cast<int>(avg_throughput_per_milli_ * remaining_time_millis);
  if (batch_size < 1) {
    batch_size = 1;
  }
  return batch_size;
}

ActiveContainers::ActiveContainers()
    : containers_(
          std::unordered_map<VersionedModelId,
                             std::map<int, std::shared_ptr<ModelContainer>>>(
              {})) {}

void ActiveContainers::add_container(VersionedModelId model, int connection_id,
                                     int replica_id, InputType input_type) {
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Adding new container - model: {}, version: {}, "
                     "connection ID: {}, replica ID: {}, input_type: {}",
                     model.get_name(), model.get_id(), connection_id,
                     replica_id, get_readable_input_type(input_type));
  boost::unique_lock<boost::shared_mutex> l{m_};
  auto new_container = std::make_shared<ModelContainer>(model, connection_id,
                                                        replica_id, input_type);
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
}
