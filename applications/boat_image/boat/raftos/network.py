import asyncio

from .log import logger
from .conf import config


class UDPProtocol(asyncio.DatagramProtocol):
    def __init__(self, queue, request_handler, loop, serializer=None, cryptor=None):
        self.queue = queue
        self.serializer = serializer or config.serializer
        self.cryptor = cryptor or config.cryptor
        self.request_handler = request_handler
        self.loop = loop

    def __call__(self):
        return self

    async def start(self):
        while not self.transport.is_closing():
            request = await self.queue.get()
            data = self.cryptor.encrypt(self.serializer.pack(request['data']))
            self.transport.sendto(data, request['destination'])

    def connection_made(self, transport):
        self.transport = transport
        asyncio.ensure_future(self.start(), loop=self.loop)

    def datagram_received(self, data, sender):
        data = self.serializer.unpack(self.cryptor.decrypt(data))
        data.update({
            'sender': sender
        })
        self.request_handler(data)

    def error_received(self, exc):
        logger.error('Error received {}'.format(exc))

    def connection_lost(self, exc):
        logger.error('Connection lost {}'.format(exc))
