#include <chrono>
#include <cmath>
#include <cstdlib>

#include <gtest/gtest.h>

#include <clipper/containers.hpp>
#include <clipper/threadpool.hpp>

using namespace clipper;

namespace {

TEST(ModelContainerTests,
     BatchSizeDeterminationExploitsAdvantageousBatchSizeLatencyRelationship) {
  VersionedModelId model("test", "1");
  ModelContainer container(model, 0, 0, InputType::Doubles, DEFAULT_BATCH_SIZE);
  EstimatorFittingThreadPool::create_queue(model, 0);

  long long base_latency = 500;
  // The factor used to enforce a throughput benefit
  // associated with batching. Because this factor is
  // constant, increasing the batch size will always
  // improve throughput. Therefore, the batch sizes emitted
  // by the batch size determination algorithm should
  // increase monotonically with the latency budget
  double decay_factor = .9;

  size_t last_batch_size = 1;
  for (long long i = 1; i < 200; ++i) {
    long long latency = static_cast<long long>(i * base_latency * decay_factor);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    container.add_processing_datapoint(i, latency);

    if (i % 10 == 0) {
      Deadline deadline =
          std::chrono::system_clock::now() + std::chrono::microseconds(latency);
      size_t batch_size = container.get_batch_size(deadline).first;
      ASSERT_GT(batch_size, last_batch_size);
      last_batch_size = batch_size;
    }
  }
}

TEST(ModelContainerTests,
     BatchSizeDeterminationEstimatesWhenLatencyBudgetHasBeenExplored) {
  VersionedModelId model("test", "1");
  ModelContainer container(model, 0, 0, InputType::Doubles, DEFAULT_BATCH_SIZE);
  EstimatorFittingThreadPool::create_queue(model, 0);

  long long base_latency = 500;

  for (long long i = 1; i <= 50; ++i) {
    long long latency = base_latency * i;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    container.add_processing_datapoint(i, latency);

    // Get the batch size corresponding to a latency budget
    // that has already been explored. We expect the determination
    // method to be "estimation"
    long long under_latency = latency - 1;
    Deadline deadline = std::chrono::system_clock::now() +
                        std::chrono::microseconds(under_latency);
    BatchSizeInfo batch_size_info = container.get_batch_size(deadline);
    BatchSizeDeterminationMethod method = batch_size_info.second;
    ASSERT_EQ(static_cast<int>(method),
              static_cast<int>(BatchSizeDeterminationMethod::Estimation));
  }
}

TEST(ModelContainerTests, IterativeMeanStdUpdatesPerformedCorrectly) {
  std::vector<double> values;
  values.reserve(100);
  for (double i = 1; i <= 100; ++i) {
    values.push_back(i);
  }

  double num_samples = 1;
  double iter_mean = values[0];
  double iter_std = 0;

  for (size_t i = 1; i < values.size(); ++i) {
    std::tie(iter_mean, iter_std) = IterativeUpdater::calculate_new_mean_std(
        num_samples, iter_mean, iter_std, values[i]);
    num_samples += 1;
  }

  double cumulative_mean =
      static_cast<double>(std::accumulate(values.begin(), values.end(), 0)) /
      static_cast<double>(values.size());
  double cumulative_var = 0;
  for (size_t i = 0; i < values.size(); ++i) {
    cumulative_var += std::pow((values[i] - cumulative_mean), 2);
  }
  cumulative_var /= values.size();
  double cumulative_std = std::sqrt(cumulative_var);

  ASSERT_LE(std::abs(iter_mean - cumulative_mean), .00001);
  ASSERT_LE(std::abs(iter_std - cumulative_std), .00001);
}

TEST(ActiveContainerTests, AddContainer) {
  VersionedModelId m1 = VersionedModelId("m", "1");
  int rep_id = 0;
  int conn_id = 0;
  std::shared_ptr<ActiveContainers> active_containers =
      std::make_shared<ActiveContainers>();

  active_containers->add_container(m1, conn_id, rep_id, InputType::Doubles);
  std::shared_ptr<ModelContainer> result =
      active_containers->get_model_replica(m1, rep_id);
  ASSERT_EQ(result->model_, m1);
  ASSERT_EQ(result->container_id_, conn_id);
}

TEST(ActiveContainerTests, AddMultipleContainersDifferentModels) {
  VersionedModelId m1 = VersionedModelId("m", "1");
  int m1rep_id = 0;
  int m1conn_id = 0;

  VersionedModelId j1 = VersionedModelId("j", "1");
  int j1rep_id = 0;
  int j1conn_id = 1;
  std::shared_ptr<ActiveContainers> active_containers =
      std::make_shared<ActiveContainers>();

  active_containers->add_container(m1, m1conn_id, m1rep_id, InputType::Doubles);
  std::shared_ptr<ModelContainer> m1result =
      active_containers->get_model_replica(m1, m1rep_id);
  ASSERT_EQ(m1result->model_, m1);
  ASSERT_EQ(m1result->container_id_, m1conn_id);

  active_containers->add_container(j1, j1conn_id, j1rep_id, InputType::Doubles);
  std::shared_ptr<ModelContainer> m1result2 =
      active_containers->get_model_replica(m1, m1rep_id);
  ASSERT_EQ(m1result2->model_, m1);
  ASSERT_EQ(m1result2->container_id_, m1conn_id);

  std::shared_ptr<ModelContainer> j1result =
      active_containers->get_model_replica(j1, j1rep_id);
  ASSERT_EQ(j1result->model_, j1);
  ASSERT_EQ(j1result->container_id_, j1conn_id);
}

TEST(ActiveContainerTests, AddMultipleContainersSameModelSameVersion) {
  VersionedModelId vm = VersionedModelId("m", "1");
  int firstrep_id = 0;
  int firstconn_id = 0;

  int secondrep_id = 1;
  int secondconn_id = 1;
  std::shared_ptr<ActiveContainers> active_containers =
      std::make_shared<ActiveContainers>();

  active_containers->add_container(vm, firstconn_id, firstrep_id,
                                   InputType::Doubles);

  active_containers->add_container(vm, secondconn_id, secondrep_id,
                                   InputType::Doubles);

  std::shared_ptr<ModelContainer> secondresult =
      active_containers->get_model_replica(vm, secondrep_id);
  ASSERT_EQ(secondresult->model_, vm);
  ASSERT_EQ(secondresult->container_id_, secondconn_id);

  std::shared_ptr<ModelContainer> firstresult =
      active_containers->get_model_replica(vm, firstrep_id);
  ASSERT_EQ(firstresult->model_, vm);
  ASSERT_EQ(firstresult->container_id_, firstconn_id);
}

TEST(ActiveContainerTests, AddMultipleContainersSameModelDifferentVersions) {
  VersionedModelId vm1 = VersionedModelId("m", "1");
  int firstrep_id = 0;
  int firstconn_id = 0;

  VersionedModelId vm2 = VersionedModelId("m", "2");
  int secondrep_id = 1;
  int secondconn_id = 1;
  std::shared_ptr<ActiveContainers> active_containers =
      std::make_shared<ActiveContainers>();

  active_containers->add_container(vm1, firstconn_id, firstrep_id,
                                   InputType::Doubles);

  active_containers->add_container(vm2, secondconn_id, secondrep_id,
                                   InputType::Doubles);

  std::shared_ptr<ModelContainer> secondresult =
      active_containers->get_model_replica(vm2, secondrep_id);
  ASSERT_EQ(secondresult->model_, vm2);
  ASSERT_EQ(secondresult->container_id_, secondconn_id);

  std::shared_ptr<ModelContainer> firstresult =
      active_containers->get_model_replica(vm1, firstrep_id);
  ASSERT_EQ(firstresult->model_, vm1);
  ASSERT_EQ(firstresult->container_id_, firstconn_id);
}
}  // namespace
