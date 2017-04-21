#include <gtest/gtest.h>

#include <clipper/containers.hpp>


using namespace clipper;

namespace {

TEST(ContainerTests, ModelContainerAverageThroughputUpdatesCorrectly) {
  VersionedModelId model("test", 1);
  ModelContainer container(model, 0, InputType::Doubles);
  std::array<long, 4> single_task_latencies_micros = {500, 2000, 3000, 5000};
  long avg_latency = 0;
  double avg_throughput_millis = 0;
  for(long latency : single_task_latencies_micros) {
    container.update_throughput(1, latency);
    avg_throughput_millis += 1 / static_cast<double>(latency);
    avg_latency += latency;
  }
  avg_throughput_millis = 1000 * (avg_throughput_millis / 4);

  ASSERT_EQ(container.get_average_throughput_per_millisecond(), avg_throughput_millis);
}

}