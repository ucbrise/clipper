#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <thread>
#include <vector>

#include <clipper/config.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/logging.hpp>
#include <clipper/redis.hpp>
#include <clipper/selection_policies.hpp>
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

const std::string LOGGING_TAG_REDIS_TEST = "REDISTEST";

class RedisTest : public ::testing::Test {
 public:
  RedisTest()
      : redis_(std::make_shared<redox::Redox>()),
        subscriber_(std::make_shared<redox::Subscriber>()) {
    Config& conf = get_config();
    redis_->connect(conf.get_redis_address(), conf.get_redis_port());
    subscriber_->connect(conf.get_redis_address(), conf.get_redis_port());

    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});

    send_cmd_no_reply<std::string>(
        *redis_, {"CONFIG", "SET", "notify-keyspace-events", "AKE"});
  }
  std::shared_ptr<redox::Redox> redis_;
  std::shared_ptr<redox::Subscriber> subscriber_;

  virtual ~RedisTest() {
    subscriber_->disconnect();
    redis_->disconnect();
  }
};

TEST_F(RedisTest, RedisConnectionRetryLoop) {
  redox::Redox no_connect_redis;
  int attempts = 0;
  int max_attempts = 20;
  while (attempts < max_attempts) {
    log_info_formatted(LOGGING_TAG_REDIS_TEST, "Attempt {} to connect to Redis",
                       attempts);
    // there's no Redis instance running on port 9999
    if (no_connect_redis.connect("localhost", 9999)) {
      break;
    }
    attempts += 1;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  ASSERT_EQ(attempts, max_attempts);
}

TEST_F(RedisTest, AddModel) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model = std::make_pair("m", 1);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path));
  auto result = get_model(*redis_, model);
  // The model table has 8 fields, so we expect
  // to get back a map with 8 entries in it
  // (see add_model() in redis.cpp for details on what the fields are).
  EXPECT_EQ(result.size(), static_cast<size_t>(7));
  ASSERT_EQ(result["model_name"], model.first);
  ASSERT_EQ(std::stoi(result["model_version"]), model.second);
  ASSERT_FLOAT_EQ(std::stof(result["load"]), 0.0);
  ASSERT_EQ(str_to_labels(result["labels"]), labels);
  ASSERT_EQ(parse_input_type(result["input_type"]), InputType::Ints);
  ASSERT_EQ(result["container_name"], container_name);
  ASSERT_EQ(result["model_data_path"], model_path);
}

TEST_F(RedisTest, SetCurrentModelVersion) {
  std::string model_name = "mymodel";
  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 2));
  ASSERT_EQ(get_current_model_version(*redis_, model_name), 2);

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 5));
  ASSERT_EQ(get_current_model_version(*redis_, model_name), 5);

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 3));
  ASSERT_EQ(get_current_model_version(*redis_, model_name), 3);
}

TEST_F(RedisTest, GetModelVersions) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = std::make_pair("m", 1);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path));
  VersionedModelId model2 = std::make_pair("m", 2);
  std::string model_path2 = "/tmp/models/m/2";
  ASSERT_TRUE(add_model(*redis_, model2, InputType::Ints, labels,
                        container_name, model_path2));
  VersionedModelId model4 = std::make_pair("m", 4);
  std::string model_path4 = "/tmp/models/m/4";
  ASSERT_TRUE(add_model(*redis_, model4, InputType::Ints, labels,
                        container_name, model_path4));

  std::vector<int> versions = get_model_versions(*redis_, "m");
  ASSERT_EQ(versions.size(), (size_t)3);
  std::sort(versions.begin(), versions.end());
  ASSERT_EQ(versions, std::vector<int>({1, 2, 4}));
}

TEST_F(RedisTest, DeleteModel) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path));
  auto add_result = get_model(*redis_, model);
  EXPECT_EQ(add_result.size(), static_cast<size_t>(7));
  ASSERT_TRUE(delete_model(*redis_, model));
  auto delete_result = get_model(*redis_, model);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, AddContainer) {
  VersionedModelId model = std::make_pair("m", 1);
  int replica_id = 4;
  int zmq_connection_id = 12;
  InputType input_type = InputType::Doubles;
  ASSERT_TRUE(
      add_container(*redis_, model, replica_id, zmq_connection_id, input_type));
  auto result = get_container(*redis_, model, replica_id);
  // The container table has 7 fields, so we expect to get back a map with 7
  // entries in it (see add_container() in redis.cpp for details on what the
  // fields are).
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
  ASSERT_TRUE(
      add_container(*redis_, model, replica_id, zmq_connection_id, input_type));
  auto get_result = get_container(*redis_, model, replica_id);
  EXPECT_EQ(get_result.size(), static_cast<size_t>(7));
  ASSERT_TRUE(delete_container(*redis_, model, replica_id));
  auto delete_result = get_container(*redis_, model, replica_id);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, AddApplication) {
  std::string name = "my_app_name";
  std::vector<std::string> model_names{"music_random_features", "simple_svm",
                                       "music_cnn"};
  InputType input_type = InputType::Doubles;
  std::string policy = DefaultOutputSelectionPolicy::get_name();
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, model_names, input_type, policy,
                              default_output, latency_slo_micros));
  auto result = get_application(*redis_, name);
  // The application table has 5 fields, so we expect to get back a map with 5
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  EXPECT_EQ(result.size(), static_cast<size_t>(5));
  EXPECT_EQ(str_to_model_names(result["candidate_model_names"]), model_names);
  EXPECT_EQ(parse_input_type(result["input_type"]), input_type);
  EXPECT_EQ(result["policy"], policy);
  EXPECT_EQ(result["default_output"], default_output);
  EXPECT_EQ(std::stoi(result["latency_slo_micros"]), latency_slo_micros);
}

TEST_F(RedisTest, DeleteApplication) {
  std::string name = "my_app_name";
  std::vector<std::string> model_names{"music_random_features", "simple_svm",
                                       "music_cnn"};
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, model_names, input_type, policy,
                              default_output, latency_slo_micros));
  auto get_result = get_application(*redis_, name);
  EXPECT_EQ(get_result.size(), static_cast<size_t>(5));
  ASSERT_TRUE(delete_application(*redis_, name));
  auto delete_result = get_application(*redis_, name);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, ListApplicationNames) {
  std::string name1 = "my_app_name1";
  std::string name2 = "my_app_name2";
  std::vector<std::string> model_names{"music_random_features", "simple_svm",
                                       "music_cnn"};
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;

  ASSERT_TRUE(add_application(*redis_, name1, model_names, input_type, policy,
                              default_output, latency_slo_micros));
  ASSERT_TRUE(add_application(*redis_, name2, model_names, input_type, policy,
                              default_output, latency_slo_micros));

  std::vector<std::string> result = list_application_names(*redis_);

  // Two apps have been registered, so we list_application_names to give us
  // a vector with 2 entries.
  ASSERT_EQ(result.size(), static_cast<size_t>(2));

  bool has_name_1 =
      std::find(result.begin(), result.end(), name1) != result.end();
  bool has_name_2 =
      std::find(result.begin(), result.end(), name2) != result.end();

  // Those entries should be the names of the two existing applications.
  ASSERT_TRUE(has_name_1 && has_name_2);
}

TEST_F(RedisTest, ListApplicationNamesNoneRegistered) {
  std::vector<std::string> result = list_application_names(*redis_);

  // No apps have been registered, so we list_application_names to give us
  // an empty vector
  ASSERT_EQ(result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, SubscriptionDetectModelAdd) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, model](
                        const std::string& key, const std::string& event_type) {
        log_info(LOGGING_TAG_REDIS_TEST, "NEW MODEL CALLBACK FIRED");
        ASSERT_EQ(event_type, "hset");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        std::string model_id_key = gen_versioned_model_key(model);
        ASSERT_EQ(key, model_id_key);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelDelete) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = std::make_pair("m", 1);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path));
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_changes(
      *subscriber_, [&notification_recv, &notification_mutex, &recv, model](
                        const std::string& key, const std::string& event_type) {
        log_info_formatted(LOGGING_TAG_REDIS_TEST, "MODEL CHANGE DETECTED: ",
                           event_type);
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

TEST_F(RedisTest, SubscriptionDetectContainerAdd) {
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
        log_info(LOGGING_TAG_REDIS_TEST, "NEW CONTAINER CALLBACK FIRED");
        ASSERT_EQ(event_type, "hset");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, replica_key);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(add_container(*redis_, model_id, model_replica_id,
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
  ASSERT_TRUE(add_container(*redis_, model_id, model_replica_id,
                            zmq_connection_id, input_type));
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_container_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &recv, replica_key](
          const std::string& key, const std::string& event_type) {
        log_info_formatted(LOGGING_TAG_REDIS_TEST,
                           "CONTAINER DELETED CALLBACK. EVENT TYPE: ",
                           event_type);
        ASSERT_TRUE(event_type == "hdel" || event_type == "del");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, replica_key);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(delete_container(*redis_, model_id, model_replica_id));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectApplicationAdd) {
  std::string name = "my_app_name";
  std::vector<std::string> model_names{"music_random_features", "simple_svm",
                                       "music_cnn"};
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
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
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_TRUE(add_application(*redis_, name, model_names, input_type, policy,
                              default_output, latency_slo_micros));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectApplicationDelete) {
  std::string name = "my_app_name";
  std::vector<std::string> model_names{"music_random_features", "simple_svm",
                                       "music_cnn"};
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, model_names, input_type, policy,
                              default_output, latency_slo_micros));
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
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  ASSERT_TRUE(delete_application(*redis_, name));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelVersionAdd) {
  std::string model_name = "mymodel";
  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_version_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &recv, model_name](
          const std::string& key, const std::string& event_type) {
        ASSERT_EQ(event_type, "set");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, model_name);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 1));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelVersionChange) {
  std::string model_name = "mymodel";

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 1));

  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<bool> recv{false};
  subscribe_to_model_version_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &recv, model_name](
          const std::string& key, const std::string& event_type) {
        ASSERT_EQ(event_type, "set");
        std::unique_lock<std::mutex> l(notification_mutex);
        recv = true;
        ASSERT_EQ(key, model_name);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, 2));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, LabelsToStr) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  ASSERT_EQ(labels_to_str(labels), "ads,images,experimental,other,labels");

  labels.clear();
  ASSERT_EQ(labels_to_str(labels), "");
}

TEST_F(RedisTest, ModelsToStr) {
  std::vector<VersionedModelId> models{
      std::make_pair("music_random_features", 1),
      std::make_pair("simple_svm", 2), std::make_pair("music_cnn", 4)};

  ASSERT_EQ(models_to_str(models),
            "music_random_features:1,simple_svm:2,music_cnn:4");

  models.clear();
  ASSERT_EQ(models_to_str(models), "");
}

}  // namespace
