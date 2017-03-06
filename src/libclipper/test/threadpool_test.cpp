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

// Tests whether a single task successfully finishes
TEST(ThreadPoolTests, TestSuccessfulJobCompletion) {
  ThreadPool threadpool(1);
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit([&counter] { task_completes(counter); });
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result =
      notification_counter.wait_for(l, std::chrono::milliseconds(2000),
                                    [&counter]() { return counter == 1; });
  ASSERT_TRUE(result);
}

// Tests whether many tasks submitted to a threadpool with 4 threads
// all complete successfully.
TEST(ThreadPoolTests, TestManyJobsComplete) {
  int num_tasks = 500;
  ThreadPool threadpool(4);
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit([&counter] { task_completes(counter); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter, &num_tasks]() { return counter == num_tasks; });
  ASSERT_TRUE(result);
}

// Tests that a single long-running task blocks all subsequent tasks
// in a threadpool with 1 thread.
TEST(ThreadPoolTests, TestHangingTaskOneThread) {
  std::cout << "Test main thread ID: " << std::this_thread::get_id()
            << std::endl;
  int num_tasks = 500;
  ThreadPool threadpool(1);
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit([&counter] { task_hangs(counter); });
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit([&counter] { task_completes(counter); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter, &num_tasks]() { return counter == num_tasks; });
  ASSERT_FALSE(result);
  ASSERT_EQ(counter, 0);
}

// Tests that a single long-running task does not block subsequent tasks
// in a threadpool with more than one thread.
TEST(ThreadPoolTests, TestHangingTaskManyThreads) {
  int num_tasks = 500;
  ThreadPool threadpool(4);
  std::atomic<int> counter(0);
  std::condition_variable_any notification_counter;
  std::mutex notification_mutex;
  threadpool.submit([&counter] { task_hangs(counter); });
  for (int i = 0; i < num_tasks; ++i) {
    threadpool.submit([&counter] { task_completes(counter); });
  }
  std::unique_lock<std::mutex> l(notification_mutex);
  bool result = notification_counter.wait_for(
      l, std::chrono::milliseconds(2000),
      [&counter, &num_tasks]() { return counter == num_tasks; });
  ASSERT_TRUE(result);
}
}
