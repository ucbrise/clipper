#ifndef CLIPPER_LIB_THREADPOOL_HPP
#define CLIPPER_LIB_THREADPOOL_HPP

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "logging.hpp"

namespace clipper {

const std::string LOGGING_TAG_THREADPOOL = "THREADPOOL";

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
  bool try_pop(T& out) {
    std::lock_guard<std::mutex> lock{mutex_};
    if (queue_.empty() || !valid_) {
      return false;
    }
    out = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  /**
   * Get the first value in the queue.
   * Will block until a value is available unless clear is called or the
   * instance is destructed.
   * Returns true if a value was successfully written to the out parameter,
   * false otherwise.
   */
  bool wait_pop(T& out) {
    std::unique_lock<std::mutex> lock{mutex_};
    condition_.wait(lock, [this]() { return !queue_.empty() || !valid_; });
    /*
     * Using the condition in the predicate ensures that spurious wakeups with a
     * valid
     * but empty queue will not proceed, so only need to check for validity
     * before proceeding.
     */
    if (!valid_) {
      return false;
    }
    out = std::move(queue_.front());
    queue_.pop();
    return true;
  }

  /**
   * Push a new value onto the queue.
   */
  void push(T value) {
    std::lock_guard<std::mutex> lock{mutex_};
    queue_.push(std::move(value));
    condition_.notify_one();
  }

  /**
   * Check whether or not the queue is empty.
   */
  bool empty(void) const {
    std::lock_guard<std::mutex> lock{mutex_};
    return queue_.empty();
  }

  /**
   * Clear all items from the queue.
   */
  void clear(void) {
    std::lock_guard<std::mutex> lock{mutex_};
    while (!queue_.empty()) {
      queue_.pop();
    }
    condition_.notify_all();
  }

  /**
   * Invalidate the queue.
   * Used to ensure no conditions are being waited on in wait_pop when
   * a thread or the application is trying to exit.
   * The queue is invalid after calling this method and it is an error
   * to continue using a queue after this method has been called.
   */
  void invalidate(void) {
    std::lock_guard<std::mutex> lock{mutex_};
    valid_ = false;
    condition_.notify_all();
  }

  /**
   * Returns whether or not this queue is valid.
   */
  bool is_valid(void) const {
    std::lock_guard<std::mutex> lock{mutex_};
    return valid_;
  }

 private:
  std::atomic_bool valid_{true};
  mutable std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable condition_;
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
    ThreadTask(Func&& func) : func_{std::move(func)} {}

    ~ThreadTask(void) override = default;
    ThreadTask(const ThreadTask& rhs) = delete;
    ThreadTask& operator=(const ThreadTask& rhs) = delete;
    ThreadTask(ThreadTask&& other) = default;
    ThreadTask& operator=(ThreadTask&& other) = default;

    /**
     * Run the task.
     */
    void execute() override { func_(); }

   private:
    Func func_;
  };

 public:
  ThreadPool(void) : done_{false}, queues_{}, threads_{} {}

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

  bool create_queue(VersionedModelId vm, int replica_id) {
    boost::unique_lock<boost::shared_mutex> l(queues_mutex_);
    size_t queue_id = get_queue_id(vm, replica_id);
    auto queue = queues_.find(queue_id);
    if (queue != queues_.end()) {
      log_error_formatted(LOGGING_TAG_THREADPOOL,
                          "Work queue already exists for model {}, replica {}",
                          versioned_model_to_str(vm),
                          std::to_string(replica_id));
      return false;
    } else {
      queues_.emplace(std::piecewise_construct, std::forward_as_tuple(queue_id),
                      std::forward_as_tuple());
      threads_.emplace(
          std::piecewise_construct, std::forward_as_tuple(queue_id),
          std::forward_as_tuple(&ThreadPool::worker, this, queue_id));
      log_info_formatted(
          LOGGING_TAG_THREADPOOL, "Work queue created for model {}, replica {}",
          versioned_model_to_str(vm), std::to_string(replica_id));
      return true;
    }
  }

  /**
   * Submit a job to be run by the thread pool.
   */
  template <typename Func, typename... Args>
  auto submit(VersionedModelId vm, int replica_id, Func&& func,
              Args&&... args) {
    auto boundTask =
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    using ResultType = std::result_of_t<decltype(boundTask)()>;
    using PackagedTask = boost::packaged_task<ResultType()>;
    using TaskType = ThreadTask<PackagedTask>;
    PackagedTask task{std::move(boundTask)};
    auto result_future = task.get_future();

    size_t queue_id = get_queue_id(vm, replica_id);
    boost::shared_lock<boost::shared_mutex> l(queues_mutex_);
    auto queue = queues_.find(queue_id);
    if (queue != queues_.end()) {
      queue->second.push(std::make_unique<TaskType>(std::move(task)));
    } else {
      std::stringstream error_msg;
      error_msg << "No work queue for model " << versioned_model_to_str(vm)
                << ", replica " << std::to_string(replica_id);
      log_error(LOGGING_TAG_THREADPOOL, error_msg.str());
      throw std::runtime_error(error_msg.str());
    }
    return result_future;
  }

  static size_t get_queue_id(const VersionedModelId& vm, const int replica_id) {
    std::size_t seed = 0;
    boost::hash_combine(seed, vm.first);
    boost::hash_combine(seed, vm.second);
    boost::hash_combine(seed, replica_id);
    return seed;
  }

 private:
  /**
   * Constantly running function each thread uses to acquire work items from the
   * queue.
   */
  void worker(size_t worker_id) {
    while (!done_) {
      std::unique_ptr<IThreadTask> pTask{nullptr};
      bool work_to_do = false;
      {
        boost::shared_lock<boost::shared_mutex> l(queues_mutex_);
        // NOTE: The use of try_pop here means the worker will spin instead of
        // block while waiting for work. This is intentional. We defer to the
        // submitted tasks to block when no work is available.
        work_to_do = queues_[worker_id].try_pop(pTask);
      }
      if (work_to_do) {
        pTask->execute();
      }
    }
    auto thread_id = std::this_thread::get_id();
    std::stringstream ss;
    ss << thread_id;
    log_info_formatted(LOGGING_TAG_THREADPOOL,
                       "Worker {}, thread {} is shutting down",
                       std::to_string(worker_id), ss.str());
  }

  /**
   * Invalidates the queue and joins all running threads.
   */
  void destroy(void) {
    log_info(LOGGING_TAG_THREADPOOL, "Destroying threadpool");
    done_ = true;
    for (auto& queue : queues_) {
      queue.second.invalidate();
    }
    for (auto& thread : threads_) {
      if (thread.second.joinable()) {
        thread.second.join();
      }
    }
  }

 private:
  std::atomic_bool done_;
  boost::shared_mutex queues_mutex_;
  std::unordered_map<size_t, ThreadSafeQueue<std::unique_ptr<IThreadTask>>>
      queues_;
  std::unordered_map<size_t, std::thread> threads_;
};

namespace TaskExecutionThreadPool {

/**
 * Convenience method to get the task execution thread pool for the application.
 */
inline ThreadPool& get_thread_pool(void) {
  static ThreadPool taskExecutionPool;
  return taskExecutionPool;
}

/**
 * Submit a job to the task execution thread pool.
 */
template <typename Func, typename... Args>
inline auto submit_job(VersionedModelId vm, int replica_id, Func&& func,
                       Args&&... args) {
  return get_thread_pool().submit(vm, replica_id, std::forward<Func>(func),
                                  std::forward<Args>(args)...);
}

inline void create_queue(VersionedModelId vm, int replica_id) {
  get_thread_pool().create_queue(vm, replica_id);
}

}  // namespace DefaultThreadPool
}  // namespace clipper

#endif  // CLIPPER_LIB_THREADPOOL_HPP
