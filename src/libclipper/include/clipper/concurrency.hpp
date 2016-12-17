#ifndef CLIPPER_LIB_CONCURRENCY_HPP
#define CLIPPER_LIB_CONCURRENCY_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// uncomment to disable assert()
// #define NDEBUG
#include <cassert>

#include "boost/optional.hpp"
#include "boost/thread.hpp"

#define UNUSED(expr) \
  do {               \
    (void)(expr);    \
  } while (0)

namespace clipper {

// Queue implementation borrowed from LatticeFlow
// https://github.com/ucbrise/LatticeFlow/blob/3d9e2fa9d84d8a5f578c0039f9ee6f3307cf8b1b/src/concurrency/queue.h
template <typename T>
class Queue {
 public:
  Queue() = default;
  explicit Queue(std::vector<T> xs) : xs_(std::move(xs)) {}
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;

  // TODO should we allow move constructors?
  Queue(Queue&&) = delete;
  Queue& operator=(Queue&&) = delete;

  void push(const T& x) {
    boost::unique_lock<boost::shared_mutex> l(m_);
    xs_.push(x);
    data_available_.notify_one();
  }

  int size() {
    boost::shared_lock<boost::shared_mutex> l(m_);
    return xs_.size();
  }

  /// Block until the queue contains at least one element, then return the
  /// first element in the queue.
  T pop() {
    boost::unique_lock<boost::shared_mutex> l(m_);
    while (xs_.size() == 0) {
      data_available_.wait(l);
    }
    const T x = xs_.front();
    xs_.pop();
    return x;
  }

  boost::optional<T> try_pop() {
    boost::unique_lock<boost::shared_mutex> l(m_);
    if (xs_.size() > 0) {
      const T x = xs_.front();
      xs_.pop();
      return x;
    } else {
      return {};
    }
  }

  std::vector<T> try_pop_batch(size_t batch_size) {
    boost::unique_lock<boost::shared_mutex> l(m_);
    std::vector<T> batch;
    while (xs_.size() > 0 && batch.size() < batch_size) {
      batch.push_back(xs_.front());
      xs_.pop();
    }
    return batch;
  }

  void clear() {
    boost::unique_lock<boost::shared_mutex> l(m_);
    xs_.clear();
  }

 private:
  boost::shared_mutex m_;
  std::condition_variable_any data_available_;
  std::queue<T> xs_;
};

/// Implementation adapted from
/// https://goo.gl/Iav87R

template <typename T>
class ThreadSafeQueue {
 public:
  /**
   * Destructor.
   */
  ~ThreadSafeQueue(void) { invalidate(); }

  /**
   * Attempt to get the first value in the queue.
   * Returns true if a value was successfully written to the out parameter,
   * false otherwise.
   */
  bool tryPop(T& out) {
    std::lock_guard<std::mutex> lock{m_mutex};
    if (m_queue.empty() || !m_valid) {
      return false;
    }
    out = std::move(m_queue.front());
    m_queue.pop();
    return true;
  }

  /**
   * Get the first value in the queue.
   * Will block until a value is available unless clear is called or the
   * instance is destructed.
   * Returns true if a value was successfully written to the out parameter,
   * false otherwise.
   */
  bool waitPop(T& out) {
    std::unique_lock<std::mutex> lock{m_mutex};
    m_condition.wait(lock, [this]() { return !m_queue.empty() || !m_valid; });
    /*
     * Using the condition in the predicate ensures that spurious wakeups with a
     * valid
     * but empty queue will not proceed, so only need to check for validity
     * before proceeding.
     */
    if (!m_valid) {
      return false;
    }
    out = std::move(m_queue.front());
    m_queue.pop();
    return true;
  }

  /**
   * Push a new value onto the queue.
   */
  void push(T value) {
    std::lock_guard<std::mutex> lock{m_mutex};
    m_queue.push(std::move(value));
    m_condition.notify_one();
  }

  /**
   * Check whether or not the queue is empty.
   */
  bool empty(void) const {
    std::lock_guard<std::mutex> lock{m_mutex};
    return m_queue.empty();
  }

  /**
   * Clear all items from the queue.
   */
  void clear(void) {
    std::lock_guard<std::mutex> lock{m_mutex};
    while (!m_queue.empty()) {
      m_queue.pop();
    }
    m_condition.notify_all();
  }

  /**
   * Invalidate the queue.
   * Used to ensure no conditions are being waited on in waitPop when
   * a thread or the application is trying to exit.
   * The queue is invalid after calling this method and it is an error
   * to continue using a queue after this method has been called.
   */
  void invalidate(void) {
    std::lock_guard<std::mutex> lock{m_mutex};
    m_valid = false;
    m_condition.notify_all();
  }

  /**
   * Returns whether or not this queue is valid.
   */
  bool isValid(void) const {
    std::lock_guard<std::mutex> lock{m_mutex};
    return m_valid;
  }

 private:
  std::atomic_bool m_valid{true};
  mutable std::mutex m_mutex;
  std::queue<T> m_queue;
  std::condition_variable m_condition;
};

class ThreadPool {
 private:
  class IThreadTask {
   public:
    IThreadTask(void) = default;
    virtual ~IThreadTask(void) = default;
    IThreadTask(const IThreadTask& rhs) = delete;
    IThreadTask& operator=(const IThreadTask& rhs) = delete;
    IThreadTask(IThreadTask&& other) = default;
    IThreadTask& operator=(IThreadTask&& other) = default;

    /**
     * Run the task.
     */
    virtual void execute() = 0;
  };

  template <typename Func>
  class ThreadTask : public IThreadTask {
   public:
    ThreadTask(Func&& func) : m_func{std::move(func)} {}

    ~ThreadTask(void) override = default;
    ThreadTask(const ThreadTask& rhs) = delete;
    ThreadTask& operator=(const ThreadTask& rhs) = delete;
    ThreadTask(ThreadTask&& other) = default;
    ThreadTask& operator=(ThreadTask&& other) = default;

    /**
     * Run the task.
     */
    void execute() override { m_func(); }

   private:
    Func m_func;
  };

 public:
  // #<{(|*
  //  * A wrapper around a std::future that adds the behavior of futures
  //  returned
  //  * from std::async.
  //  * Specifically, this object will block and wait for execution to finish
  //  * before going out of scope.
  //  |)}>#
  // template <typename T>
  // class TaskFuture {
  //  public:
  //   TaskFuture(boost::future<T>&& future) : m_future{std::move(future)} {}
  //
  //   TaskFuture(const TaskFuture& rhs) = delete;
  //   TaskFuture& operator=(const TaskFuture& rhs) = delete;
  //   TaskFuture(TaskFuture&& other) = default;
  //   TaskFuture& operator=(TaskFuture&& other) = default;
  //   ~TaskFuture(void) {
  //     if (m_future.valid()) {
  //       m_future.get();
  //     }
  //   }
  //
  //   auto get(void) { return m_future.get(); }
  //
  //  private:
  //   boost::future<T> m_future;
  // };
  //
 public:
  /**
   * Constructor.
   */
  ThreadPool(void)
      : ThreadPool{std::max(std::thread::hardware_concurrency(), 2u) - 1u} {
    /*
     * Always create at least one thread.  If hardware_concurrency() returns 0,
     * subtracting one would turn it to UINT_MAX, so get the maximum of
     * hardware_concurrency() and 2 before subtracting 1.
     */
  }

  /**
   * Constructor.
   */
  explicit ThreadPool(const std::uint32_t numThreads)
      : m_done{false}, m_workQueue{}, m_threads{} {
    try {
      for (std::uint32_t i = 0u; i < numThreads; ++i) {
        m_threads.emplace_back(&ThreadPool::worker, this);
      }
    } catch (...) {
      destroy();
      throw;
    }
  }

  /**
   * Non-copyable.
   */
  ThreadPool(const ThreadPool& rhs) = delete;

  /**
   * Non-assignable.
   */
  ThreadPool& operator=(const ThreadPool& rhs) = delete;

  /**
   * Destructor.
   */
  ~ThreadPool(void) { destroy(); }

  /**
   * Submit a job to be run by the thread pool.
   */
  template <typename Func, typename... Args>
  auto submit(Func&& func, Args&&... args) {
    auto boundTask =
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    using ResultType = std::result_of_t<decltype(boundTask)()>;
    using PackagedTask = boost::packaged_task<ResultType()>;
    using TaskType = ThreadTask<PackagedTask>;
    PackagedTask task{std::move(boundTask)};
    auto result_future = task.get_future();

    m_workQueue.push(std::make_unique<TaskType>(std::move(task)));
    return result_future;
  }

 private:
  /**
   * Constantly running function each thread uses to acquire work items from the
   * queue.
   */
  void worker(void) {
    while (!m_done) {
      std::unique_ptr<IThreadTask> pTask{nullptr};
      if (m_workQueue.waitPop(pTask)) {
        pTask->execute();
      }
    }
  }

  /**
   * Invalidates the queue and joins all running threads.
   */
  void destroy(void) {
    m_done = true;
    m_workQueue.invalidate();
    for (auto& thread : m_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

 private:
  std::atomic_bool m_done;
  ThreadSafeQueue<std::unique_ptr<IThreadTask>> m_workQueue;
  std::vector<std::thread> m_threads;
};

namespace DefaultThreadPool {

/**
 * Get the default thread pool for the application.
 * This pool is created with 4 threads.
 */
inline ThreadPool& get_thread_pool(void) {
  static ThreadPool defaultPool(4);
  return defaultPool;
}

/**
 * Submit a job to the default thread pool.
 */
template <typename Func, typename... Args>
inline auto submit_job(Func&& func, Args&&... args) {
  return get_thread_pool().submit(std::forward<Func>(func),
                                  std::forward<Args>(args)...);
}
}  // namespace DefaultThreadPool

namespace future_composition {

/// The first element in the pair is a future that will complete when
/// all the futures provided have completed. The value of the future
/// will always be true (there were some issues with a future<void>).
/// The second element is a vector
/// of futures that have the same values and will complete at the same time
/// as the futures passed in as argument.
template <class T>
std::pair<boost::future<void>, std::vector<boost::future<T>>> when_all(
    std::vector<boost::future<T>> futures) {
  if (futures.size() == 0) {
    return std::make_pair(boost::make_ready_future(), std::move(futures));
  }
  std::atomic<int> num_completed(0);
  // std::shared_ptr<boost::promise<std::vector<T>>> completion_promise =
  // std::make_shared;
  int num_futures = futures.size();
  auto completion_promise = std::make_shared<boost::promise<void>>();
  std::vector<boost::future<T>> wrapped_futures;
  for (auto f = futures.begin(); f != futures.end(); ++f) {
    wrapped_futures.push_back(f->then(
        [num_futures, completion_promise, &num_completed](auto result) mutable {
          if (num_completed + 1 == num_futures) {
            completion_promise->set_value();
            //
          } else {
            num_completed += 1;
          }
          return result.get();
        }));
  }

  return std::make_pair<boost::future<void>, std::vector<boost::future<T>>>(
      completion_promise->get_future(), std::move(wrapped_futures));

  // auto completion_future = completion_promise->get_future();
  // // boost::promise<std::vector<T>> when_all_promise;
  // // auto when_all_future = when_all_promise.get_future();
  // completion_future.then([
  //   wf = std::move(wrapped_futures), p = std::move(when_all_promise)
  // ](auto rrr) mutable {
  //   if (!rrr.get()) {
  //     std::cout << "Uh oh" << std::endl;
  //   }
  //   std::vector<T> collected_results;
  //   for (auto f = wf.begin(); f != wf.end(); ++f) {
  //     collected_results.push_back(f->get());
  //   }
  //   p.set_value(std::move(collected_results));
  // });
  // return when_all_future;
}

}  // namespace future_composition
}  // namespace clipper

#endif
