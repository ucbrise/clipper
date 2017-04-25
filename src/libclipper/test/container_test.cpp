#include <gtest/gtest.h>

#include <clipper/containers.hpp>

using namespace clipper;

namespace {

TEST(ContainerTests,
     ModelContainerAverageThroughputUpdatesCorrectlyFewSamples) {
  VersionedModelId model("test", 1);
  ModelContainer container(model, 0, InputType::Doubles);
  std::array<long, 4> single_task_latencies_micros = {500, 2000, 3000, 5000};
  long avg_latency = 0;
  double avg_throughput_millis = 0;
  for (long latency : single_task_latencies_micros) {
    container.update_throughput(1, latency);
    avg_throughput_millis += 1 / static_cast<double>(latency);
    avg_latency += latency;
  }
  avg_throughput_millis = 1000 * (avg_throughput_millis / 4);

  ASSERT_DOUBLE_EQ(container.get_average_throughput_per_millisecond(),
                   avg_throughput_millis);
}

TEST(ContainerTests,
     ModelContainerAverageThroughputUpdatesCorrectlyManySamples) {
  VersionedModelId model("test", 1);
  ModelContainer container(model, 0, InputType::Doubles);
  double avg_throughput_millis = 0;
  for (int i = 3; i < 103; i++) {
    double throughput_millis = 1 / static_cast<double>(1000 * i);
    avg_throughput_millis += throughput_millis;
  }
  avg_throughput_millis = 1000 * (avg_throughput_millis / 100);
  for (int i = 1; i < 103; i++) {
    container.update_throughput(1, 1000 * i);
  }
  ASSERT_DOUBLE_EQ(container.get_average_throughput_per_millisecond(),
                   avg_throughput_millis);
}
}
