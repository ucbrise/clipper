#ifndef CLIPPER_LIB_SELECTION_POLICY_H
#define CLIPPER_LIB_SELECTION_POLICY_H

#include <memory>

#include "datatypes.h"
#include "task_scheduler.h"

namespace clipper {

class SelectionState {
  public:
    virtual ByteBuffer serialize() const = 0;
    virtual std::unique_ptr<SelectionState> deserialize(const ByteBuffer buffer) const = 0;
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
    virtual std::unique_ptr<SelectionState>
      initialize(std::vector<VersionedModelId> candidate_models) const = 0;
    virtual std::unique_ptr<SelectionState>
      add_models(std::unique_ptr<SelectionState> state, std::vector<VersionedModelId> new_models) const = 0;

    // Used to identify a unique selection policy instance. For example,
    // if using a bandit-algorithm that does not tolerate variable-armed
    // bandits, one could hash the candidate models to identify
    // which policy instance corresponds to this exact set of arms.
    // Similarly, it provides flexibility in how to deal with different
    // versions of the same arm (different versions of same model).
    virtual long hash_models(
        std::vector<VersionedModelId> candidate_models) const = 0;




    // On the prediction path
    virtual std::vector<PredictTask> select_predict_tasks(
        std::unique_ptr<SelectionState> state,
        Query query,
        long query_id) const = 0;

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
    virtual std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
      select_feedback_tasks(
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

class SelectionPolicyFactory {
  public:
    static std::shared_ptr<SelectionPolicy> create(std::string policy_name);
};

//////////////////////////////////////////////


class NewestModelSelectionState: SelectionState {
  public:
    virtual ByteBuffer serialize() const;
    virtual std::unique_ptr<SelectionState> deserialize(const ByteBuffer buffer) const;
  private:
    VersionedModelId model_id_;
};


class NewestModelSelectionPolicy : SelectionPolicy {
    virtual std::unique_ptr<SelectionState>
      initialize(std::vector<VersionedModelId> candidate_models) const;

    virtual std::unique_ptr<SelectionState> add_models(
          std::unique_ptr<SelectionState> state,
          std::vector<VersionedModelId> new_models) const;

    virtual long hash_models(
        const std::vector<VersionedModelId>& candidate_models) const;



    // On the prediction path
    virtual std::vector<PredictTask> select_predict_tasks(
        std::unique_ptr<SelectionState> state,
        Query query,
        long query_id) const;

    // TODO: change this method name
    // TODO: I think it may make sense to decouple combine_predictions()
    // from select_predict_tasks in some cases
    virtual std::unique_ptr<Output> combine_predictions(
        std::unique_ptr<SelectionState> state,
        Query query,
        std::vector<std::shared_ptr<Output>> predictions) const;

    /// When feedback is received, the selection policy can choose
    /// to schedule both feedback and prediction tasks. Prediction tasks
    /// can be used to get y_hat for e.g. updating a bandit algorithm, 
    /// while feedback tasks can be used to optionally propogate feedback
    /// into the model containers.
    virtual std::pair<std::vector<PredictTask>, std::vector<FeedbackTask>>
      select_feedback_tasks(
        std::unique_ptr<SelectionState> state,
        Query query) const;

    /// This method will be called if at least one PredictTask
    /// was scheduled for this piece of feedback. This method
    /// is guaranteed to be called sometime after all the predict
    /// tasks scheduled by `select_feedback_tasks` complete.
    virtual std::unique_ptr<SelectionState> process_feedback(
        std::unique_ptr<SelectionState> state,
        Feedback feedback,
        std::vector<std::shared_ptr<Output>> predictions) const;
};


}


#endif // CLIPPER_LIB_SELECTION_POLICY_H
