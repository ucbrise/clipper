#include <gtest/gtest.h>
#include <time.h>
#include <chrono>
#include <ctime>
#include <thread>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real_distribution.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/selection_policies.hpp>

using namespace clipper;
const std::string LOGGING_TAG_SELECTION_POLICY_TEST = "POLICY TEST";

namespace {

class DefaultOutputSelectionPolicyTest : public ::testing::Test {
 public:
  DefaultOutputSelectionPolicyTest()
      : state_(
            std::make_shared<DefaultOutputSelectionState>(Output{"4.3", {}})) {}
  DefaultOutputSelectionPolicy policy_;
  std::shared_ptr<DefaultOutputSelectionState> state_;
};

TEST_F(DefaultOutputSelectionPolicyTest,
       TestSelectPredictTasksZeroCandidateModels) {
  Query zero_candidate_models_query{"label",
                                    clipper::DEFAULT_USER_ID,
                                    std::shared_ptr<DoubleVector>(),
                                    1000,
                                    DefaultOutputSelectionPolicy::get_name(),
                                    {}};
  auto zero_models_tasks =
      policy_.select_predict_tasks(nullptr, zero_candidate_models_query, 0);
  EXPECT_EQ(zero_models_tasks.size(), (size_t)0);
}

TEST_F(DefaultOutputSelectionPolicyTest,
       TestSelectPredictTasksTwoCandidateModels) {
  std::vector<VersionedModelId> two_models{
      VersionedModelId("music_random_features", "1"),
      VersionedModelId("simple_svm", "2")};
  Query two_candidate_models_query{"label",
                                   clipper::DEFAULT_USER_ID,
                                   std::shared_ptr<DoubleVector>(),
                                   1000,
                                   DefaultOutputSelectionPolicy::get_name(),
                                   two_models};
  auto two_models_tasks =
      policy_.select_predict_tasks(nullptr, two_candidate_models_query, 0);
  EXPECT_EQ(two_models_tasks.size(), (size_t)1);
  EXPECT_EQ(two_models_tasks.front().model_, two_models.front());
}

TEST_F(DefaultOutputSelectionPolicyTest,
       TestSelectPredictTasksOneCandidateModel) {
  std::vector<VersionedModelId> one_model{
      VersionedModelId("music_random_features", "1")};
  Query one_candidate_model_query{"label",
                                  clipper::DEFAULT_USER_ID,
                                  std::shared_ptr<DoubleVector>(),
                                  1000,
                                  DefaultOutputSelectionPolicy::get_name(),
                                  one_model};
  auto one_model_tasks =
      policy_.select_predict_tasks(nullptr, one_candidate_model_query, 0);
  EXPECT_EQ(one_model_tasks.size(), (size_t)1);
  EXPECT_EQ(one_model_tasks.front().model_, one_model.front());
}

TEST_F(DefaultOutputSelectionPolicyTest,
       TestCombinePredictionsZeroPredictions) {
  VersionedModelId m1 = VersionedModelId("music_random_features", "1");
  Query one_candidate_model_query{"label",
                                  clipper::DEFAULT_USER_ID,
                                  std::shared_ptr<DoubleVector>(),
                                  1000,
                                  DefaultOutputSelectionPolicy::get_name(),
                                  {m1}};
  auto zero_preds_output =
      policy_.combine_predictions(state_, one_candidate_model_query, {}).first;
  ASSERT_EQ(zero_preds_output, state_->default_output_);
}

TEST_F(DefaultOutputSelectionPolicyTest, TestCombinePredictionsOnePrediction) {
  VersionedModelId m1 = VersionedModelId("music_random_features", "1");
  Query one_candidate_model_query{"label",
                                  clipper::DEFAULT_USER_ID,
                                  std::shared_ptr<DoubleVector>(),
                                  1000,
                                  DefaultOutputSelectionPolicy::get_name(),
                                  {m1}};

  Output first_output = Output{"1.1", {m1}};
  auto one_pred_output =
      policy_
          .combine_predictions(state_, one_candidate_model_query,
                               {first_output})
          .first;
  ASSERT_EQ(one_pred_output, first_output);
  ASSERT_NE(one_pred_output, state_->default_output_);
}

TEST_F(DefaultOutputSelectionPolicyTest, TestCombinePredictionsTwoPredictions) {
  VersionedModelId m1 = VersionedModelId("music_random_features", "1");
  VersionedModelId m2 = VersionedModelId("simple_svm", "2");
  Query two_candidate_models_query{"label",
                                   clipper::DEFAULT_USER_ID,
                                   std::shared_ptr<DoubleVector>(),
                                   1000,
                                   DefaultOutputSelectionPolicy::get_name(),
                                   {m1, m2}};
  Output first_output = Output{"1.1", {m1}};
  Output second_output = Output{"2.2", {m2}};
  auto two_preds_output =
      policy_
          .combine_predictions(state_, two_candidate_models_query,
                               {first_output, second_output})
          .first;
  ASSERT_EQ(two_preds_output, first_output);
  ASSERT_NE(two_preds_output, state_->default_output_);
}

TEST(DefaultOutputSelectionStateTest, Serialization) {
  Output output{"4.3", {}};
  DefaultOutputSelectionState state{output};
  std::string serialized_output = state.serialize();
  DefaultOutputSelectionState deserialized_state{serialized_output};
  auto& output_y_hat = output.y_hat_;
  auto& default_y_hat = deserialized_state.default_output_.y_hat_;
  ASSERT_EQ(output_y_hat->type(), default_y_hat->type());
  ASSERT_EQ(output_y_hat->byte_size(), default_y_hat->byte_size());
  ASSERT_EQ(output_y_hat->size(), default_y_hat->size());
  SharedPoolPtr<char> output_y_hat_data = get_data<char>(output_y_hat);
  SharedPoolPtr<char> default_y_hat_data = get_data<char>(default_y_hat);
  for (size_t i = 0; i < output_y_hat->size(); ++i) {
    ASSERT_EQ(output_y_hat_data.get()[i], default_y_hat_data.get()[i]);
  }
}

}  // namespace
