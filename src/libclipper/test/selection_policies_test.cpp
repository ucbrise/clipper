#include <gtest/gtest.h>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>

using namespace clipper;

TEST(BanditPolicy, SerializeDeserializeBanditState) {
  std::string m1_name = "my_model";
  std::string m2_name = "other_name";
  std::string m3_name = "images_cnn";
  BanditState test_state{std::make_pair(std::make_pair(m1_name, 4), 1.4),
                         std::make_pair(std::make_pair(m2_name, 7), 0.13),
                         std::make_pair(std::make_pair(m3_name, 1), 9.6)};
  std::string ser_state = BanditPolicy::serialize_state(test_state);
  std::string expected_ser_state =
      "my_model:4:1.400000,other_name:7:0.130000,images_cnn:1:9.600000";
  EXPECT_EQ(ser_state, expected_ser_state);
  BanditState deser_state = BanditPolicy::deserialize_state(ser_state);
  ASSERT_EQ(test_state, deser_state);
}
