#ifndef CLIPPER_LIB_TASK_SCHEDULER_H
#define CLIPPER_LIB_TASK_SCHEDULER_H

namespace clipper {

class PredictTask {
  public:
    std::shared_ptr<Input> input_;
    VersionedModelId model_;
    float utility_;
    QueryId query_id_;
    long latency_slo_micros;
};

/// NOTE: If a feedback task is scheduled, the task scheduler
/// must send it to ALL replicas of the VersionedModelId.
class FeedbackTask {
  public:
    Feedback feedback_;
    VersionedModelId model_;
    float utility_;
    QueryId query_id_;
    long latency_slo_micros;
};

using FeedbackAck = bool;

class Scheduler {
  public:
    virtual future<Output> schedule_prediction(PredictTask t) = 0;
    virtual future<FeedbackAck> schedule_feedback(FeedbackTask t) = 0;
};

class FakeScheduler: Scheduler {
  public:
    virtual future<Output> schedule_prediction(PredictTask t);
    virtual future<FeedbackAck> schedule_feedback(FeedbackTask t);
};


} // namespace clipper


#endif // CLIPPER_LIB_TASK_SCHEDULER_H
