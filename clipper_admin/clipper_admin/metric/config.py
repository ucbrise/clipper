# This file contains the constants that will be shared across
# client and server

CHANNEL_NAME = 'clipper'  # redis pub-sub channel name
API_VERSION = "0.3.0"  # Consistent with Clipper Version.
UNIX_SOCKET_PATH = '/tmp/redis.sock'
DEFAULT_BUCKETS = [
    5, 10, 20, 35, 50, 75, 100, 150, 200, 250, 300, 400, 500,
    float('inf')
]
