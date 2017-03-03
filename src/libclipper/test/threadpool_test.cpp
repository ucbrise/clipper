#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <thread>

#include <clipper/threadpool.hpp>

using namespace clipper;
using namespace std::chrono_literals;

namespace {

// TEST(ThreadSafeQueueTests, TestTryPop) {
//   ThreadSafeQueue<int> q;
//   int out;
//   ASSERT_FALSE(q.try_pop(out));
//
//   q.push(3);
//   q.push(5);
//
//   ASSERT_TRUE(q.try_pop(out));
//   ASSERT_EQ(out, 5);
// }
}
