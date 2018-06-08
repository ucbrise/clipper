import redis

def main():
    redis_conn = redis.Redis()
    pubsub = redis_conn.pubsub()
    pubsub.subscribe(["query_rcvd", "results_rcvd"])
    for item in pubsub.listen():
        state = redis_conn.hgetall('state')
        if item['type'] == 'message':
            if item['channel'].decode("utf-8") == "query_rcvd":
                model_ids = selection_policy(state)
                redis_conn.rpush(item['data'], *model_ids)
            else:
                results = redis_conn.lrange(item['data'], 0, -1)
                final_pred = combine(state, results, 'state')
                redis_conn.set(item['data'].decode('utf-8') + '_final', final_pred)
# Testing code
def selection_policy(state):
    return [1, 2, 3]
def combine(state, results, state1):
    return 14
if __name__== "__main__":
    main()
