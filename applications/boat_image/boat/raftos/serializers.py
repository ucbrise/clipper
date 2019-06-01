import msgpack

try:
    import ujson as json
except ImportError:
    import json


class JSONSerializer:
    @staticmethod
    def pack(data):
        return json.dumps(data).encode()

    @staticmethod
    def unpack(data):
        decoded = data.decode() if isinstance(data, bytes) else data
        return json.loads(decoded)


class MessagePackSerializer:
    @staticmethod
    def pack(data):
        return msgpack.packb(data, use_bin_type=True)

    @staticmethod
    def unpack(data):
        return msgpack.unpackb(data, use_list=True, encoding='utf-8')
