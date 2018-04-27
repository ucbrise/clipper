from __future__ import absolute_import
import json
import redis
from redis.exceptions import ConnectionError

from ..exceptions import ClipperException
from .schema import Prom_Type
from .config import CHANNEL_NAME, DEFAULT_BUCKETS, UNIX_SOCKET_PATH, API_VERSION

r = redis.Redis(unix_socket_path=UNIX_SOCKET_PATH)
metric_pool = set()


def _send_to_redis(message_dict):
    r.publish(CHANNEL_NAME, json.dumps(message_dict))


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
    if metric_type not in set(Prom_Type):
        raise ClipperException("Metric type needs to be one of {}".format(
            list(Prom_Type)))

    if name in metric_pool:
        return

    message_dict = {
        'version': API_VERSION,
        'endpoint': 'add',
        'data': {
            'name': name,
            'type': metric_type,
            'description': description
        }
    }

    if metric_type == 'Histogram':
        message_dict['data']['bucket'] = buckets

    _send_to_redis(message_dict)
    metric_pool.add(name)


def report_metric(name, val):
    """Report a metric to prometheus

    Parameters
    ----------
    name: string
        The name of metric to be reported. The name must the name of metric that
        has been added via `add_metric`.
    val: int or float
        The value of metric to be reported. The value must be able to be cast
        to float.
    """
    if name not in metric_pool:
        raise ClipperException("{} has not been added. \
    Please use clipper_admin.metric.add_metric to add this metric"
                               .format(name))

    message_dict = {
        'version': API_VERSION,
        'endpoint': 'report',
        'data': {
            'name': name,
            'data': float(val)
        }
    }

    _send_to_redis(message_dict)
