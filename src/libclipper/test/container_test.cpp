#include <gtest/gtest.h>

#include <clipper/containers.hpp>

using namespace clipper;

namespace {

TEST(ModelContainerTests, AverageThroughputUpdatesCorrectlyFewSamples) {
  VersionedModelId model("test", "1");
  ModelContainer container(model, 0, 0, InputType::Doubles);
  std::array<long, 4> single_task_latencies_micros = {{500, 2000, 3000, 5000}};
  long avg_latency = 0;
  double avg_throughput_millis = 0;
  for (long latency : single_task_latencies_micros) {
    container.update_container_stats(1, latency);
    avg_throughput_millis += 1 / static_cast<double>(latency);
    avg_latency += latency;
  }
  avg_throughput_millis = 1000 * (avg_throughput_millis / 4);

  ASSERT_DOUBLE_EQ(container.get_average_throughput_per_millisecond(),
                   avg_throughput_millis);
}

TEST(ModelContainerTests, AverageThroughputUpdatesCorrectlyManySamples) {
  VersionedModelId model("test", "1");
  ModelContainer container(model, 0, 0, InputType::Doubles);
  double avg_throughput_millis = 0;
  for (int i = 3; i < 103; i++) {
    double throughput_millis = 1 / static_cast<double>(1000 * i);
    avg_throughput_millis += throughput_millis;
  }
  avg_throughput_millis = 1000 * (avg_throughput_millis / 100);
  for (int i = 1; i < 103; i++) {
    container.update_container_stats(1, 1000 * i);
  }
  ASSERT_DOUBLE_EQ(container.get_average_throughput_per_millisecond(),
                   avg_throughput_millis);
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
}
