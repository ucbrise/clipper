#include <gtest/gtest.h>

#include <clipper/config.hpp>
#include <clipper/persistent_state.hpp>
#include <clipper/redis.hpp>
#include <redox.hpp>

using namespace clipper;
using namespace clipper::redis;

namespace {

class StateDBTest : public ::testing::Test {
 public:
  StateDBTest() : redis_(std::make_shared<redox::Redox>()) {
    Config& conf = get_config();
    redis_->connect(conf.get_redis_address(), conf.get_redis_port());
    // delete all keys
    send_cmd_no_reply<std::string>(*redis_, {"FLUSHALL"});
  }

  StateDB db_;
  std::shared_ptr<redox::Redox> redis_;

  virtual ~StateDBTest() { redis_->disconnect(); }
};

TEST_F(StateDBTest, TestSinglePutGet) {
  StateKey key = std::make_tuple("Dan", 13622, 32432432);
  ASSERT_FALSE(db_.get(key));
  ASSERT_TRUE(db_.put(key, "valuestring"));
  ASSERT_EQ(db_.num_entries(), 1);
  auto v = db_.get(key);
  ASSERT_TRUE(v);
  ASSERT_EQ("valuestring", *v);
  ASSERT_TRUE(db_.remove(key));
  ASSERT_EQ(db_.num_entries(), 0);
}

TEST_F(StateDBTest, TestManyPutGet) {
  ASSERT_EQ(db_.num_entries(), 0);
  for (int i = 0; i < 100; ++i) {
    StateKey key = std::make_tuple("Dan", i, 1234);
    ASSERT_TRUE(db_.put(key, "valuestring"));
  }
  ASSERT_EQ(db_.num_entries(), 100);
  for (int i = 0; i < 100; ++i) {
    StateKey key = std::make_tuple("Dan", i, 1234);
    ASSERT_TRUE(db_.remove(key));
  }
  ASSERT_EQ(db_.num_entries(), 0);
}

TEST_F(StateDBTest, TestPutRemoveGet) {
  StateKey key = std::make_tuple("Corey", 13622, 32432432);
  ASSERT_FALSE(db_.get(key));
  ASSERT_TRUE(db_.put(key, "valuestring"));
  ASSERT_EQ(db_.num_entries(), 1);
  auto v = db_.get(key);
  ASSERT_TRUE(v);
  ASSERT_TRUE(db_.remove(key));
  ASSERT_FALSE(db_.get(key));
}

}  // namespace
