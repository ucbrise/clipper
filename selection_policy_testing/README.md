# Model Selection Policy Mock Up API
This document will go over the how to utilize the mock up designed for model selection. For more info, please look at the python files, which include comments.
## Query Processor
The Query Processor (QP) is composed of 4 main components:
* [Query Generator](#query-generator)
* [Sender](#sender)
* [Reciever](#reciever)
* [Task Executor](#task-executor)
* [Client](#client)

The following contains a discussion of each component.
### Query Generator
The Query Generator currently produces queries every 50 milliseconds. The default query format is
```python
select_flag = count % 2 == 0 
query = {'user_id': 'rdurrani', 'query_id':count, 'query':[1, 2, 3, 4],
         'msg':'select', 'select_flag':select_flag}
```
where count is a state variable that counts how many queries have been sent so far. The mechanism to produce queries is abstracted
away to the method `gen_query(count)` (found [here](query_processor.py#L20)), which can be overridden by users.

The following 4 attributes are system defined, and must remain true to the following definitions:

* The `user_id` attribute is a new feature added to Clipper that allows state to be stored for specific entities, for example,
users in a recommendation system. If state holds for the entire system, then a default entity name can be generated and used for all queries.

* The `query_id` attribute is simply an id for the query. it **MUST** be unique to each query, since it is used in caching as well as message passing.

* The `query` attribute is merely the data for the model to compute on.

* The `msg` attribute is a flag for the program to know what operation to perform on any query. The Query Processor should send queries with one of the following tags:
    * `select`: This flag tells the Selection Policy frontend to pass the query to the model selection code.
    * `combine`: This flag tells the Selection Policy frontend to pass the query to the combine code that aggregates all predictions into a final result.
    
The Query Generator should be generating queries with `select` for the `msg` attribute.

The `select_flag` attribute is a attribute added to this implementation of the mock up. It can be removed or changed, and more attributes can be added as neccessary.

The Query Generator generates queries at the rate of 1 query every 50 milliseconds. It puts these queries into a queue to be sent to the Selection Policy frontend, as well as stores them in the `query_cache`, so that the Task Executor and Client processes can use the queries' id to look it up.
### Sender
The Sender takes elements from the send queue and sends it over a socket to the Selection Policy frontend, which handles it from there.
### Reciever
The Reciever recieves messages over a socket from the Selection Policy frontend. Based off of the `msg` attribute, it assigns it to one of two queues:
* `execute` - This tag means that the model id's have been selected, and that the Selection Policy frontend is indicating that the query is ready to be passed to the models and computed on. The Reciever puts these queries in the Task Executor's Queue.
* `return` - This tag means that the Combiner has computed a final result, and that the Selection Policy frontend would like this result to be returned back to the Client. The Reciever puts these queries in the Client's queue.

The queries recieved by the Reciever are not in the same form as the queries initially sent. To understand the new formats, please look at the [Selection Policy Frontend](selection-policy-frontend) section.
### Task Executor
The Task Executor takes queries from its queue, and, after using the `query_cache` to load the query, passes them to the model containers to get the predictions. After recieving the predictions, it modifies the original query to include an array of predictions, updates the `query_cache` and puts the new query in the Sender's queue.
### Client
The Client takes queries from its queue, and, after using the `query_cache` to load the query, checks if the prediction is correct and then deletes the query from the `query_cache` to preserve space.

In the actual system, instead of a Client process, the Sender would merely send the results back to the user. The Client process can be modified to do anything else.
## Selection Policy Frontend
The Selection Policy Frontend is composed of 4 main components:
* [Reciever](reciever)
* [Sender](sender)
* [Selection Policy](selection-policy)
* [Combiner](combiner)