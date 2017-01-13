#include <gtest/gtest.h>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/redis.hpp>
#include <clipper/query_processor.hpp>
#include <redox.hpp>

using namespace clipper;

namespace {

class RegressionTest : public ::testing::Test {
 public:
  RegressionTest() {
    connect_redis();
  }

  void connect_redis() {
    Config &config = get_config();
    while (!redis_connection_.connect(config.get_redis_address(), config.get_redis_port()));
    while (!redis_subscriber_.connect(config.get_redis_address(), config.get_redis_port()));
  }

  redox::Redox redis_connection_;
  redox::Subscriber redis_subscriber_;

};

TEST_F(RegressionTest, Tst) {
  std::string connection_key;
  bool added_test_container = false;
  redis::subscribe_to_container_changes(
      redis_subscriber_,
      [&connection_key, &added_test_container](const std::string &key, const std::string &event_type) {
        if (event_type == "hset") {
          connection_key = key;
          added_test_container = true;
        }
      });
  QueryProcessor query_processor;
  while (!added_test_container);
  std::unordered_map<std::string, std::string>
      container_info = redis::get_container_by_key(redis_connection_, connection_key);
  VersionedModelId model_id = std::make_pair(container_info["model_name"], std::stoi(container_info["model_version"]));
  std::vector<VersionedModelId> candidate_models;
  candidate_models.push_back(model_id);

  std::vector<std::shared_ptr<Input>> int_inputs;
  int base = 0;
  for(int i = 0; i < 100; i++) {
    std::vector<int> int_data;
    for(int j = 0; j < 100; j++) {
      int_data.push_back(base + j);
    }
    std::shared_ptr<Input> int_input = std::make_shared<IntVector>(int_data);
    int_inputs.push_back(int_input);
    base++;
  }

  for(int i = 0; i < 100; i++) {
    Query query("test_query", 123, int_inputs[i], 30000, "simple_policy", candidate_models);
    Response response = query_processor.predict(query).get();
  }

  metrics::MetricsRegistry &registry = metrics::MetricsRegistry::get_metrics();
  std::cout << registry.report_metrics(false) << std::endl;
}

} // namespace