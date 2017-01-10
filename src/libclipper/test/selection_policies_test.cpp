#include <gtest/gtest.h>

#include <clipper/datatypes.hpp>
#include <clipper/selection_policy.hpp>

using namespace clipper;

TEST(BanditPolicy, TestSerializeDeserializeBanditState) {
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

TEST(BanditPolicy, TestBanditPolicyProcessFeedback) {
  VersionedModelId good_model = std::make_pair("gm", 1);
  VersionedModelId bad_model = std::make_pair("bm", 1);
  BanditState state = BanditPolicy::initialize({good_model, bad_model});
  Feedback f1pos =
      std::make_pair(std::shared_ptr<Input>(), Output(1.0, good_model));

  Feedback f2pos =
      std::make_pair(std::shared_ptr<Input>(), Output(1.0, bad_model));

  Feedback f3neg =
      std::make_pair(std::shared_ptr<Input>(), Output(0.0, good_model));

  // bad model incorrect, good model correct
  state = BanditPolicy::process_feedback(
      state, f1pos, {Output(1.0, good_model), Output(0.0, bad_model)});

  // both models correct
  state = BanditPolicy::process_feedback(
      state, f2pos, {Output(1.0, good_model), Output(1.0, bad_model)});

  // bad model incorrect, good model correct
  state = BanditPolicy::process_feedback(
      state, f3neg, {Output(0.0, good_model), Output(1.0, bad_model)});

  std::cout << BanditPolicy::serialize_state(state);

  ASSERT_EQ(state[0].first, good_model);
  ASSERT_EQ(state[1].first, bad_model);
  ASSERT_GT(state[0].second, state[1].second);
  ASSERT_FLOAT_EQ(state[0].second + state[1].second, 1.0);
}
