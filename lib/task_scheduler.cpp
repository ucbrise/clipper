
#include <string>
#include <utility>
#include <tuple>

#include <boost/thread.hpp>

#include "task_scheduler.hpp"

namespace clipper {
// class FakeScheduler: Scheduler {
//   public:
//     virtual future<Output> schedule_prediction(PredictTask t);
//     virtual future<FeedbackAck> schedule_feedback(FeedbackTask t);
// };

future<Output> FakeScheduler::schedule_prediction(PredictTask t) {


}

future<FeedbackAck> FakeScheduler::schedule_feedback(FeedbackTask t) {

}

} // namespace clipper
