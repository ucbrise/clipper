#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <random>

#include <clipper/constants.hpp>
#include <clipper/containers.hpp>
#include <clipper/dlib_dependencies.hpp>
#include <clipper/logging.hpp>
#include <clipper/metrics.hpp>
#include <clipper/threadpool.hpp>
#include <clipper/util.hpp>

#include <boost/circular_buffer.hpp>

namespace clipper {

const std::string LOGGING_TAG_CONTAINERS = "CONTAINERS";

ModelContainer::~ModelContainer() {
  std::lock_guard<std::mutex> activity_lock(*activity_mtx_);
  *active_ = false;
}

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
      active_(std::make_shared<bool>(true)),
      activity_mtx_(std::make_shared<std::mutex>()),
      max_batch_size_(1),
      max_latency_(0),
      // These distribution constants were selected based on the empirical
      // observation that they result in reasonably fast, yet controlled
      // exploration of new batch sizes
      explore_dist_mu_(.1),
      explore_dist_std_(.05),
      // The budget decay constant was selected based on empirical tuning
      budget_decay_(.92),
      exploration_distribution_(std::normal_distribution<double>(
          explore_dist_mu_, explore_dist_std_)),
      exploration_engine_(std::default_random_engine(
          std::chrono::system_clock::now().time_since_epoch().count())) {
  std::string model_str = model.serialize();
  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Creating new ModelContainer for model {}, id: {}",
                     model_str, std::to_string(container_id));
}

void ModelContainer::set_inactive() { connected_ = false; }

bool ModelContainer::is_active() { return connected_; }

void ModelContainer::add_processing_datapoint(
    size_t batch_size, long long processing_latency_micros) {
  if (batch_size <= 0 || processing_latency_micros <= 0) {
    throw std::invalid_argument(
        "Invalid processing datapoint: Batch size and latency must be "
        "positive.");
  }

  latency_hist_.insert(processing_latency_micros);

  boost::unique_lock<boost::shared_mutex> lock(datapoints_mtx_);

  auto bs_search = processing_datapoints_.find(batch_size);
  if (bs_search == processing_datapoints_.end()) {
    double info_size = 1;
    double std = 0;
    LatencyInfo latency_info = std::make_tuple(
        std::move(info_size), static_cast<double>(processing_latency_micros),
        std::move(std));
    processing_datapoints_.emplace(static_cast<EstimatorBatchSize>(batch_size),
                                   std::move(latency_info));
  } else {
    LatencyInfo &old_info = bs_search->second;
    LatencyInfo new_info = update_mean_std(
        old_info, static_cast<double>(processing_latency_micros));
    processing_datapoints_[batch_size] = std::move(new_info);
  }

  max_latency_ = std::max(processing_latency_micros, max_latency_);

  EstimatorFittingThreadPool::submit_job(model_, replica_id_, [
    this, container_activity_mtx = activity_mtx_, container_active = active_
  ]() {
    std::lock_guard<std::mutex> activity_lock(*container_activity_mtx);
    if (!(*container_active)) {
      return;
    }
    try {
      fit_estimator();
    } catch (std::exception const &ex) {
      log_error_formatted(LOGGING_TAG_CONTAINERS,
                          "Error fitting batch size estimator: {}", ex.what());
    }
  });
}

ModelContainer::LatencyInfo ModelContainer::update_mean_std(
    LatencyInfo &info, double new_latency) {
  double info_size = std::get<0>(info);
  double mu = std::get<1>(info);
  double std = std::get<2>(info);

  double new_mu;
  double new_std;
  std::tie(new_mu, new_std) =
      IterativeUpdater::calculate_new_mean_std(info_size, mu, std, new_latency);

  return std::make_tuple(info_size + 1, new_mu, new_std);
}

void ModelContainer::set_batch_size(int batch_size) {
  batch_size_ = batch_size;
}

BatchSizeInfo ModelContainer::get_batch_size(Deadline deadline) {
  BatchSizeDeterminationMethod method;
  if (batch_size_ != DEFAULT_BATCH_SIZE) {
    method = BatchSizeDeterminationMethod::Default;
    return std::make_pair(batch_size_, method);
  }

  double budget =
      static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(
                              deadline - std::chrono::system_clock::now())
                              .count());
  // Decay the provided latency budget by a pre-specified factor
  // in order to provide enough slack for delivering the response
  // to the user and/or coping with an anomolously high
  // processing latency
  budget = budget * budget_decay_;

  size_t curr_batch_size;
  if (budget > static_cast<double>(max_latency_)) {
    curr_batch_size = explore();
    method = BatchSizeDeterminationMethod::Exploration;
  } else {
    curr_batch_size = estimate(budget);
    method = BatchSizeDeterminationMethod::Estimation;
  }
  max_batch_size_ = std::max(curr_batch_size, max_batch_size_);
  return std::make_pair(curr_batch_size, method);
}

void ModelContainer::fit_estimator() {
  std::unique_lock<std::mutex> estimator_lock(estimator_mtx_);
  boost::shared_lock<boost::shared_mutex> datapoints_lock(datapoints_mtx_);

  size_t num_datapoints = processing_datapoints_.size();

  std::vector<EstimatorLatency> x_vals;
  x_vals.reserve(num_datapoints);
  std::vector<EstimatorBatchSize> y_vals;
  y_vals.reserve(num_datapoints);

  // Calculate the pooled processing latency
  // variance across all batch sizes. This will
  // be used to obtain an estimate for the p99
  // processing latency at each batch size
  //
  // Pooled variance (https://en.wikipedia.org/wiki/Pooled_variance)
  // is used under the empirically tested assumption that latency variance
  // does not change significantly with batch size

  double pooled_std_num = 0;
  double pooled_std_denom = -1 * static_cast<double>(num_datapoints);

  for (auto &entry : processing_datapoints_) {
    LatencyInfo &latency_info = entry.second;
    double info_size = std::get<0>(latency_info);
    if (info_size >= MINIMUM_BATCH_SAMPLE_SIZE) {
      double lats_std = std::get<2>(latency_info);
      double lats_var = std::pow(lats_std, 2);
      pooled_std_num += (info_size - 1) * lats_var;
      pooled_std_denom += info_size;
    }
  }

  double pooled_std =
      std::sqrt(pooled_std_num / std::max(1.0, pooled_std_denom));

  // Using the pooled variance, obtain the
  // estimated p99 latency for each batch size

  EstimatorLatency fitting_lat;
  for (auto &entry : processing_datapoints_) {
    EstimatorBatchSize batch_size = entry.first;
    LatencyInfo &latency_info = entry.second;
    double lats_mean = std::get<1>(latency_info);
    double upper_bound_lat = lats_mean + (LATENCY_Z_SCORE * pooled_std);
    // Scale the upper bound latency in order to moderate the regularization
    // term associated with ridge regression
    fitting_lat(0) = upper_bound_lat * REGRESSION_DATA_SCALE_FACTOR;
    x_vals.push_back(fitting_lat);
    y_vals.push_back(std::move(batch_size));
  }

  datapoints_lock.unlock();

  estimator_ = estimator_trainer_.train(x_vals, y_vals);
}

size_t ModelContainer::explore() {
  auto mb_search =
      processing_datapoints_.find(static_cast<double>(max_batch_size_));
  if (mb_search != processing_datapoints_.end() &&
      std::get<0>(mb_search->second) < MINIMUM_BATCH_SAMPLE_SIZE) {
    // We don't have a large enough latency sample
    // corresponding to the maximum batch size, so
    // we won't update the maximum size
    return max_batch_size_;
  } else if (max_batch_size_ < ADDITIVE_EXPANSION_THRESHOLD) {
    return max_batch_size_ + 1;
  } else {
    double expansion_factor = exploration_distribution_(exploration_engine_);
    expansion_factor = std::max(0.0, expansion_factor);
    return static_cast<size_t>((1 + expansion_factor) * max_batch_size_);
  }
}

size_t ModelContainer::estimate(double budget) {
  std::lock_guard<std::mutex> lock(estimator_mtx_);
  EstimatorLatency estimator_budget;
  estimator_budget(0) =
      static_cast<double>(budget) * REGRESSION_DATA_SCALE_FACTOR;
  double estimate = estimator_(estimator_budget);
  return static_cast<size_t>(std::max(1.0, std::floor(estimate)));
}

ActiveContainers::ActiveContainers()
    : batch_sizes_(std::unordered_map<VersionedModelId, int>()),
      containers_(
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
  log_active_containers();
}

void ActiveContainers::remove_container(VersionedModelId model,
                                        int replica_id) {
  boost::unique_lock<boost::shared_mutex> l{m_};

  auto replicas_map_entry = containers_.find(model);
  if (replicas_map_entry == containers_.end()) {
    log_error_formatted(LOGGING_TAG_CONTAINERS,
                        "Requested removing container {} for model {} NOT FOUND",
                        replica_id, model.serialize());
    return;
  }

  log_info_formatted(LOGGING_TAG_CONTAINERS,
                     "Removing container {} for model {}",
                     replica_id, model.serialize());

  size_t initial_size = containers_[model].size();

  for (auto it = containers_[model].begin(); it != containers_[model].end();) {
    if (it->first == replica_id) {
      (it->second)->set_inactive();
      it = containers_[model].erase(it);
    } else {
      ++it;
    }
  }

  assert(containers_[model].size() == initial_size - 1);

  if (containers_[model].size() == 0) {
    log_info_formatted(LOGGING_TAG_CONTAINERS,
                       "All containers of model {} are removed. Remove itself",
                       model.serialize());
    containers_.erase(model);
  }

  log_active_containers();
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

void ActiveContainers::unregister_batch_size(VersionedModelId model) {
  auto batch_size_entry = batch_sizes_.find(model);
  if (batch_size_entry != batch_sizes_.end()) {
    batch_sizes_.erase(model);
  }
}

std::shared_ptr<ModelContainer> ActiveContainers::get_model_replica(
    const VersionedModelId &model, const int replica_id) {
  boost::shared_lock<boost::shared_mutex> l{m_};

  auto replicas_map_entry = containers_.find(model);
  if (replicas_map_entry == containers_.end()) {
    log_error_formatted(LOGGING_TAG_CONTAINERS,
                        "Requested container {} for model {} NOT FOUND",
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

std::map<int, std::shared_ptr<ModelContainer>>
ActiveContainers::get_replicas_for_model(const VersionedModelId &model) {
  boost::shared_lock<boost::shared_mutex> l{m_};

  auto replicas_map_entry = containers_.find(model);
  if (replicas_map_entry == containers_.end()) {
    log_error_formatted(LOGGING_TAG_CONTAINERS, "Requested model {} NOT FOUND",
                        model.serialize());
    return {};
  }

  return replicas_map_entry->second;
}

std::vector<VersionedModelId> ActiveContainers::get_known_models() {
  boost::shared_lock<boost::shared_mutex> l{m_};
  std::vector<VersionedModelId> keys;
  for (auto m : containers_) {
    keys.push_back(m.first);
  }
  return keys;
}

void ActiveContainers::log_active_containers() {
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
}  // namespace clipper
