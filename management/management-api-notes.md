
# Management API: REST vs File-based

__I think the question about providing a REST vs file-based API centers around the ease of manipulating a global file-system that all replicas can read/write in a serving cluster setting.__



## Requirements for the management API

Management functionality can be divided into two components:

1. __Running and monitoring the Clipper serving system: Python object/REST API__
    1. Start Clipper
    1. Stop Clipper
    1. Get model performance metrics (accuracy, correction-policy diversity, etc.)
    1. Get system performance metrics (e.g. cache hit rate, latencies, throughput, memory consumption)
    1. Cluster management: load balancing, replication, availability
1. __Managing the models Clipper serves__
    1. Add a model
    1. Update a model
    1. Rollback a model
    1. Delete model
    1. Set permissions (read, write) on a per-model basis
    1. Alias a model(?)
    1. Inspect who last updated a model, history of a model, etc.
    1. Staged rollout
    
For this second set of functionality, there is a certain amount of metadata about the models that needs to be kept. We can either use existing file-system functionality to track much (but not all) of it, or track it internally.

One argument in favor of the file-system is discoverability, but I'm not sure that's actually a very strong argument.
People know how to do things like add a model, add a new version, etc., but we need to impose some structure on this to implement something like `start()` or `reload()` (e.g. directory structure or naming conventions or something). At that point, we've limited the flexibility of the file API anyway and things like a post-commit hook to write a file to a specific place vs call the REST API start to look very similar.
    


+ Be able to manage models
+ Versioning (updates)
    + including rollback
+ Metrics/monitoring (what happened)
    + Performance of model
    + perf of service (physical performance)
+ naming
+ permissions (who can do what?)
+ availability/load balancing
+ minimize additional dependencies

## Non-requirements:
+ Throughput
+ Security



## REST API

#### Overview

#### Pros
+ Matches the way data scientists are already used to operating
+ Easy to put a GUI around
+ Makes simple things simple

#### Cons
+ Discoverability, extensibility
  + How do you find out what's possibile without reading the docs
  + How do you extend the API
  




### File API

Where does managing a model diverge from managing a file? When does the abstraction "models are just data" break?

Is a model data or code? Or both or neither?

The concept of a model in Clipper has two components:
1. The model evaluation code, written in the ML framework of choice (e.g. Scikit-Learn). This is independent of the model parameters and is static for the lifetime of a model (it's possible to change this by pointing Clipper to a new model wrapper endpoint, but from the perspective of a model wrapper the code is static).
2. The model parameters. This is the "data product," the result of learning using some training dataset. This might be updated frequently (i.e. nightly or multiple times a day). The model wrapper logic should be able to read this updated state with no modifications. On update, we may want to check that certain invariants hold!

The model wrappers should be checked in and treated like regular code (because they are just code).


Let's talk about the model data for a minute. While the model parameters may be structured data such as Parquet
files (this is the format Spark uses to serialize models) we neither want to impose a specific structure nor even enforce structure at all. For example, the idiomatic way to save Scikit-Learn models is essentially pickling them into a binary format.

Model data is just data:
+ Can handle large objects
+ Provides naming, aliasing
+ Can use file system permissions to distinguish who can read, write to models
+ If we enforce an append-only filesystem versioning becomes simple
+ Another advantage of the filesystem API: HDFS, Etcd, Zookeeper expose essentially the same filesystem API so the same Clipper management infrastructure could be used for reading from a variety of filesystems

_To load new models we need a globally readable file system that's also simple to write to._ Is this a common component of a serving cluster?

##### Model data in Git?
Git is designed to manage a versioned filesystem where each change can be traced to a particular user and commit.

__Parts of Git that would be useful__
+ Change tracking (who did what?)
+ Automatically tracks full history, harder to accidentally overwrite a file and lose the changes
+ Already tracks the metadata, provides the tools needed to detect changes and move through versions
+ Being able to check out the full state needed can simplify distributed deployment

__Disadvantages of Git__
+ Yet another dependency. Lots of organizations don't use Git.
+ Imposes restrictions on the file-system API. If we depend on Git for change tracking, we require a file system that Git can be installed on. This rules out things like Tachyon/HDFS, as well as DBs like Etcd, Zookeeper, Redis.
+ One of the most powerful components of Git is the idea that files evolve and change over time (this is how software development works). Things like diffs, patches, etc. all make sense in this context. However, the model data we are considering evolves in a much less granular way. The files/data is machine-generated (the output of training the model) and so entire files or even directories are output at a single time. Because the code is not compiled, model versions can sit next to each other on the file system, and files should probably never be overwritten. Furthermore, even though the idea of a model diff does make sense, Git's diff algorithm is almost certainly the wrong one to use. Instead, a much higher level semantic diff probably makes more sense. And this ties back into enforcing that model versions obey some set of invariants.
    + As an aside, the part of the software development workflow that model versioning/updating probably mirrors
      most closely is continuous integration and automated testing, rather than version control
+ Git is not equipped to handle large files. Part of this is the diffing/hashing that Git uses to track versions. This can be worked around, but I think it is a consequence of the use-case mismatch between software development version control and model parameter data.







### Clipper management use cases

How do these work in the single-server, data-scientist setting? How do these work in the enterprise serving cluster setting?


##### Add a model
+ Distribute model wrapper code and any dependencies to Clipper instances
+ Ship model data to Clipper instances
+ Start model wrapper and provide model data as arg

##### Update a model version
+ Ship new model data to Clipper instances
+ Restart model wrapper providing new model data

##### Rollback to a previous model version


##### Delete a model

##### Inspect who last updated a model

##### Set permissions (read, write)

##### Alias a model

## TODO
+ Invariants, post-commit hooks discussion
+ I worry that we are over-engineering this for now based on very limited information about use cases

File-based API:
`start(), stop(), watch(), reload()`

## What does a Clipper cluster look like?

Set of Clipper instances (Docker container?), each running its own set of model wrappers. Each replica will have its own local cache internally.

Two cluster modes:
1. Stateless, replicated Clipper. In this mode, all Clipper instances are identical and any instance can serve any request. Traffic can be distributed via a load-balancer. All correction model state is stored in a globally readable/writable DB, and on every request user's correction model state is read from this DB.
2. Partitioned Clipper. In this mode, each Clipper instance (or set of replicas) is responsible for a disjoint subset of the user keyspace. This means that all user requests go to the same node so correction model state can be managed locally. But this can lead to hotspots or poor load balance, and if a node goes down we have to manage failover somehow.

# Issues to discuss:
+ The easiest API for the single data-scientist (single replica) use case is a Python management object + file-system API.
+ The problem is that managing a serving cluster is different from a single server, so best way to manage that additional complexity is by providing more powerful tools.
+ I think the question about providing a REST vs file-based API centers around the ease of manipulating a global file-system that all replicas can read/write in a serving cluster setting.

