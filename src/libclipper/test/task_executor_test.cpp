#include <gtest/gtest.h>
#include <chrono>
#include <sstream>

#include <boost/optional.hpp>

#include <clipper/containers.hpp>
#include <clipper/datatypes.hpp>
#include <clipper/task_executor.hpp>

using namespace clipper;

namespace {

/**
 * Creates a predict task with the specified query_id.
 * Specification of this id is used to disambiguate
 * predict tasks for testing (i.e. task queuing order stability)
 */
PredictTask create_predict_task(long query_id, long latency_slo_millis) {
  std::vector<double> data;
  VersionedModelId model_id = VersionedModelId("test", "1");
  data.push_back(1.0);
  std::shared_ptr<Input> input = std::make_shared<DoubleVector>(data);
  PredictTask task(input, model_id, 1.0, query_id, latency_slo_millis);
  return task;
}

TEST(TaskExecutorTests, TestDeadlineComparisonsWorkCorrectly) {
  Deadline current_time = std::chrono::system_clock::now();
  Deadline earlier = current_time - std::chrono::hours(1);
  Deadline later = current_time + std::chrono::hours(1);

  PredictTask task = create_predict_task(1, 10000);
  std::pair<Deadline, PredictTask> deadline_task_pair_current =
      std::make_pair(current_time, task);
  std::pair<Deadline, PredictTask> deadline_task_pair_earlier =
      std::make_pair(earlier, task);
  std::pair<Deadline, PredictTask> deadline_task_pair_later =
      std::make_pair(later, task);

  DeadlineCompare deadline_compare;
  bool current_greater_than_earlier =
      deadline_compare(deadline_task_pair_current, deadline_task_pair_earlier);
  bool earlier_greater_than_current =
      deadline_compare(deadline_task_pair_earlier, deadline_task_pair_current);
  ASSERT_TRUE(current_greater_than_earlier);
  ASSERT_FALSE(earlier_greater_than_current);

  bool current_greater_than_later =
      deadline_compare(deadline_task_pair_current, deadline_task_pair_later);
  bool later_greater_than_current =
      deadline_compare(deadline_task_pair_later, deadline_task_pair_current);
  ASSERT_FALSE(current_greater_than_later);
  ASSERT_TRUE(later_greater_than_current);

  bool earlier_greater_than_later =
      deadline_compare(deadline_task_pair_earlier, deadline_task_pair_later);
  bool later_greater_than_earlier =
      deadline_compare(deadline_task_pair_later, deadline_task_pair_earlier);
  ASSERT_FALSE(earlier_greater_than_later);
  ASSERT_TRUE(later_greater_than_earlier);
}

TEST(ModelQueueTests, TestGetBatchQueueNotEmpty) {
  PredictTask task_a = create_predict_task(1, 10000);
  PredictTask task_b = create_predict_task(2, 10000);
  PredictTask task_c = create_predict_task(3, 10000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  // Because we added tasks a through c in alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in the same order
  ASSERT_EQ(tasks[0].query_id_, task_a.query_id_);
  ASSERT_EQ(tasks[1].query_id_, task_b.query_id_);
  ASSERT_EQ(tasks[2].query_id_, task_c.query_id_);
}

TEST(ModelQueueTests, TestGetBatchOrdersByEarliestDeadline) {
  PredictTask task_a = create_predict_task(1, 20000);
  PredictTask task_b = create_predict_task(2, 10000);
  PredictTask task_c = create_predict_task(3, 30000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  // Because we added tasks a through c in alphabetical
  // order with the same latency slos, we expect the model
  // queue's get_batch() function to return them in the same order
  ASSERT_EQ(tasks[0].query_id_, task_b.query_id_);
  ASSERT_EQ(tasks[1].query_id_, task_a.query_id_);
  ASSERT_EQ(tasks[2].query_id_, task_c.query_id_);
}

TEST(ModelQueueTests, TestGetBatchRemovesTasksWithElapsedDeadline) {
  PredictTask task_a = create_predict_task(1, 0);
  PredictTask task_b = create_predict_task(2, 0);
  PredictTask task_c = create_predict_task(3, 10000);

  ModelQueue model_queue;

  model_queue.add_task(task_a);
  model_queue.add_task(task_b);
  model_queue.add_task(task_c);

  std::vector<PredictTask> tasks =
      model_queue.get_batch([](Deadline) { return 3; });

  ASSERT_EQ(tasks.size(), (size_t)1);
  ASSERT_EQ(tasks[0].query_id_, task_c.query_id_);
}

TEST(PredictionCacheTests,
     TestOutputDataSizeDoesNotIncreaseBeyondMaximumCacheSize) {
  std::vector<std::shared_ptr<Input>> inputs;
  std::vector<Output> outputs;
  size_t cache_size = 0;
  for (int i = 0; i < 4; ++i) {
    inputs.push_back(std::make_shared<IntVector>(std::vector<int>{i}));
    Output output(std::to_string(i), std::vector<VersionedModelId>{});
    size_t output_size = output.y_hat_.size();
    outputs.push_back(output);
    cache_size += output_size;
  }
  VersionedModelId model_id("TEST", "1");
  PredictionCache cache(cache_size);
  for (size_t i = 0; i < inputs.size(); ++i) {
    cache.put(model_id, inputs[i], outputs[i]);
    auto output_future = cache.fetch(model_id, inputs[i]);
    ASSERT_TRUE(output_future.isReady());
    ASSERT_EQ(output_future.get().y_hat_, outputs[i].y_hat_);
  }
  std::shared_ptr<Input> last_input =
      std::make_shared<IntVector>(std::vector<int>{5});
  cache.put(model_id, last_input, outputs[0]);
  auto last_output_future = cache.fetch(model_id, last_input);
  ASSERT_TRUE(last_output_future.isReady());
  ASSERT_EQ(last_output_future.get().y_hat_, outputs[0].y_hat_);
  // Inserting a fifth entry should have brought the cache size above the
  // maximum
  // threshold. In accordance with the clock eviction policy, the output
  // corresponding
  // to inputs[0] at page buffer index 0 should have been replaced with the
  // output
  // corresponding to `last_input`, which is outputs[0]
  ASSERT_FALSE(cache.fetch(model_id, inputs[0]).isReady());
  // The cache should still contain entries corresponding to input indices 1, 2,
  // and 3
  for (int i = 1; i < 4; ++i) {
    auto output_future = cache.fetch(model_id, inputs[i]);
    ASSERT_TRUE(output_future.isReady());
    ASSERT_EQ(output_future.get().y_hat_, outputs[i].y_hat_);
  }
}

TEST(PredictionCacheTests,
     TestEvictionPolicyConsistentWithVariableSizeClockMultipleFetchesAndPuts) {
  std::string small_output_text("text");
  std::string large_output_text("texttext");
  ASSERT_EQ(large_output_text.size(), small_output_text.size() * 2);

  std::vector<Output> small_outputs;
  std::vector<std::shared_ptr<Input>> small_inputs;
  for (int i = 0; i < 5; ++i) {
    std::shared_ptr<Input> input =
        std::make_shared<IntVector>(std::vector<int>{i});
    Output output(small_output_text, std::vector<VersionedModelId>{});
    small_inputs.push_back(input);
    small_outputs.push_back(output);
  }

  std::vector<Output> large_outputs;
  std::vector<std::shared_ptr<Input>> large_inputs;
  for (int i = 0; i < 2; ++i) {
    std::shared_ptr<Input> input =
        std::make_shared<IntVector>(std::vector<int>{i, i});
    Output output(large_output_text, std::vector<VersionedModelId>{});
    large_inputs.push_back(input);
    large_outputs.push_back(output);
  }

  PredictionCache cache((3 * small_output_text.size()) +
                        (2 * large_output_text.size()));
  VersionedModelId model_id("TEST", "1");
  cache.put(model_id, large_inputs[0], large_outputs[0]);
  cache.put(model_id, large_inputs[1], large_outputs[1]);
  for (int i = 0; i < 3; ++i) {
    cache.put(model_id, small_inputs[i], small_outputs[1]);
  }

  // The cache should contain both large output entries
  auto first_large_output_future = cache.fetch(model_id, large_inputs[0]);
  ASSERT_TRUE(first_large_output_future.isReady());
  ASSERT_EQ(first_large_output_future.get().y_hat_, large_outputs[0].y_hat_);
  auto second_large_output_future = cache.fetch(model_id, large_inputs[1]);
  ASSERT_TRUE(second_large_output_future.isReady());
  ASSERT_EQ(second_large_output_future.get().y_hat_, large_outputs[1].y_hat_);

  // The cache should contain the three previously-inserted small output
  // entries, but it
  // should not contain any other small output entries
  for (int i = 0; i < 3; ++i) {
    auto small_output_future = cache.fetch(model_id, small_inputs[i]);
    ASSERT_TRUE(small_output_future.isReady());
    ASSERT_EQ(small_output_future.get().y_hat_, small_outputs[i].y_hat_);
  }
  ASSERT_FALSE(cache.fetch(model_id, small_inputs[3]).isReady());
  ASSERT_FALSE(cache.fetch(model_id, small_inputs[4]).isReady());

  // Inserting the small output entry `small_outputs_[3]` should
  // set the used bit of every page in the buffer to zero. Then, the first
  // large output entry containing `large_outputs[0]` at page buffer index 0
  // should be replaced with a new entry containing `small_outputs[3]`
  cache.put(model_id, small_inputs[3], small_outputs[3]);
  ASSERT_FALSE(cache.fetch(model_id, large_inputs[0]).isReady());
  ASSERT_TRUE(cache.fetch(model_id, large_inputs[1]).isReady());

  // Because large entries are double the size of small entries, the previous
  // eviction of a large entry should leave enough space for the small entry
  // corresponding to `small_outputs[4]` to be inserted. The cache should now
  // contain entries corresponding to all five small outputs, as well as
  // large_outputs[1]
  cache.put(model_id, small_inputs[4], small_outputs[4]);
  ASSERT_FALSE(cache.fetch(model_id, large_inputs[0]).isReady());
  ASSERT_TRUE(cache.fetch(model_id, large_inputs[1]).isReady());
  for (int i = 0; i < 5; ++i) {
    auto small_output_future = cache.fetch(model_id, small_inputs[i]);
    ASSERT_TRUE(small_output_future.isReady());
    ASSERT_EQ(small_output_future.get().y_hat_, small_outputs[i].y_hat_);
  }

  // Inserting an additional small output should evict the entry containing
  // large_outputs[1] at page buffer index 2. The cache should now contain
  // entries corresponding to small outputs at vector indices 0-4, as well as
  // an additional entry corresponding to small_outputs[0] that we just inserted
  std::shared_ptr<Input> last_small_input =
      std::make_shared<IntVector>(std::vector<int>{6});
  cache.put(model_id, last_small_input, small_outputs[0]);
  ASSERT_FALSE(cache.fetch(model_id, large_inputs[1]).isReady());
  for (int i = 0; i < 5; ++i) {
    auto small_output_future = cache.fetch(model_id, small_inputs[i]);
    ASSERT_TRUE(small_output_future.isReady());
    ASSERT_EQ(small_output_future.get().y_hat_, small_outputs[i].y_hat_);
  }
  auto last_small_output_future = cache.fetch(model_id, last_small_input);
  ASSERT_TRUE(last_small_output_future.isReady());
  ASSERT_EQ(last_small_output_future.get().y_hat_, small_outputs[0].y_hat_);

  // Inserting an additional small large input should evict the entry containing
  // small_outputs[0] at page buffer index 3. The cache should now contain
  // entries corresponding to small outputs at vector indices 1-4, as well as
  // an additional entries corresponding to: large_outputs[0] that (just
  // inserted)
  // and small_outputs[0] (inserted in the previous step)
  std::shared_ptr<Input> last_large_input =
      std::make_shared<IntVector>(std::vector<int>{3, 3});
  cache.put(model_id, last_large_input, large_outputs[0]);
  ASSERT_FALSE(cache.fetch(model_id, large_inputs[0]).isReady());
  for (int i = 1; i < 5; ++i) {
    auto small_output_future = cache.fetch(model_id, small_inputs[i]);
    ASSERT_TRUE(small_output_future.isReady());
    ASSERT_EQ(small_output_future.get().y_hat_, small_outputs[i].y_hat_);
  }
  auto last_large_output_future = cache.fetch(model_id, last_large_input);
  ASSERT_TRUE(last_large_output_future.isReady());
  ASSERT_EQ(last_large_output_future.get().y_hat_, large_outputs[0].y_hat_);
}

TEST(PredictionCacheTests, TestIncompleteFuturesAreCompletedOnPut) {
  std::shared_ptr<Input> input =
      std::make_shared<IntVector>(std::vector<int>{10});
  std::string output_text("1234");
  Output output(output_text, std::vector<VersionedModelId>{});

  VersionedModelId model_id("TEST", "1");
  PredictionCache cache1(output_text.size());

  auto output_future = cache1.fetch(model_id, input);
  ASSERT_FALSE(output_future.isReady());

  // cache1 is large enough to contain the output entry that is being inserted,
  // so subsequent lookups should succeed
  cache1.put(model_id, input, output);
  for (int i = 0; i < 5; ++i) {
    output_future = cache1.fetch(model_id, input);
    ASSERT_TRUE(output_future.isReady());
    ASSERT_EQ(output_future.get().y_hat_, output.y_hat_);
  }

  ASSERT_GT(output_text.size(), 1UL);
  PredictionCache cache2(1);

  output_future = cache2.fetch(model_id, input);
  ASSERT_FALSE(output_future.isReady());
  // cache2 is too small to contain the output entry being inserted. Executing
  // `put` should still complete the previously-constructed future, but all
  // subsequent lookups should fail
  cache2.put(model_id, input, output);
  ASSERT_TRUE(output_future.isReady());
  ASSERT_EQ(output_future.get().y_hat_, output.y_hat_);
  for (int i = 0; i < 5; ++i) {
    output_future = cache2.fetch(model_id, input);
    ASSERT_FALSE(output_future.isReady());
  }

  PredictionCache cache3(output_text.size());
  output_future = cache3.fetch(model_id, input);
  ASSERT_FALSE(output_future.isReady());
  for (int i = 0; i < 5; ++i) {
    std::shared_ptr<Input> new_input =
        std::make_shared<IntVector>(std::vector<int>{i});
    Output new_output("12", std::vector<VersionedModelId>{});
    cache3.put(model_id, new_input, new_output);
  }
  ASSERT_FALSE(output_future.isReady());
  cache3.put(model_id, input, output);
  // After inserting several other elements, a final `put()` call should
  // still complete the output future that we obtained prior to the additional
  // insertions
  ASSERT_TRUE(output_future.isReady());
  ASSERT_EQ(output_future.get().y_hat_, output.y_hat_);
}

TEST(PredictionCacheTests, TestEntryLargerThanCacheSizeIsEvicted) {
  VersionedModelId model_id("TEST", "1");
  PredictionCache cache(0);
  std::shared_ptr<Input> input =
      std::make_shared<IntVector>(std::vector<int>{0});
  Output output("text", std::vector<VersionedModelId>{});
  cache.put(model_id, input, output);
  ASSERT_FALSE(cache.fetch(model_id, input).isReady());
}
}  // namespace
