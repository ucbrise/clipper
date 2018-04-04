from ..version import __version__
from .client import add_metric, report_metric
import server

if not server.redis_daemon_exist():
    server.start_redis_daemon()
