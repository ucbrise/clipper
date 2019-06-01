from .conf import configure, config
from .replicator import Replicated, ReplicatedDict, ReplicatedList
from .server import register, stop
from .state import State


__all__ = [
    'Replicated',
    'ReplicatedDict',
    'ReplicatedList',

    'config',
    'configure',
    'register',
    'stop',

    'get_leader',
    'wait_until_leader'
]


get_leader = State.get_leader
wait_until_leader = State.wait_until_leader
