#include <gtest/gtest.h>

#include <clipper/config.hpp>
#include <clipper/persistent_state.hpp>

using namespace clipper;

namespace {

class StateDBTest : public ::testing::Test {
 public:
  StateDBTest() {}

  StateDB db_;
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
  for (int i = 0; i < 5000; ++i) {
    StateKey key = std::make_tuple("Dan", i, 1234);
    ASSERT_TRUE(db_.put(key, "valuestring"));
  }
  ASSERT_EQ(db_.num_entries(), 5000);
  for (int i = 0; i < 5000; ++i) {
    StateKey key = std::make_tuple("Dan", i, 1234);
    ASSERT_TRUE(db_.remove(key));
  }
  ASSERT_EQ(db_.num_entries(), 0);
}

}  // namespace
