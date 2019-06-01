import asyncio, copy

class AtomicQueue:
    def __init__(self):
        self.queue = list()
        self.cond = asyncio.Condition()

    async def wait_and_dequeue(self):
        await self.cond.acquire()
        while not self.queue:
            await self.cond.wait()
        head, *(self.queue) = self.queue
        self.cond.release()
        return head

    async def wait_and_dequeue_all(self):
        await self.cond.acquire()
        while not self.queue:
            await self.cond.wait()
        all = copy.deepcopy(self.queue)
        self.queue.clear()
        self.cond.release()
        return all

    async def enqueue_and_notify(self, obj):
        await self.cond.acquire()
        self.queue.append(obj)
        self.cond.notify()
        self.cond.release()
    
    async def is_empty(self):
        await self.cond.acquire()
        val = not self.queue
        self.cond.release()
        return val

class AtomicDict:
    def __init__(self):
        self.data = dict()
        self.lock = asyncio.Lock()
        
    async def has(self, key):
        async with self.lock:
            return (key in self.data)
    
    async def set(self, key, value):
        async with self.lock:
            self.data[key] = value

