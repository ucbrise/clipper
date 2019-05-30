#ifndef CLIPPER_LIB_THREADPOOL_HPP
#define CLIPPER_LIB_THREADPOOL_HPP

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

#include <boost/thread.hpp>

#include "datatypes.hpp"
#include "exceptions.hpp"
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
  bool wait_pop(T& out, boost::shared_lock<boost::shared_mutex>& queues_l) {
    std::unique_lock<std::mutex> lock{mutex_};
    queues_l.unlock();
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
 protected:
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

 protected:
  /**
   * Submit a job to be run by the thread pool.
   */
  template <typename Func, typename... Args>
  auto submit(size_t queue_id, Func&& func, Args&&... args) {
    auto boundTask =
        std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    using ResultType = std::result_of_t<decltype(boundTask)()>;
    using PackagedTask = boost::packaged_task<ResultType()>;
    using TaskType = ThreadTask<PackagedTask>;
    PackagedTask task{std::move(boundTask)};
    auto result_future = task.get_future();

    boost::shared_lock<boost::shared_mutex> l(queues_mutex_);
    auto queue = queues_.find(queue_id);
    if (queue != queues_.end()) {
      queue->second.push(std::make_unique<TaskType>(std::move(task)));
    } else {
      std::stringstream error_msg;
      error_msg << "No work queue found with id " << queue_id;
      log_error(LOGGING_TAG_THREADPOOL, error_msg.str());
      throw ThreadPoolError(error_msg.str());
    }
    return result_future;
  }

  /**
   * Constantly running function each thread uses to acquire work items from the
   * queue.
   */
  void worker(size_t worker_id, bool is_block_worker) {
    auto thread_id = std::this_thread::get_id();
    std::stringstream ss;
    ss << thread_id;

    try {
        while (!done_ && queues_[worker_id].is_valid()) {
          boost::this_thread::interruption_point();
          std::unique_ptr<IThreadTask> pTask{nullptr};
          bool work_to_do = false;
          {
            boost::shared_lock<boost::shared_mutex> l(queues_mutex_);
            if (is_block_worker) {
              work_to_do = queues_[worker_id].wait_pop(pTask, l);
            } else {
              // NOTE: The use of try_pop here means the worker will spin instead of
              // block while waiting for work. This is intentional. We defer to the
              // submitted tasks to block when no work is available.
              work_to_do = queues_[worker_id].try_pop(pTask);
            }
          }
          if (work_to_do) {
            pTask->execute();
          }
        }
    } catch (boost::thread_interrupted&) {
      log_info_formatted(LOGGING_TAG_THREADPOOL,
                         "Worker {}, thread {} is interrupted",
                         std::to_string(worker_id), ss.str());
    }
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

  std::atomic_bool done_;
  boost::shared_mutex queues_mutex_;
  std::unordered_map<size_t, ThreadSafeQueue<std::unique_ptr<IThreadTask>>>
      queues_;
  boost::shared_mutex threads_mutex_;
  std::unordered_map<size_t, boost::thread> threads_;
};

class ModelQueueThreadPool : public ThreadPool {
 public:
  /**
   * Submit a job to be run by the thread pool.
   */
  template <typename Func, typename... Args>
  auto submit(VersionedModelId vm, int replica_id, Func&& func,
              Args&&... args) {
    size_t queue_id = get_queue_id(vm, replica_id);
    try {
      ThreadPool::submit(queue_id, func, args...);
    } catch (ThreadPoolError& e) {
      log_error_formatted(LOGGING_TAG_THREADPOOL,
                          "Failed to submit task for model {}, replica {}",
                          vm.serialize(), replica_id);
      throw(e);
    }
  }

  bool create_queue(VersionedModelId vm, int replica_id, bool is_block_worker) {
    boost::unique_lock<boost::shared_mutex> lock_q(queues_mutex_);
    boost::unique_lock<boost::shared_mutex> lock_t(threads_mutex_);
    size_t queue_id = get_queue_id(vm, replica_id);
    auto queue = queues_.find(queue_id);
    if (queue != queues_.end()) {
      log_error_formatted(LOGGING_TAG_THREADPOOL,
                          "Work queue already exists for model {}, replica {}",
                          vm.serialize(), std::to_string(replica_id));
      return false;
    } else {
      queues_.emplace(std::piecewise_construct, std::forward_as_tuple(queue_id),
                      std::forward_as_tuple());
      threads_.emplace(std::piecewise_construct,
                       std::forward_as_tuple(queue_id),
                       std::forward_as_tuple(&ModelQueueThreadPool::worker,
                                             this, queue_id, is_block_worker));
      log_info_formatted(LOGGING_TAG_THREADPOOL,
                         "Work queue created for model {}, replica {}",
                         vm.serialize(), std::to_string(replica_id));
      return true;
    }
  }

  bool interrupt_thread(VersionedModelId vm, const int replica_id) {
    boost::shared_lock<boost::shared_mutex> l(threads_mutex_);
    size_t queue_id = get_queue_id(vm, replica_id);
    auto thread = threads_.find(queue_id);
    if (thread == threads_.end()) {
      log_error_formatted(LOGGING_TAG_THREADPOOL,
                          "Thread does not exist for model {}, replica {}",
                          vm.serialize(), replica_id);
      return false;
    } else {
      thread->second.interrupt();
      log_info_formatted(LOGGING_TAG_THREADPOOL,
                         "Thread is interrupted for model {}, replica {}",
                         vm.serialize(), replica_id);
      return true;
    }
  }

  bool delete_queue(VersionedModelId vm, const int replica_id) {
    boost::unique_lock<boost::shared_mutex> lock_q(queues_mutex_);
    boost::unique_lock<boost::shared_mutex> lock_t(threads_mutex_);
    size_t queue_id = get_queue_id(vm, replica_id);
    auto queue = queues_.find(queue_id);
    if (queue == queues_.end() || !queue->second.is_valid()) {
      log_error_formatted(LOGGING_TAG_THREADPOOL,
                          "Work queue does not exist for model {}, replica {}",
                          vm.serialize(), replica_id);
      return false;
    }

    queue->second.invalidate();

    auto thread = threads_.find(queue_id);
    if (thread->second.joinable()) {
      thread->second.join();
    }

    queues_.erase(queue);
    threads_.erase(thread);

    log_info_formatted(LOGGING_TAG_THREADPOOL,
                       "Work queue destroyed for model {}, replica {}",
                       vm.serialize(), replica_id);
    return true;
  }

  static size_t get_queue_id(const VersionedModelId& vm, const int replica_id) {
    std::size_t seed = 0;
    boost::hash_combine(seed, vm.get_name());
    boost::hash_combine(seed, vm.get_id());
    boost::hash_combine(seed, replica_id);
    return seed;
  }
};

class FixedSizeThreadPool : public ThreadPool {
 public:
  FixedSizeThreadPool(const size_t num_threads, bool is_block_worker)
      : ThreadPool(), queue_id_(1) {
    boost::unique_lock<boost::shared_mutex> l(queues_mutex_);
    if (num_threads <= 0) {
      throw std::runtime_error(
          "Attempted to construct threadpool with no threads");
    }
    queues_.emplace(std::piecewise_construct, std::forward_as_tuple(queue_id_),
                    std::forward_as_tuple());
    for (size_t i = 0; i < num_threads; ++i) {
      threads_.emplace(std::piecewise_construct,
                       std::forward_as_tuple(queue_id_),
                       std::forward_as_tuple(&FixedSizeThreadPool::worker, this,
                                             queue_id_, is_block_worker));
    }
  }

  /**
   * Submit a job to be run by the thread pool
   */
  template <typename Func, typename... Args>
  auto submit(Func&& func, Args&&... args) {
    try {
      ThreadPool::submit(queue_id_, func, args...);
    } catch (ThreadPoolError& e) {
      log_error(LOGGING_TAG_THREADPOOL,
                "Failed to submit task for FixedSizeThreadPool");
      throw(e);
    }
  }

 private:
  size_t queue_id_ = 1;
};

namespace TaskExecutionThreadPool {

/**
 * Convenience method to get the task execution thread pool for the application
 */
inline ModelQueueThreadPool& get_thread_pool(void) {
  static ModelQueueThreadPool task_execution_pool;
  return task_execution_pool;
}

/**
 * Submit a job to the task execution thread pool
 */
template <typename Func, typename... Args>
inline auto submit_job(VersionedModelId vm, int replica_id, Func&& func,
                       Args&&... args) {
  return get_thread_pool().submit(vm, replica_id, std::forward<Func>(func),
                                  std::forward<Args>(args)...);
}

inline void create_queue(VersionedModelId vm, int replica_id) {
  get_thread_pool().create_queue(vm, replica_id, false);
}

inline void delete_queue(VersionedModelId vm, int replica_id) {
  get_thread_pool().delete_queue(vm, replica_id);
}

inline void interrupt_thread(VersionedModelId vm, int replica_id) {
  get_thread_pool().interrupt_thread(vm, replica_id);
}

}  // namespace TaskExecutionThreadPool

namespace EstimatorFittingThreadPool {
/**
 * Convenience method to get the task execution thread pool for the
 * application.
 */
inline ModelQueueThreadPool& get_thread_pool(void) {
  static ModelQueueThreadPool estimator_fitting_pool;
  return estimator_fitting_pool;
}

/**
 * Submit a job to the estimator fitting thread pool
 */
template <typename Func, typename... Args>
inline auto submit_job(Func&& func, Args&&... args) {
  return get_thread_pool().submit(std::forward<Func>(func),
                                  std::forward<Args>(args)...);
}

inline void create_queue(VersionedModelId vm, int replica_id) {
  get_thread_pool().create_queue(vm, replica_id, true);
}

inline void delete_queue(VersionedModelId vm, int replica_id) {
  get_thread_pool().delete_queue(vm, replica_id);
}

}  // namespace EstimatorFittingThreadPool

namespace GarbageCollectionThreadPool {
/**
 * Convenience method to get the garbage collection thread pool for the
 * application
 */

inline FixedSizeThreadPool& get_thread_pool(void) {
  static FixedSizeThreadPool garbage_collection_pool(1, true);
  return garbage_collection_pool;
}

/**
 * Submit a job to the garbage collection thread pool
 */
template <typename Func, typename... Args>
inline auto submit_job(Func&& func, Args&&... args) {
  return get_thread_pool().submit(std::forward<Func>(func),
                                  std::forward<Args>(args)...);
}
}  // namespace GarbageCollectionThreadPool

}  // namespace clipper

#endif  // CLIPPER_LIB_THREADPOOL_HPP
