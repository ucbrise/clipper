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

TEST_F(RedisTest, ParseModelReplicaKey) {
  VersionedModelId model = VersionedModelId("model1", "4");
  int replica_id = 7;
  std::string key = gen_model_replica_key(model, replica_id);
  std::pair<VersionedModelId, int> parse_result = parse_model_replica_key(key);
  ASSERT_EQ(parse_result.first, model);
  ASSERT_EQ(parse_result.second, replica_id);
}

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
  VersionedModelId model = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
  auto result = get_model(*redis_, model);
  // The model table has 9 fields, so we expect
  // to get back a map with 9 entries in it
  // (see add_model() in redis.cpp for details on what the fields are).
  EXPECT_EQ(result.size(), static_cast<size_t>(9));
  ASSERT_EQ(result["model_name"], model.get_name());
  ASSERT_EQ(result["model_version"], model.get_id());
  ASSERT_FLOAT_EQ(std::stof(result["load"]), 0.0);
  ASSERT_EQ(str_to_labels(result["labels"]), labels);
  ASSERT_EQ(parse_input_type(result["input_type"]), InputType::Ints);
  ASSERT_EQ(result["container_name"], container_name);
  ASSERT_EQ(result["model_data_path"], model_path);
}

TEST_F(RedisTest, AddModelLinks) {
  std::string app_name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = DefaultOutputSelectionPolicy::get_name();
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, app_name, input_type, policy,
                              default_output, latency_slo_micros));

  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  std::string model_name_1 = "model_1";
  std::string model_name_2 = "model_2";
  VersionedModelId model_1 = VersionedModelId(model_name_1, "1");
  VersionedModelId model_2 = VersionedModelId(model_name_2, "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model_1, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
  ASSERT_TRUE(add_model(*redis_, model_2, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));

  std::vector<std::string> model_names =
      std::vector<std::string>{model_name_1, model_name_2};
  ASSERT_TRUE(add_model_links(*redis_, app_name, model_names));

  auto linked_models = get_linked_models(*redis_, app_name);
  std::sort(linked_models.begin(), linked_models.end());
  std::sort(model_names.begin(), model_names.end());
  ASSERT_EQ(model_names, linked_models);
}

TEST_F(RedisTest, DeleteModelLinks) {
  std::string app_name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = DefaultOutputSelectionPolicy::get_name();
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, app_name, input_type, policy,
                              default_output, latency_slo_micros));

  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  std::string model_name_1 = "model_1";
  std::string model_name_2 = "model_2";
  VersionedModelId model_1 = VersionedModelId(model_name_1, "1");
  VersionedModelId model_2 = VersionedModelId(model_name_2, "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model_1, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
  ASSERT_TRUE(add_model(*redis_, model_2, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));

  std::vector<std::string> model_names =
      std::vector<std::string>{model_name_1, model_name_2};
  ASSERT_TRUE(add_model_links(*redis_, app_name, model_names));
  ASSERT_TRUE(delete_model_links(*redis_, app_name, model_names));

  auto linked_models = get_linked_models(*redis_, app_name);
  ASSERT_EQ(linked_models.size(), (size_t)0);
}

TEST_F(RedisTest, SetCurrentModelVersion) {
  std::string model_name = "mymodel";
  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "2"));
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), "2");

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "5"));
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), "5");

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "3"));
  ASSERT_EQ(*get_current_model_version(*redis_, model_name), "3");
}

TEST_F(RedisTest, GetModelVersions) {
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));
  VersionedModelId model2 = VersionedModelId("m", "2");
  std::string model_path2 = "/tmp/models/m/2";
  ASSERT_TRUE(add_model(*redis_, model2, InputType::Ints, labels,
                        container_name, model_path2, DEFAULT_BATCH_SIZE));
  VersionedModelId model4 = VersionedModelId("m", "4");
  std::string model_path4 = "/tmp/models/m/4";
  ASSERT_TRUE(add_model(*redis_, model4, InputType::Ints, labels,
                        container_name, model_path4, DEFAULT_BATCH_SIZE));

  std::vector<std::string> versions = get_model_versions(*redis_, "m");
  ASSERT_EQ(versions.size(), (size_t)3);
  std::sort(versions.begin(), versions.end());
  ASSERT_EQ(versions, std::vector<std::string>({"1", "2", "4"}));
}

TEST_F(RedisTest, GetAllModelNames) {
  // Add multiple models (some with multiple versions)
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));
  VersionedModelId model2 = VersionedModelId("m", "2");
  std::string model_path2 = "/tmp/models/m/2";
  ASSERT_TRUE(add_model(*redis_, model2, InputType::Ints, labels,
                        container_name, model_path2, DEFAULT_BATCH_SIZE));
  VersionedModelId model3 = VersionedModelId("n", "3");
  std::string model_path3 = "/tmp/models/n/3";
  ASSERT_TRUE(add_model(*redis_, model3, InputType::Ints, labels,
                        container_name, model_path3, DEFAULT_BATCH_SIZE));

  // get_all_model_names() should return the de-duplicated model names
  std::vector<std::string> names = get_all_model_names(*redis_);
  ASSERT_EQ(names.size(), static_cast<size_t>(2));
  std::sort(names.begin(), names.end());
  ASSERT_EQ(names, std::vector<std::string>({"m", "n"}));
}

TEST_F(RedisTest, GetAllModels) {
  // Add multiple models (some with multiple versions)
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  VersionedModelId model1 = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model1, InputType::Ints, labels,
                        container_name, model_path, DEFAULT_BATCH_SIZE));
  VersionedModelId model2 = VersionedModelId("m", "2");
  std::string model_path2 = "/tmp/models/m/2";
  ASSERT_TRUE(add_model(*redis_, model2, InputType::Ints, labels,
                        container_name, model_path2, DEFAULT_BATCH_SIZE));
  VersionedModelId model3 = VersionedModelId("n", "3");
  std::string model_path3 = "/tmp/models/n/3";
  ASSERT_TRUE(add_model(*redis_, model3, InputType::Ints, labels,
                        container_name, model_path3, DEFAULT_BATCH_SIZE));

  // get_all_model_names() should return the de-duplicated model names
  std::vector<VersionedModelId> models = get_all_models(*redis_);
  ASSERT_EQ(models.size(), static_cast<size_t>(3));

  bool model_found;
  std::vector<VersionedModelId> expected_models({model1, model2, model3});
  for (auto expected_model : expected_models) {
    model_found =
        std::find(models.begin(), models.end(), expected_model) != models.end();
    ASSERT_TRUE(model_found);
  }
}

TEST_F(RedisTest, DeleteModel) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
  auto add_result = get_model(*redis_, model);
  EXPECT_EQ(add_result.size(), static_cast<size_t>(9));
  ASSERT_TRUE(delete_versioned_model(*redis_, model));
  auto delete_result = get_model(*redis_, model);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, AddContainer) {
  VersionedModelId model = VersionedModelId("m", "1");
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
  EXPECT_EQ(result["model_name"], model.get_name());
  EXPECT_EQ(result["model_version"], model.get_id());
  EXPECT_EQ(result["model_id"], gen_versioned_model_key(model));
  EXPECT_EQ(std::stoi(result["model_replica_id"]), replica_id);
  EXPECT_EQ(std::stoi(result["zmq_connection_id"]), zmq_connection_id);
  EXPECT_EQ(std::stoi(result["batch_size"]), 1);
  EXPECT_EQ(parse_input_type(result["input_type"]), input_type);
}

TEST_F(RedisTest, GetAllContainers) {
  VersionedModelId model = VersionedModelId("m", "1");
  int zmq_connection_id = 0;
  InputType input_type = InputType::Doubles;
  ASSERT_TRUE(add_container(*redis_, model, 0, zmq_connection_id, input_type));

  ASSERT_TRUE(
      add_container(*redis_, model, 1, zmq_connection_id + 1, input_type));

  VersionedModelId model2 = VersionedModelId("other_model", "3");
  ASSERT_TRUE(
      add_container(*redis_, model2, 0, zmq_connection_id + 2, input_type));

  std::vector<std::pair<VersionedModelId, int>> containers =
      get_all_containers(*redis_);

  ASSERT_EQ(containers.size(), static_cast<size_t>(3));
  std::vector<std::pair<VersionedModelId, int>> expected_containers(
      {std::make_pair(model, 0), std::make_pair(model, 1),
       std::make_pair(model2, 0)});

  bool container_found;
  for (auto expected_container : expected_containers) {
    container_found = std::find(containers.begin(), containers.end(),
                                expected_container) != containers.end();
    ASSERT_TRUE(container_found);
  }
}

TEST_F(RedisTest, DeleteContainer) {
  VersionedModelId model = VersionedModelId("m", "1");
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
  InputType input_type = InputType::Doubles;
  std::string policy = DefaultOutputSelectionPolicy::get_name();
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));
  auto result = get_application(*redis_, name);
  // The application table has 4 fields, so we expect to get back a map with 4
  // entries in it (see add_application() in redis.cpp for details on what the
  // fields are).
  EXPECT_EQ(result.size(), static_cast<size_t>(4));
  EXPECT_EQ(parse_input_type(result["input_type"]), input_type);
  EXPECT_EQ(result["policy"], policy);
  EXPECT_EQ(result["default_output"], default_output);
  EXPECT_EQ(std::stoi(result["latency_slo_micros"]), latency_slo_micros);
}

TEST_F(RedisTest, DeleteApplication) {
  std::string name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));
  auto get_result = get_application(*redis_, name);
  EXPECT_EQ(get_result.size(), static_cast<size_t>(4));
  ASSERT_TRUE(delete_application(*redis_, name));
  auto delete_result = get_application(*redis_, name);
  EXPECT_EQ(delete_result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, GetAllApplicationNames) {
  // Add a few applications, get should return all of their names.
  std::string name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));
  std::string name2 = "my_app_name_2";
  InputType input_type2 = InputType::Doubles;
  std::string policy2 = "exp4_policy";
  int latency_slo_micros2 = 50000;
  std::string default_output2 = "1.0";
  ASSERT_TRUE(add_application(*redis_, name2, input_type2, policy2,
                              default_output2, latency_slo_micros2));

  std::vector<std::string> app_names = get_all_application_names(*redis_);
  ASSERT_EQ(app_names.size(), static_cast<size_t>(2));
  std::sort(app_names.begin(), app_names.end());
  ASSERT_EQ(app_names, std::vector<std::string>({name, name2}));
}

TEST_F(RedisTest, GetAllApplicationNamesNoneRegistered) {
  std::vector<std::string> result = get_all_application_names(*redis_);

  // No apps have been registered, so get_all_application_names should
  // return an empty vector
  ASSERT_EQ(result.size(), static_cast<size_t>(0));
}

TEST_F(RedisTest, SubscriptionDetectModelAdd) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = VersionedModelId("m", "1");
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
                        model_path, DEFAULT_BATCH_SIZE));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelDelete) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model = VersionedModelId("m", "1");
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model, InputType::Ints, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
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
  ASSERT_TRUE(delete_versioned_model(*redis_, model));
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectContainerAdd) {
  std::vector<std::string> labels{"ads", "images", "experimental"};
  VersionedModelId model_id = VersionedModelId("m", "1");
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
  VersionedModelId model_id = VersionedModelId("m", "1");
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

  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectApplicationDelete) {
  std::string name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));
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

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "1"));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(l, std::chrono::milliseconds(1000),
                                           [&recv]() { return recv == true; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelLinksAdd) {
  // Register the application to link to
  std::string name = "my_app_name";
  InputType input_type = InputType::Doubles;
  std::string policy = "exp3_policy";
  std::string default_output = "1.0";
  int latency_slo_micros = 10000;
  ASSERT_TRUE(add_application(*redis_, name, input_type, policy, default_output,
                              latency_slo_micros));

  // Register the models to link
  std::vector<std::string> labels{"ads", "images", "experimental", "other",
                                  "labels"};
  std::string model_name_1 = "model_1";
  std::string model_name_2 = "model_2";
  std::string model_version = "1";
  VersionedModelId model_1_id = VersionedModelId(model_name_1, model_version);
  VersionedModelId model_2_id = VersionedModelId(model_name_2, model_version);
  std::string container_name = "clipper/test_container";
  std::string model_path = "/tmp/models/m/1";
  ASSERT_TRUE(add_model(*redis_, model_1_id, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));
  ASSERT_TRUE(add_model(*redis_, model_2_id, input_type, labels, container_name,
                        model_path, DEFAULT_BATCH_SIZE));

  std::condition_variable_any notification_recv;
  std::mutex notification_mutex;
  std::atomic<int> num_sadd_recv{0};
  subscribe_to_model_link_changes(
      *subscriber_,
      [&notification_recv, &notification_mutex, &num_sadd_recv, name](
          const std::string& key, const std::string& event_type) {
        ASSERT_EQ(event_type, "sadd");
        std::unique_lock<std::mutex> l(notification_mutex);
        num_sadd_recv += 1;
        ASSERT_EQ(key, name);
        notification_recv.notify_all();
      });
  // give Redis some time to register the subscription
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  std::vector<std::string> model_names = {model_name_1, model_name_2};
  ASSERT_TRUE(add_model_links(*redis_, name, model_names));

  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_recv.wait_for(
      l, std::chrono::milliseconds(1000),
      [&num_sadd_recv]() { return num_sadd_recv == 2; });
  ASSERT_TRUE(result);
}

TEST_F(RedisTest, SubscriptionDetectModelVersionChange) {
  std::string model_name = "mymodel";

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "1"));

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

  ASSERT_TRUE(set_current_model_version(*redis_, model_name, "2"));

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
      VersionedModelId("music_random_features", "1"),
      VersionedModelId("simple_svm", "2"), VersionedModelId("music_cnn", "4")};

  ASSERT_EQ(models_to_str(models),
            "music_random_features:1,simple_svm:2,music_cnn:4");

  models.clear();
  ASSERT_EQ(models_to_str(models), "");
}

}  // namespace
