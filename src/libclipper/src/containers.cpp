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
#include <clipper/threadpool.hpp>
#include <clipper/util.hpp>

#include <dlib/matrix.h>
#include <dlib/svm.h>
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
      latency_hist_("container:" + model.serialize() + ":" +
                        std::to_string(replica_id) + ":prediction_latency",
                    "microseconds", HISTOGRAM_SAMPLE_SIZE),
      processing_datapoints_(DATAPOINTS_BUFFER_CAPACITY),
      max_batch_size_(1),
      max_latency_(0),
      estimator_trainer(dlib::krr_trainer<EstimatorKernel>()),
      explore_dist_mu_(.1),
      explore_dist_std_(.05),
      estimate_decay_(.8),
      exploration_distribution_(std::normal_distribution<double>(
          explore_dist_mu_, explore_dist_std_)),
      exploration_engine_(std::default_random_engine(
          std::chrono::system_clock::now().time_since_epoch().count())) {
  size_t gamma = 1;
  size_t coef = 0;
  size_t degree = 4;

  estimator_trainer.set_kernel(EstimatorKernel(gamma, coef, degree));

  std::string model_str = model.serialize();
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Creating new ModelContainer for model {}, id: {}",
                     model_str, std::to_string(container_id));
}

void ModelContainer::add_processing_datapoint(
    size_t batch_size, long long processing_latency_micros) {
  if (batch_size <= 0 || processing_latency_micros <= 0) {
    throw std::invalid_argument(
        "Invalid processing datapoint: Batch size and latency must be "
        "positive.");
  }

  latency_hist_.insert(processing_latency_micros);

  boost::unique_lock<boost::shared_mutex> lock(datapoints_mutex_);
  EstimatorLatency new_lat;
  new_lat(0) = static_cast<double>(processing_latency_micros);
  processing_datapoints_.push_back(
      std::make_pair(new_lat, static_cast<double>(batch_size)));
  max_latency_ = std::max(processing_latency_micros, max_latency_);

  EstimatorFittingThreadPool::submit_job([this]() { fit_estimator(); });
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
  if (budget > max_latency_) {
    curr_batch_size = explore();
  } else {
    curr_batch_size = estimate(budget);
  }
  max_batch_size_ = std::max(curr_batch_size, max_batch_size_);
  return curr_batch_size;
}

void ModelContainer::fit_estimator() {
  size_t num_datapoints = processing_datapoints_.size();

  std::vector<EstimatorLatency> x_vals;
  x_vals.reserve(num_datapoints);
  std::vector<EstimatorBatchSize> y_vals;
  y_vals.reserve(num_datapoints);

  for (size_t i = 0; i < num_datapoints; ++i) {
    EstimatorLatency point_lat;
    EstimatorBatchSize point_bs;
    std::tie(point_lat, point_bs) = processing_datapoints_[i];
    x_vals.push_back(std::move(point_lat));
    y_vals.push_back(std::move(point_bs));
  }

  auto new_estimator = estimator_trainer.train(x_vals, y_vals);
  std::lock_guard<std::mutex> lock(estimator_mtx_);
  estimator_ = new_estimator;
  EstimatorLatency test_lat;
  test_lat(0) = 1000000.0;
  double estimate = estimator_(test_lat);
  log_info_formatted(LOGGING_TAG_CONTAINERS, "ESTIMATE: {}", estimate);
}

size_t ModelContainer::explore() {
  if (max_batch_size_ < 10) {
    return max_batch_size_ + 1;
  } else {
    double expansion_factor = exploration_distribution_(exploration_engine_);
    return static_cast<size_t>((1 + expansion_factor) * max_batch_size_);
  }
}

size_t ModelContainer::estimate(long long budget) {
  std::lock_guard<std::mutex> lock(estimator_mtx_);
  return estimator_(EstimatorLatency(budget));
}

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
