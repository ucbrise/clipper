#ifndef CLIPPER_LIB_FUTURE_HPP
#define CLIPPER_LIB_FUTURE_HPP

#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <vector>

#include "boost/thread.hpp"

namespace clipper {

namespace future {

/*
 * The first element in the pair is a future that will complete when
 * all the futures provided have completed. The second element is a vector
 * of futures that have the same values and will complete at the same time
 * as the futures passed in as argument.
 *
 * \param futures A list of futures to compose
 * \param num_completed A counter indicating the number of futures that
 * have completed so far. The reason this is an argument to the function
 * instead of internal state is that the caller of `when_all` must make sure
 * that the counter stays in scope for the lifetime of all the futures.
*/
template <class T>
std::pair<boost::future<void>, std::vector<boost::future<T>>> when_all(
    std::vector<boost::future<T>> futures,
    std::shared_ptr<std::atomic<int>> num_completed) {
  if (futures.size() == 0) {
    return std::make_pair(boost::make_ready_future(),
                          std::vector<boost::future<T>>{});
  }
  int num_futures = futures.size();
  auto completion_promise = std::make_shared<boost::promise<void>>();
  std::vector<boost::future<T>> wrapped_futures;
  for (auto f = futures.begin(); f != futures.end(); ++f) {
    wrapped_futures.push_back(f->then(
        [num_futures, completion_promise, num_completed](auto result) mutable {
          if (num_completed->fetch_add(1) + 1 == num_futures) {
            completion_promise->set_value();
            assert(*num_completed == num_futures);
          }
          // if (*num_completed + 1 == num_futures) {
          //   completion_promise->set_value();
          //   //
          // } else {
          //   *num_completed += 1;
          // }
          return result.get();
        }));
  }

  return std::make_pair<boost::future<void>, std::vector<boost::future<T>>>(
      completion_promise->get_future(), std::move(wrapped_futures));
}

template <typename R>
boost::future<R> wrap_when_both(
    boost::future<R> future, std::shared_ptr<std::atomic<int>> num_completed,
    std::shared_ptr<boost::promise<void>> completion_promise) {
  return future.then([completion_promise, num_completed](auto result) mutable {

    int num_already_completed = num_completed->fetch_add(1);
    // means the other future has already finished
    if (num_already_completed == 1) {
      completion_promise->set_value();
    }
    return result.get();
  });
}

/**
 * A function for waiting until both of a pair of futures has completed.
 *
 *
 * \param num_completed A counter indicating the number of futures that
 * have completed so far. The reason this is an argument to the function
 * instead of internal state is that the caller of `when_both` must make sure
 * that the counter stays in scope for the lifetime of all the futures.
 *
 * \return A tuple of three futures. The first is the composed future that will
 * complete when both of the two futures provided as argument completes. The
 * second two elements in the tuple are futures that will complete at the same
 * time and with the same value as the futures passed in as arguments.
 */
template <typename R0, typename R1>
std::tuple<boost::future<void>, boost::future<R0>, boost::future<R1>> when_both(
    boost::future<R0> f0, boost::future<R1> f1,
    std::shared_ptr<std::atomic<int>> num_completed) {
  auto completion_promise = std::make_shared<boost::promise<void>>();
  auto wrapped_f0 =
      wrap_when_both(std::move(f0), num_completed, completion_promise);
  auto wrapped_f1 =
      wrap_when_both(std::move(f1), num_completed, completion_promise);

  return std::make_tuple<boost::future<void>, boost::future<R0>,
                         boost::future<R1>>(completion_promise->get_future(),
                                            std::move(wrapped_f0),
                                            std::move(wrapped_f1));
}

template <typename R>
boost::future<R> wrap_when_either(
    boost::future<R> future, std::shared_ptr<std::atomic_flag> completed_flag,
    std::shared_ptr<boost::promise<void>> completion_promise) {
  return future.then([completion_promise, completed_flag](auto result) mutable {

    // Only set completed to true if the original value was false. This ensures
    // that we only set the value once, and therefore only complete the promise
    // once.
    bool flag_already_set = completed_flag->test_and_set();
    if (!flag_already_set) {
      completion_promise->set_value();
    }
    return result.get();
  });
}

/**
 * A function for waiting until either of a pair of futures has completed.
 *
 * \param completed_flag A flag indicating whether any futures have completed
 * so far. The reason this is an argument to the function
 * instead of internal state is that the caller of `when_either` must make sure
 * that the counter stays in scope for the lifetime of all the futures.
 *
 * \return A tuple of three futures. The first is the composed future that will
 * complete when either of the two futures provided as argument completes. The
 * second two elements in the tuple are futures that will complete at the same
 * time and with the same value as the futures passed in as arguments.
 */
template <typename R0, typename R1>
std::tuple<boost::future<void>, boost::future<R0>, boost::future<R1>>
when_either(boost::future<R0> f0, boost::future<R1> f1,
            std::shared_ptr<std::atomic_flag> completed_flag) {
  auto completion_promise = std::make_shared<boost::promise<void>>();
  auto wrapped_f0 =
      wrap_when_either(std::move(f0), completed_flag, completion_promise);
  auto wrapped_f1 =
      wrap_when_either(std::move(f1), completed_flag, completion_promise);

  return std::make_tuple<boost::future<void>, boost::future<R0>,
                         boost::future<R1>>(completion_promise->get_future(),
                                            std::move(wrapped_f0),
                                            std::move(wrapped_f1));
}

}  // namespace future
}  // namespace clipper

#endif  // define CLIPPER_LIB_FUTURE_HPP
