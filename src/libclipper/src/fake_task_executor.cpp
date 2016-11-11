
#include <string>
#include <utility>
#include <tuple>

#include <boost/thread.hpp>

#include <clipper/task_scheduler.hpp>
#include <clipper/datatypes.hpp>

namespace clipper {

future<Output> FakeTaskExecutor::schedule_prediction(PredictTask t) {
  Output output{1.0, t.model_};
  return boost::make_ready_future<Output>(output);
}

future<FeedbackAck> FakeTaskExecutor::schedule_feedback(FeedbackTask t) {
  return boost::make_ready_future<FeedbackAck>(true);
}

} // namespace clipper
