import json
import redis
from redis.exceptions import ConnectionError
from schema import Prom_Type

from config import CHANNEL_NAME, DEFAULT_BUCKETS, UNIX_SOCKET_PATH, API_VERSION

r = redis.Redis(unix_socket_path=UNIX_SOCKET_PATH)


def _send_to_redis(messege_dict):
    r.publish(CHANNEL_NAME, json.dumps(messege_dict))


def add_metric(name, metric_type, description, buckets=DEFAULT_BUCKETS):
    assert metric_type in set(Prom_Type)

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


def report_metric(name, val):
    messege_dict = {
        'version': API_VERSION,
        'endpoint': 'report',
        'data': {
            'name': name,
            'data': float(val)
        }
    }

    _send_to_redis(messege_dict)
