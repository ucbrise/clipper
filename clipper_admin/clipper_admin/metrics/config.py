# This file contains the constants that will be shared across
# client and server
from . import __version__

CHANNEL_NAME = 'clipper'  # redis pub-sub channel name
API_VERSION = __version__  # Consistent with Clipper Version.
UNIX_SOCKET_PATH = '/tmp/redis.sock'
DEFAULT_BUCKETS = [
    5, 10, 20, 35, 50, 75, 100, 150, 200, 250, 300, 400, 500,
    float('inf')
]
