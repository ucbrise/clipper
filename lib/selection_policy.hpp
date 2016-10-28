#ifndef CLIPPER_LIB_SELECTION_POLICY_H
#define CLIPPER_LIB_SELECTION_POLICY_H

#include <memory>

#include "datatypes.h"
#include "task_scheduler.h"

namespace clipper {

class SelectionState {
  public:
    virtual ByteBuffer serialize() const;
    virtual std::unique_ptr<SelectionState> deserialize(const ByteBuffer buffer) const;
};

/// Note that all selection policy methods are const because
/// any "instance" state is captured in the `SelectionState` instead
/// of as instance member variables of the `SelectionPolicy`. There
/// is no guarantee about which instance of a SelectionPolicy
/// will be used (e.g. it would be perfectly valid, albeit
/// inefficient, to construct a new SelectionPolicy instance on every
/// new method call).
class SelectionPolicy {
  public:
    virtual std::unique_ptr<SelectionState> initialize(std::vector<VersionedModelId> candidate_models) const = 0;
    virtual std::unique_ptr<SelectionState> add_models(std::vector<VersionedModelId> new_models) const = 0;

    // On the prediction path
    virtual std::vector<PredictTask> select_predict_tasks(
        std::unique_ptr<SelectionState> state,
        Query query) const = 0;

    // TODO: change this method name
    // TODO: I think it may make sense to decouple combine_predictions()
    // from select_predict_tasks in some cases
    virtual std::unique_ptr<Output> combine_predictions(
        std::unique_ptr<SelectionState> state,
        Query query,
        std::vector<std::shared_ptr<Output>> predictions) const = 0;

    /// When feedback is received, the selection policy can choose
    /// to schedule both feedback and prediction tasks. Prediction tasks
    /// can be used to get y_hat for e.g. updating a bandit algorithm, 
    /// while feedback tasks can be used to optionally propogate feedback
    /// into the model containers.
    virtual std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>> select_feedback_tasks(
        std::unique_ptr<SelectionState> state,
        Query query) const = 0;

    /// This method will be called if at least one PredictTask
    /// was scheduled for this piece of feedback. This method
    /// is guaranteed to be called sometime after all the predict
    /// tasks scheduled by `select_feedback_tasks` complete.
    virtual std::unique_ptr<SelectionState> process_feedback(
        std::unique_ptr<SelectionState> state,
        Feedback feedback,
        std::vector<std::shared_ptr<Output>> predictions) const = 0;
};

}


#endif // CLIPPER_LIB_SELECTION_POLICY_H
