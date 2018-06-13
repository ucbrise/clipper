from __future__ import absolute_import
from .client import add_metric, report_metric
from . import server

if not server.redis_daemon_exist():
    server.start_redis_daemon()
