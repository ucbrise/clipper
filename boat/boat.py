
import asyncio
import os
import logging
import aiohttp
import json
import multiprocessing
from datetime import datetime

import raftos
from rest_api import BoatAPI
from atomic import AtomicQueue, AtomicDict


class Boat:
    def __init__(self, num_nodes, node_id, start_time): 
        # Add member
        self.num_nodes = num_nodes
        self.node_id = node_id
        self.start_time = start_time

        # RaftOS can perform callbacks for the same request
        # So we add a time -> request dict for quick replicate lookup
        self.registry = AtomicDict()

        # Submitted requests via Rest API
        # self.api_queue -> [{'time': str, 'request': str}]
        self.api_queue = AtomicQueue()
        # Requests to send to Clipper
        # self.clipper_queue -> [str]
        self.clipper_queue = AtomicQueue()

        # Retrieve asyncio loop
        self.loop = asyncio.get_event_loop()

        # Configure raftos

        self.raft_node = '127.0.0.1:{}'.format(8000+self.node_id)
        self.raft_cluster = ['127.0.0.1:{}'.format(8000+i) for i in range(num_nodes) if i != self.node_id]

        self.timestamp = start_time.strftime('%Y-%m-%d-%H-%M-%S')
        
        self.log_dir = './logs/{}'.format(self.timestamp)
        os.makedirs('./logs', exist_ok=True)
        os.makedirs(self.log_dir, exist_ok=True)

        raftos.configure({
            'log_path': self.log_dir,
            'serializer': raftos.serializers.JSONSerializer,
            'on_receive_append_entries_callback': self.on_receive_append_entries_callback
        })
    
        # Configure REST API
        self.rest_api = BoatAPI(self, '127.0.0.1', 8080+self.node_id)


    # Everything about async goes here
    def run(self):
        # Schedule Rest API
        self.loop.create_task(self.rest_api.run(self.loop))
        
        # Schedule main loop
        self.loop.create_task(self.main_loop())

        # Schedule clipper feeding
        self.loop.create_task(self.clipper_feeder())

        # Blocking call to run the asyncio loop
        logging.info('Boat {}: Running. API listening to {}:{}'.format(
            self.node_id, self.rest_api.addr, self.rest_api.port))
        self.loop.run_forever()

    # As leader
    async def main_loop(self):
        '''Main loop'''
        await raftos.register(self.raft_node, cluster=self.raft_cluster)
        data_list = raftos.ReplicatedList(name='requests')

        while True:
            # Check whether there is something in the request queue
            rs = await self.api_queue.wait_and_dequeue_all()

            # We can also check if raftos.get_leader() == node_id
            await raftos.wait_until_leader(self.raft_node)

            for r in rs:
                # Broadcast the request send to prediction
                try:
                    await data_list.append(r)
                    logging.info('Boat {}: Request from API appended to state: {}'.format(self.node_id, r))
                    await self.predict_handler(r)
                except:
                    logging.error('Boat {}: Failed to append to state: {}'.format(self.node_id, r))
        
        await raftos.wait_until_leader(self.raft_node)
    
    # As follower
    def on_receive_append_entries_callback(self, command):
        '''Callback function for updates from Raft

           Request sent to Clipper'''
        
        if ('requests' in command) and command['requests']:
            # Update from leader received
            logging.info('Boat {}: Request received from raft: {}'.format(self.node_id, command['requests'][-1]))
            # Send to Clipper
            self.loop.create_task(self.predict_handler(command['requests'][-1]))
        else:
            logging.error('Boat {}: Update from raft received with invalid format'.format(self.node_id))

    # Unified interface before requests being sent to prediction no matter it comes from because we need to check replication
    # r -> { 'time': str, 'request': str }
    async def predict_handler(self, r):
        # if type(r) == dict and 'time' in r and 'request' in r and type(r['time']) == type(r['request']) == str:
        if await self.registry.has(r['time']):
            logging.info('Boat {}: Replicated request {} abandoned.'.format(self.node_id, r))
            return

        await self.registry.set(r['time'], r['request'])
        await self.clipper_queue.enqueue_and_notify(r['request'])

    # Coroutine that gets from Clipper queue and send to Clipper
    async def clipper_feeder(self):
        while True:
            r = await self.clipper_queue.wait_and_dequeue()
            async with aiohttp.ClientSession() as session:
                async with session.post('http://127.0.0.1:{}/default/predict'.format(1337+self.node_id), 
                    headers={'Content-Type': 'application/json'}, data=r.encode()) as resp:
                    
                    logging.info('Boat {}: {} sent to clipper with status {} and response:\n\t{}'.format(
                        self.node_id, r, resp.status, await resp.text()))

    # API handler
    async def get_status(self):
        return { 'is_leader': raftos.get_leader() == self.raft_node }

    # API handler
    async def post_predict(self, request):
        '''Put the incoming request into the queue with timestamp'''
        data_map = {
            'time': str(datetime.now()),
            'request': request
        }
        await self.api_queue.enqueue_and_notify(data_map)        

    # API handler
    async def post_control(self, request):
        '''React to the control message'''
        req_dict = json.loads(request)
        if 'cmd' in req_dict:
            if req_dict['cmd'] == 'exit':
                logging.info('Boat {}: Exit command received. Killing myself.'.format(self.node_id))
                # Clean up storage to simulate a failure
                files = os.listdir(self.log_dir)
                for file in files:
                    if '_'.join(self.raft_node.split(':')) in file:
                        full_path = os.path.join(self.log_dir, file)
                        logging.info('Boat {}: Removing from disk {}'.format(self.node_id, full_path))
                        try:
                            os.remove(full_path)
                        except:
                            logging.info('Boat {}: Error occurred while removing {}'.format(self.node_id, full_path))
                os.kill(os.getpid(), 9)
        raise Exception()
        