
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <thread>
#include <unordered_map>

#include <boost/thread.hpp>
#include <clipper/future.hpp>

using namespace clipper;
using namespace std::chrono_literals;

namespace {

TEST(WhenAllTests, DontCompleteEarly) {
  boost::promise<void> p1;
  boost::promise<void> p2;
  boost::promise<void> p3;

  auto num_completed = std::make_shared<std::atomic<int>>(0);
  std::vector<boost::shared_future<void>> v;
  v.push_back(p1.get_future());
  v.push_back(p2.get_future());
  v.push_back(p3.get_future());

  boost::future<void> completion_future;
  std::vector<boost::future<void>> v_copy;
  std::tie(completion_future, v_copy) =
      future::when_all(std::move(v), num_completed);

  ASSERT_FALSE(completion_future.is_ready());
  p1.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(completion_future.is_ready());
  p2.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(completion_future.is_ready());
  // p3.set_value();
  // std::this_thread::sleep_for(500us);
  // ASSERT_TRUE(completion_future.is_ready());
}

TEST(WhenAllTests, CompleteCorrectly) {
  boost::promise<void> p1;
  boost::promise<void> p2;
  boost::promise<void> p3;

  auto num_completed = std::make_shared<std::atomic<int>>(0);
  std::vector<boost::shared_future<void>> v;
  v.push_back(p1.get_future());
  v.push_back(p2.get_future());
  v.push_back(p3.get_future());

  boost::future<void> completion_future;
  std::vector<boost::future<void>> v_copy;
  std::tie(completion_future, v_copy) =
      future::when_all(std::move(v), num_completed);

  ASSERT_FALSE(completion_future.is_ready());
  p1.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(completion_future.is_ready());
  p2.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_FALSE(completion_future.is_ready());
  p3.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(completion_future.is_ready());
}

TEST(WhenAnyTests, CompleteFirstEntry) {
  boost::promise<void> p1;
  boost::promise<void> p2;
  boost::future<void> f1 = p1.get_future();
  boost::future<void> f2 = p2.get_future();

  auto num_completed = std::make_shared<std::atomic<int>>(0);
  boost::future<void> completion_future;
  std::tie(completion_future, f1, f2) =
      future::when_any(std::move(f1), std::move(f2), num_completed);

  ASSERT_FALSE(completion_future.is_ready());
  p1.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(completion_future.is_ready());
}

TEST(WhenAnyTests, CompleteSecondEntry) {
  boost::promise<void> p1;
  boost::promise<void> p2;
  boost::future<void> f1 = p1.get_future();
  boost::future<void> f2 = p2.get_future();

  auto num_completed = std::make_shared<std::atomic<int>>(0);
  boost::future<void> completion_future;
  std::tie(completion_future, f1, f2) =
      future::when_any(std::move(f1), std::move(f2), num_completed);

  ASSERT_FALSE(completion_future.is_ready());
  p2.set_value();
  std::this_thread::sleep_for(500us);
  ASSERT_TRUE(completion_future.is_ready());
}
}
