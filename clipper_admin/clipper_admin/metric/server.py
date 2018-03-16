from prometheus_client import start_http_server, Gauge, Counter, Histogram, Summary
import redis
import json
import logging
import sys
from subprocess import call
import psutil
from schema import validate_schema, Prom_Type

from config import CHANNEL_NAME, DEFAULT_BUCKETS, UNIX_SOCKET_PATH


class Metric:
    def __init__(self, name, metric_type, description, buckets):
        self.name = name
        self.type = metric_type

        if metric_type == 'Counter':
            self._metric = Counter(name, description)
        elif metric_type == 'Gauge':
            self._metric = Gauge(name, description)
        elif metric_type == 'Histogram':
            self._metric = Histogram(name, description, buckets=buckets)
        elif metric_type == 'Summary':
            self._metric = Summary(name, description)

    def report(self, value):
        value = float(value)

        if self.type == 'Counter':
            self._metric.inc(value)
        elif self.type == 'Gauge':
            self._metric.set(value)
        elif self.type == 'Histogram' or self.type == 'Summary':
            self._metric.observe(value)


def add_metric(name, metric_type, description, buckets, metric_pool):
    metric_pool[name] = Metric(name, metric_type, description, buckets)


def report_metric(name, val, metric_pool):
    if name in metric_pool:
        metric_pool[name].report(val)
    else:
        logger = logging.getLogger(__name__)
        logger.error("{} not found in metric pool: {}".format(
            name, metric_pool.keys()))


def handle_messege(messege_dict, metric_pool):
    endpoint = messege_dict['endpoint']
    data = messege_dict['data']

    if endpoint == 'add':
        add_metric(data['name'], data['type'], data['description'],
                   data.get('buckets', DEFAULT_BUCKETS), metric_pool)
    elif endpoint == 'report':
        report_metric(data['name'], data['data'], metric_pool)


def start_server():
    logger = _init_logger()

    start_http_server(1390)

    r = redis.Redis(unix_socket_path=UNIX_SOCKET_PATH)
    sub = r.pubsub(ignore_subscribe_messages=True)
    sub.subscribe(CHANNEL_NAME)

    metric_pool = {}
    for messege in sub.listen():  # Blocking, will run forever
        logger.debug(messege)
        try:
            messege_dict = json.loads(messege['data'])
            validate_schema(messege_dict)
            handle_messege(messege_dict, metric_pool)
        except Exception as e:
            logger.error(e)
            pass


def _init_logger():
    logging.basicConfig(
        filename='/metric_server.log',
        format=
        '%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
        datefmt='%y-%m-%d:%H:%M:%S',
        level=logging.DEBUG)
    logger = logging.getLogger(__name__)
    return logger


def start_redis_daemon():
    cmd = [
        '/redis-stable/src/redis-server', '--unixsocket', '/tmp/redis.sock',
        '--daemonize', 'yes'
    ]
    call(cmd)


def redis_daemon_exist():
    pids = psutil.pids()
    process_names = [psutil.Process(pid).name() for pid in pids]
    return 'redis-server' in process_names


if __name__ == '__main__':
    if not redis_daemon_exist():
        start_redis_daemon()

    # This snippet of code spin up a debug server
    # that sends the log to 1392. Don't forget to add
    # the debug line to container manager as well!
    if len(sys.argv) > 1 and sys.argv[-1] == 'DEBUG':

        def start_debug_server():
            from flask import Flask, send_file, jsonify
            app = Flask(__name__)

            @app.route('/')
            def show_log():
                return send_file('/metric_server.log')

            app.run(host='0.0.0.0', port=1392)

        from multiprocessing import Process
        debug_proc = Process(target=start_debug_server)
        debug_proc.start()

    start_server()
