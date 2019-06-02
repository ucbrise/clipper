import asyncio
import functools

from .state import State


def atomic_method(func):

    @functools.wraps(func)
    async def wrapped(self, *args, **kwargs):
        with await self.lock:
            result = await func(self, *args, **kwargs)

        return result
    return wrapped


class Replicated:
    """
    Replication class makes sure data changes are all applied to State Machine
    You can create your own Storage by subclassing it
    """

    DEFAULT_VALUE = None

    def __init__(self, name, default='REPLICATED_DEFAULT'):
        self.lock = asyncio.Lock()
        self.name = name

        # For subclasses like ReplicatedDict
        if default == 'REPLICATED_DEFAULT':
            self.value = self.DEFAULT_VALUE
        else:
            self.value = default

        self.in_memory = False

    async def get(self):
        # If we didn't set a value in this life cycle try to get it from State Machine
        if not self.in_memory:
            try:
                self.value = await State.get_value(self.name)
            except KeyError:
                pass

        return self.value

    async def set(self, value):
        await State.set_value(self.name, value)
        self.value = value
        self.in_memory = True


class ReplicatedContainer(Replicated):
    async def __getitem__(self, key):
        return (await self.get()).__getitem__(key)

    async def length(self):
        data = await self.get()
        return len(data)


class ReplicatedDict(ReplicatedContainer):
    """Replication class with dict-like methods"""

    DEFAULT_VALUE = {}

    @atomic_method
    async def update(self, kwargs):
        data = await self.get()
        data.update(kwargs)
        await self.set(data)

    async def keys(self):
        data = await self.get()
        return data.keys()

    async def values(self):
        data = await self.get()
        return data.values()

    async def items(self):
        data = await self.get()
        return data.items()

    @atomic_method
    async def pop(self, key, default):
        data = await self.get()
        item = data.pop(key, default)
        await self.set(data)
        return item

    @atomic_method
    async def delete(self, key):
        data = await self.get()
        del data[key]
        await self.set(data)


class ReplicatedList(ReplicatedContainer):
    """Replication class with list-like methods"""

    DEFAULT_VALUE = []

    @atomic_method
    async def append(self, kwargs):
        data = await self.get()
        data.append(kwargs)
        await self.set(data)

    @atomic_method
    async def extend(self, lst):
        data = await self.get()
        data.extend(lst)
        await self.set(data)
