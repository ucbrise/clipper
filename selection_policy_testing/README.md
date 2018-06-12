# Model Selection Policy Mock Up API
This document will go over the how to utilize the mock up designed for model selection. For more info, please look at the python files, which include comments.
## Query Processor
The Query Processor (QP) is composed of 4 main components:
* [Query Generator](#query-generator)
* [Sender](#sender)
* [Task Executor](#task-executor)
* [Client](#client)

The following contains a discussion of each component.
### Query Generator
The Query Generator currently produces queries every 50 milliseconds. The default query format is
```python
select_flag = count % 2 == 0 
query = {'user_id': 'rdurrani', 'query_id':count, 'query':[1, 2, 3, 4], 'msg':'select', 'select_flag':select_flag}
```
where count is a state variable that counts how many queries have been sent so far. The mechanism to produce queries is abstracted
away to the method `gen_query(count)` (found [here](query_processor.py#L20)), which can be overridden by users.
### Sender
### Task Executor
### Client 