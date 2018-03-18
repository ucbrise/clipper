import json
import redis
from redis.exceptions import ConnectionError
from schema import Prom_Type

from config import CHANNEL_NAME, DEFAULT_BUCKETS, UNIX_SOCKET_PATH, API_VERSION

r = redis.Redis(unix_socket_path=UNIX_SOCKET_PATH)
metric_pool = set()


def _send_to_redis(messege_dict):
    r.publish(CHANNEL_NAME, json.dumps(messege_dict))


def add_metric(name, metric_type, description, buckets=DEFAULT_BUCKETS):
    """Add a prometheus metric. 

    Parameters
    ----------
    name: string
        The name of the metric. The name need to be unique. If it's repeated, 
        this function call will be ignored.
    metric_type: string
        A prometheus data type. Needs to be one of ['Gauge', 'Counter', 
        'Histogram', 'Summary']
    description: string
        The description for prometheus metric. 
    buckets: list of ints or float
        The histogram buckets for Prometheus Histogram. For example, [1,5,100]. 
        If it's not specificed and the metric_type is 'Histogram', the default buckets
        [5, 10, 20, 35, 50, 75, 100, 150, 200, 250, 300, 400, 500] will be used. 
    """
    assert metric_type in set(
        Prom_Type), "metric type needs to be one of {}".format(
            list(Prom_Type))

    if name in metric_pool:
        return

    messege_dict = {
        'version': API_VERSION,
        'endpoint': 'add',
        'data': {
            'name': name,
            'type': metric_type,
            'description': description
        }
    }

    if metric_type == 'Histogram':
        messege_dict['data']['bucket'] = buckets

    _send_to_redis(messege_dict)
    metric_pool.add(name)


def report_metric(name, val):
    """Report a metric to prometheus

    Parameters
    ----------
    name: string
        The name of metric to be reported. The name must the name of metric that 
        has been added via `add_metric`.
    val: int or float
        The value of metric to be reported. The value must be able to be casted 
        as float. 
    """
    assert name in metric_pool, "{} has not been added. \
    Please use clipper_admin.metric.add_metric to add this metric".format(name)

    messege_dict = {
        'version': API_VERSION,
        'endpoint': 'report',
        'data': {
            'name': name,
            'data': float(val)
        }
    }

    _send_to_redis(messege_dict)
