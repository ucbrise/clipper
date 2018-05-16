#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <random>
#include <thread>

#include <clipper/threadpool.hpp>

using namespace clipper;
using namespace std::chrono_literals;

namespace {

void task_hangs(std::atomic<int>& counter) {
  std::this_thread::sleep_for(10s);
  counter.fetch_add(1);
}

void task_completes(std::atomic<int>& counter) {
  std::this_thread::sleep_for(500us);
  counter.fetch_add(1);
}

TEST(ThreadPoolTests, TestSingleQueueSingleJob) {
  ModelQueueThreadPool threadpool;
  VersionedModelId vm = VersionedModelId("m", "1");
  int replica_id = 17;
  ASSERT_TRUE(threadpool.create_queue(vm, replica_id, false));
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit(vm, replica_id, [&counter] { task_completes(counter); });
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result =
      notification_counter.wait_for(l, std::chrono::milliseconds(2000),
                                    [&counter]() { return counter == 1; });
  ASSERT_TRUE(result);
}

TEST(ThreadPoolTests, TestSingleQueueManyJobs) {
  int num_tasks = 500;
  ModelQueueThreadPool threadpool;
  VersionedModelId vm = VersionedModelId("m", "1");
  int replica_id = 17;
  ASSERT_TRUE(threadpool.create_queue(vm, replica_id, false));
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit(vm, replica_id, [&counter] { task_completes(counter); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter, &num_tasks]() { return counter == num_tasks; });
  ASSERT_TRUE(result);
}

TEST(ThreadPoolTests, TestSingleQueueJobHangs) {
  int num_tasks = 500;
  ModelQueueThreadPool threadpool;
  VersionedModelId vm = VersionedModelId("m", "1");
  int replica_id = 17;
  ASSERT_TRUE(threadpool.create_queue(vm, replica_id, false));
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit(vm, replica_id, [&counter] { task_hangs(counter); });
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit(vm, replica_id, [&counter] { task_completes(counter); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter, &num_tasks]() { return counter == num_tasks; });
  ASSERT_FALSE(result);
  ASSERT_EQ(counter, 0);
}

// Tests to make sure a blocked task in one queue doesn't block
// other queues
TEST(ThreadPoolTests, TestMultipleQueuesOneQueueHangs) {
  int num_tasks = 500;
  ModelQueueThreadPool threadpool;
  VersionedModelId vm_one = VersionedModelId("m", "1");
  int replica_id_one = 17;
  VersionedModelId vm_two = VersionedModelId("j", "3");
  int replica_id_two = 3;
  ASSERT_TRUE(threadpool.create_queue(vm_one, replica_id_one, false));
  ASSERT_TRUE(threadpool.create_queue(vm_two, replica_id_two, false));

  std::atomic<int> counter_one(0);
  std::atomic<int> counter_two(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit(vm_two, replica_id_two,
                    [&counter_two] { task_hangs(counter_two); });
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit(vm_two, replica_id_two,
                      [&counter_two] { task_completes(counter_two); });
  }
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit(vm_one, replica_id_one,
                      [&counter_one] { task_completes(counter_one); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter_one, &counter_two, &num_tasks]() {
        return counter_one == num_tasks && counter_two == num_tasks;
      });
  ASSERT_FALSE(result);
  ASSERT_EQ(counter_two, 0);
  ASSERT_EQ(counter_one, num_tasks);
}

TEST(ThreadPoolTests, TestCreateDuplicateQueue) {
  ModelQueueThreadPool threadpool;
  VersionedModelId vm = VersionedModelId("m", "1");
  int replica_id = 17;
  ASSERT_TRUE(threadpool.create_queue(vm, replica_id, false));
  ASSERT_FALSE(threadpool.create_queue(vm, replica_id, false));
}

TEST(ThreadPoolTests, TestSubmitToNonexistentQueue) {
  ModelQueueThreadPool threadpool;
  VersionedModelId vm_one = VersionedModelId("m", "1");
  int replica_id_one = 17;
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  ASSERT_THROW(threadpool.submit(vm_one, replica_id_one,
                                 [&counter] { task_completes(counter); }),
               std::runtime_error);
  ASSERT_TRUE(threadpool.create_queue(vm_one, replica_id_one, false));
  threadpool.submit(vm_one, replica_id_one,
                    [&counter] { task_completes(counter); });
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result =
      notification_counter.wait_for(l, std::chrono::milliseconds(2000),
                                    [&counter]() { return counter == 1; });
  ASSERT_TRUE(result);
}

TEST(ThreadPoolTests, TestQueueIdHash) {
  // Same model name and version, different replica
  ASSERT_NE(ModelQueueThreadPool::get_queue_id(VersionedModelId("m", "1"), 1),
            ModelQueueThreadPool::get_queue_id(VersionedModelId("m", "1"), 2));

  // Same model name, different version, same replica
  ASSERT_NE(ModelQueueThreadPool::get_queue_id(VersionedModelId("m", "1"), 1),
            ModelQueueThreadPool::get_queue_id(VersionedModelId("m", "2"), 1));

  // Different model name, same version, same replica
  ASSERT_NE(ModelQueueThreadPool::get_queue_id(VersionedModelId("m", "1"), 1),
            ModelQueueThreadPool::get_queue_id(VersionedModelId("j", "1"), 1));
}
}  // namespace
