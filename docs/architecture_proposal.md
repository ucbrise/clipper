# Proposed Architecture

## Data Types

```
struct Query {
  user_id: u64,
  input: Input,
  latency_micros: u64,
  candidate_models: Set<VersionedModel>,
  // Alternative:
  candidate_model_predicate: (List<ModelKeys>, All/Any),
  selection_policy: SelectionPolicy,
}

struct Feedback {
  user_id: u64,
  updates: List<Update>,
  candidate_models: Set<VersionedModel>,
  // Alternative:
  candidate_model_predicate: (List<ModelKeys>, All/Any),
  policies_to_update: List<SelectionPolicy>,
}

struct VersionedModel {
  id: VersionedModelId,
  keys: <ModelKey>,
  // ???
}

// Unspecified types

struct Input {
  // ???
}

// Things this may need to express: confidence, regression/classification
// labels, which model or models were used to make the prediction?
struct Output {
  // ???
}

// One option for feedback is just an (Input, Output) tuple.
// However, feedback is likely to be more general than
// explicit labeled pairs for supervised learning.
struct Feedback {
  // ???
}

// The big open question here is whether we want to impose
// any sort of comparison operations on models? If so, what
// are the comparison semantics?
// We also may need to compare sets of VersionedModels in a
// version aware way. It depends on the versioning semantics I guess.
struct VersionedModelId {
  // ???
}


// A way to associate properties with models.
struct ModelKey {
  // ???
}

```

### Input
At a minimum, input should be able to capture raw bytes (audio
or video signal, some images),
feature vectors (arrays of ints or floats), and strings (both
for text and for structured documents like JSON).

### Output




## API

__Query Processing Frontend__

```
impl ClipperServer {
  // We return both the final prediction and the set of models
  // used to make the prediction. This will make sense to the user
  // because the query includes a list candidate models, so the returned
  // list will be a subset of the candidate list.
  fn predict(query: Query) -> Future<(Output, List<VersionedModelId>)>;
  fn send_feedback(feedback: Feedback) -> ();
}
```





__Monitoring and Selection Policy__

```
impl SelectionPolicy {
  // On the prediction path
  // pre-process
  fn select_predict_tasks(state: S, query: Query) -> List<Task>;
  //post-process
  fn combine(state: S, query: Query, predictions: List<Output) -> Output;
  
  // On the feedback path
  // pre-process
  // select_feedback_tasks(state: S, feedback: Feedback) -> List<Task>;
  // post-process
  fn process_feedback(state: S, feedback: Feedback) -> S;
}
```

This API enables a few critical pieces of functionality. It allows both
selection of which models to run _a priori_ and allows pre-processing
of the inputs, paving the way for addressing e.g. covariate shift in
the selection policy. The emitted `Task`s don't need to have the same
input as the raw input from the `Query`, this can be modified.
Learned information about covariate shift can be stored in the `State`.

Note that the list of predictions in `fn combine()` contains predictions
from a subset of the `candidate_models` in the `Query`. Also note that
the tasks returned from `fn select_predict_tasks()` are presumably all prediction
tasks, whereas the tasks returned from `fn select_feedback_tasks()` may
be any type of RPC call (predict, feedback to propagate feedback to the models,
or some combination of both).


_Open Question:_ What is the right API to allow users to implement their
own model monitoring tests? Some of this can be done on a per-query basis,
but others may need to compute some aggregate measures that are non-monotonic
such as clustering or outlier detection.

## Query/Feedback Processing

#### HIGHLY LATENCY SENSITIVE TASKS: Threadpool 1

__Task A: Pre-process Prediction__

Fetch any necessary policy state, select which models this query should
be forwarded to, check the cache for any pre-computed predictions, then schedule
a Model Prediction task for each selected model (potentially along with an
accuracy value). Finally, set a timer to fire at the end of the latency
objective.

Called by `Clipper::schedule_prediction()` when a prediction is first received.

__Task B: Post-process Prediction__
Run when a timer fires (when the single-threaded timer event-loop detects
a timer has fired), this task is run in a separate thread. It queries
the cache for available predictions from the candidate set of predictions
and uses these predictions and the selection state to return a response.
Once this response is computed, it completes the `Promise` associated with
the returned future.


#### LESS LATENCY SENSITIVE TASKS: Threadpool 2
__Task C: Model Prediction__

All Model Prediction tasks are sent to the prediction scheduler. Each task
has a `(Input, VersionedModelId)` pair to identify which model it runs on.
The scheduler groups these tasks together and decides where to run each model.
Once the scheduling decisions are made, the scheduler schedules an Prediction RPC
task for each model that contains all the inputs for that model.

_Open question:_ Do we also associate a `QueryId` and potentially a `Value`
with the task as well? The reason for doing this would be to let the scheduler
optionally factor in the value of a task for processing a query. This would let
queries request many model predictions and let the scheduler remove
the low-value ones under heavy load. At some point, we want to factor in
both prediction cost and value together when deciding whether to
run a prediction.

_Open question:_ Does scheduler need concurrency?

__Task D: Prediction RPC Request__

Once Prediction RPC tasks are serialized (where does this happen),
the byte buffers are passed to a single-threaded event loop that writes
the buffers to each socket for tranmission to model containers.

_Open question:_ Are Prediction RPC tasks pre-serialized and ready for sending?

__Task E: Prediction RPC Response__
When RPC event loop receives a response from a model container, it reads
the bytes into a buffer then schedules a task to run in a separate thread
(from an existing threadpool) to process the bytes. This processing
deserializes the responses and puts them in the prediction cache.
It also records the latency of the request for future use in cost-based
scheduling.


__Task F: Prep Update__

Fetch policy state, collect futures of all needed predictions by querying
the cache, schedule any needed Model Prediction tasks for entries missing
from the cache, then schedule a Compute Update task to run when the futures
have all completed.

__Task G: Compute Update__

Call `SelectionPolicy::update()` and write the new selection state to
Redis.




#### Other processes

__Timer event loop__ Single-threaded event-loop waiting for timers to fire

__RPC IO event loops__ Single-threaded event loop (or maybe a read thread
and a write thread) for reading and writing to sockets for RPC connections
between Clipper and the active model containers.

## Scheduler

```
struct Task {
  input: Input,
  model: VersionedModelId,
  query_id: QueryId,
  // only comparable to other tasks with the same query_id
  value: f32,
  latency_slo_micros: u64,
  // Can be predict, feedback, etc.
  // See impl Model for the full list of RPC
  // calls available to model containers.
  type: TaskType
}

impl Scheduler {
  fn schedule_prediction(task: Task);
}
```

The first version of the scheduler can re-implement the existing
adaptive batching strategy. This `BatchingScheduler` will make no decisions
about where to place model containers, only deciding when to execute RPC
requests by obeying a `max_delay` setting and limiting the batch size
based on the estimate from the adaptive algorithm.

## Management

__API__

```
  // Writes
  fn add_container_type(user, loc)
  fn insert_model(user, name, version, container_type, model_data)
  fn update_model_version(user, name, version, model_data)
  fn remove_model(user, name, version)

  // Reads can be arbitrary queries on the DB

```


The state of a Clipper cluster is stored and managed in configuration (metadata)
database. We can start with something internal, but eventually this should be
Etcd (or possibly Zookeeper).

The configuration database includes a `DeployedModelsTable`.
Users are responsible for deploying new models or new versions and deleting
old versions, but Clipper manages the creation and replication of the actual
model containers. We can have a special container type 
(set `replicable` to `False`) that works the way
containers work today (container is expected to be running already, Clipper just
connects, can't replicate) to be used for e.g. hosted ML services, AMPCrowd
resources, etc.

```
DeployedModelsTable
model_id | name | version | container_type | 
```

```
ContainerTable
id | name | container_name | container_location | replicable

```

_Open Question:_ Should we store the info about the active model containers
in the configuration DB as well? This table would be different for
different containers.

## Model Containers

__Model Container API (RPC Interface)__

```
// Required
fn predict_batch(inputs: List<Input>) -> List<Output>

// Under consideration
fn feedback(feedback: Feedback) -> Ack;
fn accuracy() -> u64

```

