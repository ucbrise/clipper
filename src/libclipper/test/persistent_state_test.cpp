#include <gtest/gtest.h>

#include <clipper/persistent_state.hpp>

using namespace clipper;

class StateDBTest : public ::testing::Test {
 public:
  StateDBTest() {}

  StateDB db_;
};

TEST_F(StateDBTest, TestInit) {
  StateKey key = std::make_tuple("Dan", 13622, 32432432);
  ASSERT_FALSE(db_.get(key));
  ASSERT_FALSE(db_.put(key, "valuestring"));
  ASSERT_FALSE(db_.get(key));
}

TEST_F(StateDBTest, TestPutGet) {
  ASSERT_TRUE(db_.init());
  StateKey key = std::make_tuple("Dan", 13622, 32432432);
  ASSERT_FALSE(db_.get(key));
  ASSERT_TRUE(db_.put(key, "valuestring"));
  auto v = db_.get(key);
  ASSERT_TRUE(v);
  ASSERT_EQ("valuestring", *v);
  ASSERT_TRUE(db_.delete_key(key));
}
