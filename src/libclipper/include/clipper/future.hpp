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
    std::vector<boost::shared_future<T>> futures,
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
          if (*num_completed + 1 == num_futures) {
            completion_promise->set_value();
            //
          } else {
            *num_completed += 1;
          }
          return result.get();
        }));
  }

  return std::make_pair<boost::future<void>, std::vector<boost::future<T>>>(
      completion_promise->get_future(), std::move(wrapped_futures));
}

template <typename R>
boost::future<R> wrap_when_any(
    boost::future<R> f, std::shared_ptr<std::atomic<int>> completed,
    std::shared_ptr<boost::promise<void>> completion_promise) {
  return f.then([completion_promise, completed](auto result) mutable {

    // Only set completed to true if the original value was false. This ensures
    // that we only set the value once, and therefore only complete the promise
    // once.
    int num_finished = completed->fetch_add(1);
    if (num_finished == 0) {
      completion_promise->set_value();
    }
    return result.get();
  });
}

/**
 * A function for waiting until either of a pair of futures has completed.
 *
 *
 *
 * \param num_completed A counter indicating the number of futures that
 * have completed so far. The reason this is an argument to the function
 * instead of internal state is that the caller of `when_all` must make sure
 * that the counter stays in scope for the lifetime of all the futures.
 *
 * \return A tuple of three futures. The first is the composed future that will
 * complete when either of the two futures provided as argument completes. The
 * second two elements in the tuple are futures that will complete at the same
 * time and with the same value as the futures passed in as arguments.
 */
template <typename R0, typename R1>
std::tuple<boost::future<void>, boost::future<R0>, boost::future<R1>> when_any(
    boost::future<R0> f0, boost::future<R1> f1,
    std::shared_ptr<std::atomic<int>> completed) {
  auto completion_promise = std::make_shared<boost::promise<void>>();
  auto wrapped_f0 = wrap_when_any(std::move(f0), completed, completion_promise);
  auto wrapped_f1 = wrap_when_any(std::move(f1), completed, completion_promise);

  return std::make_tuple<boost::future<void>, boost::future<R0>,
                         boost::future<R1>>(completion_promise->get_future(),
                                            std::move(wrapped_f0),
                                            std::move(wrapped_f1));
}

}  // namespace future
}  // namespace clipper

#endif  // define CLIPPER_LIB_FUTURE_HPP
