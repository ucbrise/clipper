# Clipper Design Document

[TOC]

## System Overview

+ Clipper: The main serving module that receives queries from
frontend applications and dispatches them to models for predictions. This module is the focus of this design document. Clipper runs as a standalone process.
+ Model containers: Docker container running a lightweight wrapper around a _single_ deployed model. Model containers communicate with Clipper over RPC.
+ Monitoring: Clipper employs InfluxDB (a time-series DB) and Grafana (a dashboard and visualization library) to store and display various metrics about the system.
+ Persistent Storage: All persistent state in Clipper is stored in Redis for fault-tolerance and persistence.
+ Management: Clipper provides a management utility that can be used to query and update the state of a running Clipper cluster. This state includes which versions of which models are currently deployed, the number of replicas of each model, who deployed a model, and several others.

Internally, Clipper is organized into two pieces, a library that contains all
of the Clipper functionality exposed through the creation of a
`ClipperServer` object and a set of Clipper frontends that receive queries in
a variety of ways and pass these queries to a `ClipperServer` object for
processing. This separation makes it simple to add new frontends to Clipper.
The current Clipper-Rust prototype has a REST frontend and several benchmarking
frontends that generate queries in-process and dispatch them directly to the
ClipperServer object. Going forward, we likely will want to provide an RPC
frontend using a standard RPC library.
Finally, if applications want to link directly with Clipper and dispatch queries directly, they can use the Clipper library directly and link to the dylib.


## Implementation of Clipper Library

The Clipper library is further divided into several modules. 

This section walks through the code path of a few of the commands that `ClipperServer` provides to illustrate how these modules fit together.

#### Life of a Predict Query

A prediction query is represented as a `PredictionRequest` object,
containing the input, user ID, an explicitly enumerated set of candidate
models that can process this query, and a callback to be executed once the response is available.

```rust
struct PredictionRequest {
  uid: u32,
  input: Input,
  candidate_models: List<VersionedModel>,
  recv_time: Timestamp,
  // callback executed once prediction is available
  on_predict_callback: Fn(Output) -> (),
  // In the current implementation, slo is assumed to be
  // the same for all queries.
  slo: Duration
}
```

Prediction queries are submitted to Clipper through the
`ClipperServer::schedule_prediction()` method. Once a query is submitted
to Clipper, the system forwards it to one of the prediction
workers for pre-processing (predictions are randomly partitioned among
the workers) by pushing it to the work queue of the prediction worker.
Pre-processing consists of model selection based on current per-user
selection policy state. The prediction worker fetches the user's state from
Redis and selects which models to use for query execution using the selection
policy and fetched state, independent of the query input.
For each model selected, the prediction worker checks the cache to see if
prediction for this query is cached. On each cache miss, the query is forwarded
to the model's prediction batcher by pushing the query onto the batcher's
work queue. If the prediction is already cached, the prediction worker
does nothing (based on the assumption that the entry is unlikely to be evicted 
between now and when the query response is generated at the end of the
latency objective). Once the query has been forwarded to all the necessary
prediction batchers for evaluation, a timer is set to go off at the end
of the latency objective. Because the slo is a duration (offset), the end
of the latency objective is `query.recv_time + query.slo`.

When the timer fires, the prediction worker looks up the predictions from
all of the selected models in the cache, tracking which predictions (if any)
resulted in a cache miss. These predictions along with the user selection state fetched earlier from Redis are passed to the model selection policy's predict
implementation which generates the final prediction. The prediction query callback
is then executed with the final prediction passed to the callback function
as an argument.

In seperate threads, each of the prediction batchers processes its work queue
to generate predictions within the latency objective of the queries. The
prediction batcher runs in an infinite loop. In each iteration of the loop,
it dequeues requests from its work queue, limiting the number of requests
dequeued according to the batch size found by the adaptive batching algorithm.
If the work queue contains less than the maximum number of requests, the batcher
will delay sending the batch for a short, configurable period of time (generally
set between 0-2ms) to allow more requests to come in. A batch of requests
is sealed once it reaches either the maximum size or is delayed the maximum
delay. Once a batch is sealed, it is serialized into an RPC request and sent
to the model for predictions in a blocking call. The call returns when
the model has computed the predictions for _all requests_ in the batch
and returned them via the RPC interface. Once the predictions for a batch
have been returned, the batcher inserts each request into the prediction
cache for its model and starts the next iteration of the loop.

#### Life of an Update Query


#### Life of a Model Addition Request

### Clipper Server

Implemented in `src/clipper/server.rs`

API of the ClipperServer:

### Prediction Worker

### Update Worker

### Model Resource Tracker (`ModelSet`)

Currently, this module stores metadata tracking which models are currently
deployed in Clipper and available to query.

Going forward, this should be extended to managing all of the metadata
about the runtime state of a Clipper cluster. This certainly includes
(version) history and current state of each of the models (as well as potentially
measured information about the performance of each container),
but may include other information as well. The management utility in Clipper
will then correspond to reads and writes into this metadata store. A write
can have triggers associated with it (e.g. an insert into the model table
means that a new model has been deployed in Clipper and the system can
take the appropriate actions).


### Batching

Implemented in `src/clipper/batching.rs`

The batching module manages the connections to each model container and issues
the RPC `predict_batch` call. The primary responsibility of the batching module
is to schedule these batched prediction requests in order to meet latency
objectives and maximize throughput. In practice, by the time queries reach the
batching module, the scheduling decisions are a) how large of a batch to
schedule and b) how long to delay issuing the next `predict_batch` call (small
delays allow more queries to accumulate and therefore let Clipper schedule a
larger batch which can significantly improve throughput for some model
containers). Queries can also have an optional TTL attached to them.
The batching module will filter queries whose TTL has expired before
they were able to be scheduled for prediction.

Clipper creates a separate instance of a `PredictionBatcher` for each `VersionedModel`. The `PredictionBatcher` then spawns a separate thread for
replica of the model. The end result is that there is a separate thread
handling the connection to each model container.
To make the scheduling
decisions, a `PredictionBatcher` instance measures the _latency profile_ of
a container by measuring evaluation time (latency) of a `predict_batch` RPC call
as a function of batch size. The determine the maximum batch size to use we use
an adaptive Additive-Increase-Multiplicative-Decrease algorithm, implemented
in `fn update_batch_size_aimd()`. Clipper also allows the maximum batch size
to be set statically when a `PredictionBatcher` is created. Static batch
sizes are primarily used for model containers running deep networks on GPUs,
where the batch size is hard-coded into the model architecture.

The responses from a call to `predict_batch` are inserted into the
shared prediction cache once they are available.

API:
```rust
// Constructor
pub fn new(name: String,
           input_type: InputType,
           cache: Cache,
           slo_micros: u32,
           wait_time_nanos: u64,
           batch_strategy: BatchStrategy)
           -> PredictionBatcher;

pub fn request_prediction(&self, req: RpcPredictRequest);
```




__Potential changes to the design:__

+ Have model containers connect to Clipper instead of the other way around.
Clipper is much longer-lived than any individual model container (which can be
dynamically created throughout the lifetime of a Clipper instance).  If we
implement this, then when initiating a connection the container must
communicate its name, version, and some other configuration information to
Clipper. This will result in non-trivial changes to how Clipper handles model
additions, updates, rollbacks and replication decisions.
+ Rather than having each batcher managing its own TCP connection in a separate
thread, we could have a single read thread and single write thread doing async
socket IO.


### RPC

Implemented in `src/clipper/rpc.rs`

The RPC module implements the RPC mechanism, serializing requests
and writing them to the open TCP connection to the model container
hosting the RPC server, then blocking until a complete response is read from
the same TCP stream and deserialized.

Clipper currently uses a custom RPC and serialization mechanism.
It was difficult to find a simple and fast RPC implementation when we
started, and Clipper does not (currently) need a general-purpose RPC mechanism
because the RPC layer actually only supports a single method: `predict_batch`.
`predict_batch` is parameterized by different data types, and Clipper
currently supports 7 types of input:

1. Fixed length integer vectors
2. Variable length integer vectors (different vectors in the batch can have
    different dimension)
3. Fixed length float64 vectors
4. Variable length float64 vectors
5. Fixed length byte vectors
6. Variable length byte vectors
7. Variable length strings

#### RPC Format Specification

__INPUT SPECIFICATION:__

This is the specification of how `predict_batch` messages are serialized and
sent over the wire. This serialization logic is implemented in the
the RPC module. Each implementation of the model container RPC logic must be able
to deserialize messages in this format (e.g. once for each programming language).

+ _1 byte: input format specifier_ (the item number specifies the const byte
    value. E.g. for a message containing variable length f64s, the first byte
    would be set to 5):
    1. fixed length i32
    2. fixed length f64
    3. fixed length u8
    4. variable length i32
    5. variable length f64
    6. variable length u8
    7. variable length utf-8 encoded string
+ _4 bytes: the number of inputs as u32_
+ Format specific encodings:
    + Fixed length formats {1,2,3}:
        + _4 bytes: input length_
        + The input contents sequentially
+ Variable length numeric inputs:
    + _4 bytes_: number of content bytes (so we know how much to recieve)
    + Each input has the format: `<4 bytes for the input length, followed by the input>`
+ Strings:
    + _4 bytes_: number of content bytes (so we know how much to recieve)
    + A list of 4 byte i32s indicating the length of each individual string when 
  uncompressed, followed by all of the strings concatenated together and 
  compressed with LZ4


__OUTPUT SPECIFICATION:__

For the sake of simplicity, Clipper has restricted the model container output
to be a single float per input per container. As a result, the output message
is just an array of floats with length equal to the input batch size. This
is the case regardless of the input type.

_Going forward, this is too restrictive. I have yet to decide on a good API
for expressing output type._

__API__
```rust

pub fn send_batch(stream: TcpStream,
                  inputs: Vec<RpcPredictRequest>,
                  input_type: InputType);

// close connection to the model container
pub fn shutdown(stream: TcpStream);
```


__Potential changes to the design:__

+ We should probably switch to a standard serialization object serialization format. Possibilities include Protobuf, Cap'n Proto (which has excellent C++ and Python support).

### Model Selection/Correction

Generally in Clipper there are multiple deployed models that can be
used to execute a query. The set of candidate models (and versions) is attached
to a query. The correction policy interface provides methods to specify
how to _select_ a model or set of models from the candidates to use
for query execution. It also specifies how to combine predictions from
multiple models together into single response for the client. Finally, when
labeled feedback is received, Clipper passes this feedback to the correction
policy which can use it to update any internal state.

The correction policy is queried before predictions are dispatched to
determine which models to use, and then queried at the end of the
latency objective to aggregate predictions from multiple models into a
single response and compensate for missing predictions (i.e. predictions
that were not evaluated within the latency objective).

__Correction Policy State:__ The correction policy state (e.g. ensemble model
    parameters) is the most important mutable state that Clipper stores.
While correction policy implementations must implement an interface,
the interface only contains static methods and state is explicitly managed
by Clipper and passed into these methods as needed. This has several advantages.
First, Clipper forces the state to be serializable and stores a serialized
version in a database (Redis) for fault-tolerance and persistence, ensuring
that the state survives a system crash. Further, this state may need to be
shared among multiple instances of Clipper running on different machines
in a cluster for high-availability or scaleout reasons. By managing the state
explicitly, we can choose the appropriate tradeoff between consistency and
performance simply by changing the policies around how Clipper manages this
explicit state (e.g. changing the underlying DB).
Different sets of candidate models may result in different policy state,
so internally Clipper stores a map `(Uid, Set<VersionedModel>) -> State`
(note the use of Set not List, the mapping is dependenent only on membership
not order),
mapping the pair of user and set of candidate models to the serializable
state.

It remains an open question whether there is a good way to initialize the
state for a new candidate model set from an existing (and overlapping)
candidate model set. For example, if we update the version of a single
model should we initialize the new state with the default correction
state or the correction state from the candidate model set containing
the old version.

__API:__
```
trait CorrectionPolicy<S> where S: Serialize + Deserialize {
    fn new(models: Vec<&String>) -> S;

    fn accepts_input_type(input_type: &InputType) -> bool;

    fn get_name() -> &'static str;

    fn predict(state: &S,
               predictions: HashMap<String, Output>,
               missing_predictions: Vec<String>)
               -> Output;

    /// Prioritize the importance of models from highest to lowest priority.
    fn rank_models_desc(state: &S, model_names: Vec<&String>) -> Vec<String>;

    /// Update the correction model state with newly observed, labeled
    /// training data.
    fn train(state: &S,
             inputs: Vec<Arc<Input>>,
             predictions: Vec<HashMap<String, Output>>,
             labels: Vec<Output>)
             -> S;
}
```

### Caching


__Current API:__
```rust

/// Handles higher-level semantics of caching predictions. This includes
/// locality-sensitive hashing and other specialized hash functions.
pub trait PredictionCache<V: 'static + Clone> {
    /// Look up the key in the cache and return immediately
    fn fetch(&self, model: &String, input: &Input, salt: Option<i32>) -> Option<V>;

    /// Insert the key-value pair into the cache, evicting an older entry
    /// if necessary. Called by model batch schedulers.
    fn put(&self, model: String, input: &Input, v: V, salt: Option<i32>);

    fn add_listener(&self,
                    model: &String,
                    input: &Input,
                    salt: Option<i32>,
                    listener: Box<Fn(V) -> () + Send + Sync>);
}


```

__Proposed API:__
```rust

// Returns a future that will be filled when the cache lookup
// completes. This requires Future to have a Future:is_complete()
// method that indicates whether the value is available yet without
// invalidating the Future if it isn't complete.
//
// This replaces the fetch and add_listener of the current implementation
// with a single method that returns a future.
fn fetch(model: VersionedModelId, input: Input) -> Future<Output>;

fn put(model: VersionedModelId, input: Input, output: Output);

```


### Configuration

Implemented in `src/clipper/configuration.rs`.

This is a very simple module to set various configuration options at
initialization time. Configuration options can be either set programmatically
or parsed from a TOML configuration file.

### Metrics and Monitoring

### Data Types

__Input and Output Specification__

```rust
// Specifies the input type and expected length. A negative length indicates
// a variable length input. `Str` does not have a length because Strings
// are assumed to be always be variable length.
#[derive(Clone, PartialEq, Debug, Serialize)]
pub enum InputType {
    Integer(i32),
    Float(i32),
    Str,
    Byte(i32),
}


#[derive(Clone,Debug,Serialize,Deserialize, PartialEq, PartialOrd)]
pub enum Input {
    Str { s: String },
    Bytes { b: Vec<u8>, length: i32 },
    Ints { i: Vec<i32>, length: i32 },
    Floats { f: Vec<f64>, length: i32 },
}

pub type Output = f64;
```

__Predictions__

```rust
pub type OnPredict = Fn(Output) -> () + Send;

pub struct PredictionRequest {
    recv_time: PreciseTime,
    uid: u32,
    query: Arc<Input>,
    salt: Option<i32>,
    on_predict: Box<OnPredict>,
    // Specifies which offline models to use by name
    // and version. If offline models is `None`, then the latest
    // version of all models will be used.
    //
    // This allows Clipper to split requests between different
    // versions of a model, and eventually will open the way
    // to allow a single Clipper instance to serve multiple
    // applications.
    offline_models: Option<Vec<VersionedModel>>
}
```

__Updates__

```rust
pub struct Update {
    pub query: Arc<Input>,
    pub label: Output,
}

pub struct UpdateRequest {
    recv_time: PreciseTime,
    uid: u32,
    updates: Vec<Update>,
    offline_models: Option<Vec<VersionedModel>>,
}
```


## Problems with the current architecture

+ Concurrency is all static, thread-based concurrency. This worked fine
for the prototype, but I think will lead to problems going forward (all
the problems of forcing the OS to do thread scheduling rather
than doing it internally in the application).
+ Clipper launches a separate thread for each connection to a model container.
This significantly limits the number of concurrent active containers Clipper can
support, even if each of the containers has a relatively small query workload.
+ Model Selection
    + Is the current API sufficient for supporting bandit algorithms, selection
    both before and after querying, input and output transformations?
    + I'd like to consolidate the model monitoring API and the model selection
    API into a single API.
    + There are some problems with how we manage selection policy state
    versioning. Specifically, the state is uniquely identiefied by
    the key `(UserID, List<VersionedModel>)`, and there is no relationship
    between model states with the same user and overlapping lists of models.
    This means that if we have a selection policy that has learned the best
    model over many candidate models, and then a data scientists deploys a new
    model to Clipper, the selection state will be re-initialized from scratch.
    I think the solution is to add a
    `fn add_model(state: S, m: VersionedModel) -> S` and a
    `fn update_model_version(state: S, old_m: VersionedModel, new_m: VersionedModel) -> S` to the selection policy API.
+ The timer system for ensuring that Clipper responds to requests by the end
the latency objective is very hacky. The biggest problem is that assumes that
all requests have the same latency objective (and thus the priority of requests
can be managed with a simple FIFO queue). I would like to support queries
with different latency objectives within the Clipper, which necessitates a real
high-performance timer system with task-dispatch multiplexed over a threadpool.
I'm not sure how to implement this efficiently though.
+ There are a few places that take callbacks that should instead return futures.
    + In the cache, instead of registering a callback there should be a call
    that is
    `fn fetch_future(m: VersionedModelId, input: Input) -> Future<Output>`.
    This method is used when we want to block until a prediction is available.
    + `Clipper::schedule_prediction()`, the frontend API call to actually
    make a prediction, takes a callback `Fn(Output) -> ()` that is executed
    when the prediction is available. Instead, `schedule_prediction()` should
    return a `Future<Output>` that is completed when the prediction is available.


## Proposed new components:

### Change batching to scheduling


Currently the scheduling logic is split between the prediction workers
and the batchers, and much of it is hardcoded into the implementation.
The prediction workers choose which models to use for each query (based on
the candidate models for a query, the model selection policy, and the workload).
Each deployed model in Clipper has a separate batcher.
When the worker schedules a query for evaluation on a model, it places it on
the end of batcher work queue for that model. The sole responsibility of
a batcher is to process the requests in its work queue at the highest
rate possible while respecting request latency objectives.
Because the batchers do 



The proposed scheduler has several decisions to make:

#### At Each Scheduling Epoch (~10ms):

1. Which models to use to evaluate each query in the current epoch? The
result of this decision is a mapping `Query -> List<ModelId>` for all queries
scheduled for evaluation in this epoch (incidentally, how do we choose
which queries get scheduled for this epoch?). This decision would
ideally account for accuracy, cost, fairness.
2. Given the mapping of queries to models and a set of available workers
(e.g. computational resources) to
schedule model evaluation on, how does Clipper assign models to workers?
3. Once models have been assigned to workers, how do we execute the
queries assigned to a model? E.g., send one big batch? Stream them?


#### Out of Band Decisions (~100ms-10s):

1. How should we allocate or deallocate model containers? When making
query scheduling decisions, Clipper can only schedule allocated model
containers. If a particular model sees a large increase in query demand,
then it would be beneficial to allocate more replicas of the container.
Similarly, if a model sees a drop in demand (e.g. becomes much less useful),
we may want to deallocate it.
    + These decisions become particularly important as the number of deployed
    models grows into the hundreds or thousands.



### New model interface

```
interface Model {
    
    predict(x: Input, session: UID) -> y: Output, confidence: Option<float>;
    correct(correction: Feedback, session: UID);
    accuracy() -> float;
    

}

```



## User profiles

### Data scientist

__Tasks:__ Deploy models, monitor and diagnose model performance, query models
for further development.

The data scientist should not need extensive software engineering skills to
deploy a model into production. She should be able to focus on her job of
building high-value models and leverage Clipper to maximize serving performance
of a model.

Clipper should also provide her with the appropriate API to monitor the
performance of her models, both in isolation and in aggregate.


### Devops

__Tasks:__ Manage Clipper deployment. Specifically, manage hardware resources,
  change size of cluster, ensure high-availability in the face of software
  or machine failure, ensure a dependence on Clipper doesn't impact
  end-to-end serving performance.

