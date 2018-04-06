#ifndef CLIPPER_LIB_CONTAINERS_HPP
#define CLIPPER_LIB_CONTAINERS_HPP

constexpr int DEFAULT_BATCH_SIZE = -1;

#include <memory>
#include <random>
#include <unordered_map>

#include <dlib/matrix.h>
#include <dlib/svm.h>

#include <boost/circular_buffer.hpp>

#include <clipper/datatypes.hpp>
#include <clipper/metrics.hpp>
#include <clipper/threadpool.hpp>
#include <clipper/util.hpp>

namespace clipper {

// We use the system clock for the deadline time point
// due to its cross-platform consistency (consistent epoch, resolution)
using Deadline = std::chrono::time_point<std::chrono::system_clock>;

class ModelContainer {
 public:
  ~ModelContainer() = default;
  ModelContainer(VersionedModelId model, int container_id, int replica_id,
                 InputType input_type, int batch_size);
  // disallow copy
  ModelContainer(const ModelContainer &) = delete;
  ModelContainer &operator=(const ModelContainer &) = delete;

  ModelContainer(ModelContainer &&) = default;
  ModelContainer &operator=(ModelContainer &&) = default;

  size_t get_batch_size(Deadline deadline);
  void add_processing_datapoint(size_t batch_size,
                                long long processing_latency_micros);
  void send_feedback(PredictTask task);
  void set_batch_size(int batch_size);

  VersionedModelId model_;
  int container_id_;
  int replica_id_;
  InputType input_type_;
  int batch_size_;
  clipper::metrics::Histogram latency_hist_;

 private:
  using EstimatorLatency = dlib::matrix<double, 1, 1>;
  using EstimatorBatchSize = double;
  using EstimatorKernel = dlib::linear_kernel<EstimatorLatency>;
  using Estimator = dlib::decision_function<EstimatorKernel>;
  // Tuple of num latencies, mean latency, latency std
  using LatencyInfo = std::tuple<double, double, double>;

  // Updates the specified LatencyInfo tuple to account
  // for a new latency entry. The info tuple's mean and
  // standard deviation are updated recursively; the recursive
  // standard deviation relation is defined here:
  // https://link.springer.com/article/10.1007%2FBF02262936
  LatencyInfo update_mean_std(LatencyInfo &info, double new_latency);

  bool connected_{true};
  Queue<FeedbackTask> feedback_queue_;
  boost::shared_mutex datapoints_mtx_;
  std::unordered_map<EstimatorBatchSize, LatencyInfo> processing_datapoints_;

  size_t max_batch_size_;
  long long max_latency_;
  Estimator estimator_;
  dlib::rr_trainer<EstimatorKernel> estimator_trainer_;
  std::mutex estimator_mtx_;

  // Exploration and estimation parameters
  // for adaptive batching
  double explore_dist_mu_;
  double explore_dist_std_;
  double budget_decay_;
  std::normal_distribution<double> exploration_distribution_;
  std::default_random_engine exploration_engine_;

  static constexpr size_t HISTOGRAM_SAMPLE_SIZE = 256;
  // The minimum number of latency entries associated with a batch
  // size that must exist in order to continue exploration
  // or incorporate variance data
  static constexpr uint32_t MINIMUM_BATCH_SAMPLE_SIZE = 5;
  static constexpr uint32_t LATENCY_Z_SCORE = 3;
  static constexpr double REGRESSION_DATA_SCALE_FACTOR = .001;
  static constexpr size_t ADDITIVE_EXPANSION_THRESHOLD = 10;

  void fit_estimator();
  size_t explore();
  size_t estimate(double budget);
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

  void add_container(VersionedModelId model, int connection_id, int replica_id,
                     InputType input_type);
  void register_batch_size(VersionedModelId model, int batch_size);

  /// This method returns the active container specified by the
  /// provided model id and replica id. This is threadsafe because each
  /// individual ModelContainer object is threadsafe, and this method returns
  /// a shared_ptr to a ModelContainer object.
  std::shared_ptr<ModelContainer> get_model_replica(
      const VersionedModelId &model, const int replica_id);

  /// Get list of all models that have at least one active replica.
  std::vector<VersionedModelId> get_known_models();
  std::unordered_map<VersionedModelId, int> batch_sizes_;

 private:
  // Protects the models-replicas map. Must acquire an exclusive
  // lock to modify the map and a shared_lock when accessing
  // replicas. The replica ModelContainer entries are independently threadsafe.
  boost::shared_mutex m_;

  // A mapping of models to their replicas. The replicas
  // for each model are represented as a map keyed on replica id.
  std::unordered_map<VersionedModelId,
                     std::map<int, std::shared_ptr<ModelContainer>>>
      containers_;
};
}  // namespace clipper

#endif
