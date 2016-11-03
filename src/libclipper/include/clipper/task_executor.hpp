#ifndef CLIPPER_LIB_TASK_EXECUTOR_H
#define CLIPPER_LIB_TASK_EXECUTOR_H

// namespace clipper {
//
// class PredictTask {
//   public:
//     std::shared_ptr<Input> input_;
//     VersionedModelId model_;
//     float utility_;
//     QueryId query_id_;
//     long latency_slo_micros;
// };
//
// /// NOTE: If a feedback task is scheduled, the task scheduler
// /// must send it to ALL replicas of the VersionedModelId.
// class FeedbackTask {
//   public:
//     Feedback feedback_;
//     VersionedModelId model_;
//     float utility_;
//     QueryId query_id_;
//     long latency_slo_micros;
// };
//
// using FeedbackAck = bool;
//
// // auto task_futures = schedule_prediction(tasks);
// // either(timer_future, all(task_futures).then { // lambda
// //  for (auto f : task_futures) {
// //    if f.is_complete() {
// //      //use it
// //
// //    } else {
// //      // sub anytime estimate
// //    }
// //
// //  }
// // }:
// class TaskExecutor {
//   public:
//     virtual std::vector<future<Output>> schedule_prediction(const
//     std::vector<PredictTask>& tasks) = 0;
//     virtual future<FeedbackAck> schedule_feedback(const
//     std::vector<FeedbackTask> tasks) = 0;
//   private:
//     ResourceState resource_state_;
// };
//
// class FakeTaskExecutor: TaskExecutor {
//   public:
//     virtual future<Output> schedule_prediction(const std::vector<PredictTask>
//     t);
//     virtual future<FeedbackAck> schedule_feedback(const
//     std::vector<FeedbackTask> t);
// };
//
// class Scheduler {
//
//   public:
//     virtual void add_model() = 0;
//     virtual void add_replica() = 0;
//
//     virtual void add_model() = 0;
//     virtual void add_replica() = 0;
//
//     // backpressure??
// };
//
//
// } // namespace clipper

#endif  // CLIPPER_LIB_TASK_EXECUTOR_H
