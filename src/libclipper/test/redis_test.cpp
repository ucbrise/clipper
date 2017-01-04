#include <gtest/gtest.h>
#include <condition_variable>
#include <memory>
#include <vector>

#include <clipper/constants.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/redis.hpp>
#include <redox.hpp>

using namespace clipper;
using namespace clipper::redis;

namespace {

// NOTE: THIS IS A POTENTIALLY FLAKY TEST SUITE.
//
// Several of the tests in this suite register a subscription
// with Redis, then issue commmands to test that the subscription
// is behaving correctly. In order to make sure that the
// subscription was registered, the tests wait 500ms between
// requesting the subscription and testing its behavior. If this
// is not enough time for the subscription to be registered, the
// tests will fail.

class RedisTest : public ::testing::Test {
 public:
  RedisTest()
      : redis_(std::make_shared<redox::Redox>()),
        subscriber_(std::make_shared<redox::Subscriber>()) {
    redis_->connect("localhost", REDIS_TEST_PORT);
    subscriber_->connect("localhost", REDIS_TEST_PORT);

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});

    send_cmd_no_reply<std::string>(
        *redis_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
  }
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;

  virtual ~RedisTest() {
    redis_->disconnect();
    subscriber_->disconnect();
  }
};

TEST_F(RedisTest, InsertModel) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model = std::make_pair("m", 1);
  ASSERT_TRUE(insert_model(*redis_, model, InputType::Ints, "double", labels));
  auto result = get_model(*redis_, model);
  EXPECT_EQ(result.size(), static_cast<size_t>(6));
  ASSERT_EQ(result["model_name"], model.first);
  ASSERT_EQ(std::stoi(result["model_version"]), model.second);
  ASSERT_FLOAT_EQ(std::stof(result["load"]), 0.0);
  ASSERT_EQ(str_to_labels(result["labels"]), labels);
  ASSERT_EQ(parse_input_type(result["input_type"]), InputType::Ints);
}

TEST_F(RedisTest, DeleteModel) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  ASSERT_TRUE(insert_model(*redis_, model, InputType::Ints, "double", labels));
  auto insert_result = get_model(*redis_, model);
  EXPECT_EQ(insert_result.size(), static_cast<size_t>(6));
  ASSERT_TRUE(delete_model(*redis_, model));
  auto delete_result = get_model(*redis_, model);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, InsertContainer) {
  VersionedModelId model = std::make_pair("m", 1);
  int replica_id = 4;
  int zmq_connection_id = 12;
  InputType input_type = InputType::Doubles;
  ASSERT_TRUE(insert_container(*redis_, model, replica_id, zmq_connection_id,
                               input_type));
  auto result = get_container(*redis_, model, replica_id);
  EXPECT_EQ(result.size(), static_cast<size_t>(7));
  EXPECT_EQ(result["model_name"], model.first);
  EXPECT_EQ(std::stoi(result["model_version"]), model.second);
  EXPECT_EQ(result["model_id"], gen_versioned_model_key(model));
  EXPECT_EQ(std::stoi(result["model_replica_id"]), replica_id);
  EXPECT_EQ(std::stoi(result["zmq_connection_id"]), zmq_connection_id);
  EXPECT_EQ(std::stoi(result["batch_size"]), 1);
  EXPECT_EQ(parse_input_type(result["input_type"]), input_type);
}

TEST_F(RedisTest, DeleteContainer) {
  VersionedModelId model = std::make_pair("m", 1);
  int replica_id = 4;
  int zmq_connection_id = 12;
  InputType input_type = InputType::Strings;
  ASSERT_TRUE(insert_container(*redis_, model, replica_id, zmq_connection_id,
                               input_type));
  auto get_result = get_container(*redis_, model, replica_id);
  EXPECT_EQ(get_result.size(), static_cast<size_t>(7));
  ASSERT_TRUE(delete_container(*redis_, model, replica_id));
  auto delete_result = get_container(*redis_, model, replica_id);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, InsertApplication) {
  std::string name = "my_app_name";
  std::vector<VersionedModelId> models{
      std::make_pair("music_random_features", 1),
      std::make_pair("simple_svm", 2), std::make_pair("music_cnn", 4)};
  InputType input_type = InputType::Doubles;
  std::string output_type = "double";
  std::string policy = "exp3_policy";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(insert_application(*redis_, name, models, input_type, output_type,
                                 policy, latency_slo_micros));
  auto result = get_application(*redis_, name);
  EXPECT_EQ(result.size(), static_cast<size_t>(5));
  EXPECT_EQ(str_to_models(result["candidate_models"]), models);
  EXPECT_EQ(parse_input_type(result["input_type"]), input_type);
  EXPECT_EQ(result["output_type"], output_type);
  EXPECT_EQ(result["policy"], policy);
  EXPECT_EQ(std::stoi(result["latency_slo_micros"]), latency_slo_micros);
}

TEST_F(RedisTest, DeleteApplication) {
  std::string name = "my_app_name";
  std::vector<VersionedModelId> models{
      std::make_pair("music_random_features", 1),
      std::make_pair("music_cnn", 4)};
  InputType input_type = InputType::Doubles;
  std::string output_type = "double";
  std::string policy = "exp3_policy";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(insert_application(*redis_, name, models, input_type, output_type,
                                 policy, latency_slo_micros));
  auto get_result = get_application(*redis_, name);
  EXPECT_EQ(get_result.size(), static_cast<size_t>(5));
  ASSERT_TRUE(delete_application(*redis_, name));
  auto delete_result = get_application(*redis_, name);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, SubscriptionDetectModelInsert) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, model](
                        const std::string& key, const std::string& event_type) {
        std::cout << "NEW MODEL CALLBACK FIRED" << std::endl;
        ASSERT_EQ(event_type, "hset");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        std::string model_id_key = gen_versioned_model_key(model);
        ASSERT_EQ(key, model_id_key);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(insert_model(*redis_, model, InputType::Ints, "double", labels));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelDelete) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  ASSERT_TRUE(insert_model(*redis_, model, InputType::Ints, "double", labels));
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, model](
                        const std::string& key, const std::string& event_type) {
        std::cout << "MODEL CHANGE DETECTED: " << event_type << std::endl;
        ASSERT_TRUE(event_type == "hdel" || event_type == "del");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        std::string model_id_key = gen_versioned_model_key(model);
        ASSERT_EQ(key, model_id_key);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(delete_model(*redis_, model));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectContainerInsert) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model_id = std::make_pair("m", 1);
  int model_replica_id = 0;
  int zmq_connection_id = 7;
  std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
  InputType input_type = InputType::Strings;

  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_container_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &recv, replica_key](
          const std::string& key, const std::string& event_type) {
        std::cout << "NEW CONTAINER CALLBACK FIRED" << std::endl;
        ASSERT_EQ(event_type, "hset");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, replica_key);
        notification_recv.notify_one();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(insert_container(*redis_, model_id, model_replica_id,
                               zmq_connection_id, input_type));

  // std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectContainerDelete) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model_id = std::make_pair("m", 1);
  int model_replica_id = 0;
  int zmq_connection_id = 7;
  std::string replica_key = gen_model_replica_key(model_id, model_replica_id);
  InputType input_type = InputType::Strings;
  ASSERT_TRUE(insert_container(*redis_, model_id, model_replica_id,
                               zmq_connection_id, input_type));
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_container_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &recv, replica_key](
          const std::string& key, const std::string& event_type) {
        std::cout << "CONTAINER DELETED CALLBACK. EVENT TYPE: " << event_type
                  << std::endl;
        ASSERT_TRUE(event_type == "hdel" || event_type == "del");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, replica_key);
        notification_recv.notify_one();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(delete_container(*redis_, model_id, model_replica_id));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectApplicationInsert) {
  std::string name = "my_app_name";
  std::vector<VersionedModelId> models{
      std::make_pair("music_random_features", 1),
      std::make_pair("music_cnn", 4)};
  InputType input_type = InputType::Doubles;
  std::string output_type = "double";
  std::string policy = "exp3_policy";
  int latency_slo_micros = 10000;

  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_application_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, name](
                        const std::string& key, const std::string& event_type) {
        ASSERT_EQ(event_type, "hset");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, name);
        notification_recv.notify_one();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_TRUE(insert_application(*redis_, name, models, input_type, output_type,
                                 policy, latency_slo_micros));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectApplicationDelete) {
  std::string name = "my_app_name";
  std::vector<VersionedModelId> models{
      std::make_pair("music_random_features", 1),
      std::make_pair("music_cnn", 4)};
  InputType input_type = InputType::Doubles;
  std::string output_type = "double";
  std::string policy = "exp3_policy";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(insert_application(*redis_, name, models, input_type, output_type,
                                 policy, latency_slo_micros));
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_application_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, name](
                        const std::string& key, const std::string& event_type) {
        ASSERT_TRUE(event_type == "hdel" || event_type == "del");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, name);
        notification_recv.notify_one();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(delete_application(*redis_, name));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

}  // namespace
