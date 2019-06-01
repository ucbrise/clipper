import asyncio, json
from aiohttp import web

class BoatAPI:
    def __init__(self, boat, addr, port):
        self.boat = boat
        self.addr = addr
        self.port = port

    async def request_handler(self, request):
        if request.rel_url.path == '/':
            # For all methods to root
            return web.Response(text='Hello from Boat API')
        elif request.rel_url.path == '/status':
            if request.method == 'GET':
                response_json_string = json.dumps(await self.boat.get_status())
                return web.Response(content_type='application/json',  text=response_json_string)
        elif request.rel_url.path == '/predict':
            if request.method == 'POST' and request.body_exists and request.can_read_body:
                request_string = await request.text()
                await self.boat.post_predict(request_string)
                return web.Response()
        elif request.rel_url.path == '/control':
            if request.method == 'POST' and request.body_exists and request.can_read_body:
                request_string = await request.text()
                try:
                    await self.boat.post_control(request_string)
                except:
                    return web.Response(status=400)
                return web.Response()
        return web.Response(status=400)

    async def run(self, loop):
        server = web.Server(self.request_handler)
        await loop.create_server(server, self.addr, self.port)
