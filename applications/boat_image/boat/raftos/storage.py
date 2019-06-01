import os

from .conf import config


class FileDict:
    """Persistent dict-like storage on a disk accessible by obj['item_name']"""

    def __init__(self, filename, serializer=None):
        self.filename = filename.replace(':', '_')
        os.makedirs(os.path.dirname(self.filename), exist_ok=True)

        self.cache = {}
        self.serializer = serializer or config.serializer

    def update(self, kwargs):
        for key, value in kwargs.items():
            self[key] = value

    def exists(self, name):
        try:
            self[name]
            return True

        except KeyError:
            return False

    def __getitem__(self, name):
        if name not in self.cache:
            try:
                content = self._get_file_content()
                if name not in content:
                    raise KeyError

            except FileNotFoundError:
                open(self.filename, 'wb').close()
                raise KeyError

            else:
                self.cache = content

        return self.cache[name]

    def __setitem__(self, name, value):
        try:
            content = self._get_file_content()
        except FileNotFoundError:
            content = {}

        content.update({name: value})
        with open(self.filename, 'wb') as f:
            f.write(self.serializer.pack(content))

        self.cache = content

    def _get_file_content(self):
        with open(self.filename, 'rb') as f:
            content = f.read()
            if not content:
                return {}

        return self.serializer.unpack(content)


class Log:
    """Persistent Raft Log on a disk
    Log entries:
        {term: <term>, command: <command>}
        {term: <term>, command: <command>}
        ...
        {term: <term>, command: <command>}

    Entry index is a corresponding line number
    """

    UPDATE_CACHE_EVERY = 5

    def __init__(self, node_id, serializer=None):
        self.filename = os.path.join(config.log_path, '{}.log'.format(node_id.replace(':', '_')))
        os.makedirs(os.path.dirname(self.filename), exist_ok=True)
        open(self.filename, 'a').close()

        self.serializer = serializer or config.serializer
        self.cache = self.read()

        # All States

        """Volatile state on all servers: index of highest log entry known to be committed
        (initialized to 0, increases monotonically)"""
        self.commit_index = 0

        """Volatile state on all servers: index of highest log entry applied to state machine
        (initialized to 0, increases monotonically)"""
        self.last_applied = 0

        # Leaders

        """Volatile state on Leaders: for each server, index of the next log entry to send to that server
        (initialized to leader last log index + 1)
            {<follower>:  index, ...}
        """
        self.next_index = None

        """Volatile state on Leaders: for each server, index of highest log entry known to be replicated on server
        (initialized to 0, increases monotonically)
            {<follower>:  index, ...}
        """
        self.match_index = None

    def __getitem__(self, index):
        return self.cache[index - 1]

    def __bool__(self):
        return bool(self.cache)

    def __len__(self):
        return len(self.cache)

    def write(self, term, command):
        with open(self.filename, 'ab') as f:
            entry = {
                'term': term,
                'command': command
            }
            f.write(self.serializer.pack(entry) + '\n'.encode())

        self.cache.append(entry)
        if not len(self) % self.UPDATE_CACHE_EVERY:
            self.cache = self.read()

        return entry

    def read(self):
        with open(self.filename, 'rb') as f:
            return [self.serializer.unpack(entry) for entry in f.readlines()]

    def erase_from(self, index):
        updated = self.cache[:index - 1]
        open(self.filename, 'wb').close()
        self.cache = []

        for entry in updated:
            self.write(entry['term'], entry['command'])

    @property
    def last_log_index(self):
        """Index of last log entry staring from _one_"""
        return len(self.cache)

    @property
    def last_log_term(self):
        if self.cache:
            return self.cache[-1]['term']

        return 0


class StateMachine(FileDict):
    """Raft Replicated State Machine — dict"""

    def __init__(self, node_id):
        filename = os.path.join(config.log_path, '{}.state_machine'.format(node_id))
        super().__init__(filename)

    def apply(self, command):
        """Apply command to State Machine"""

        self.update(command)


class FileStorage(FileDict):
    """Persistent storage

    — term — latest term server has seen (initialized to 0 on first boot, increases monotonically)
    — voted_for — candidate_id that received vote in current term (or None)
    """

    def __init__(self, node_id):
        filename = os.path.join(config.log_path, '{}.storage'.format(node_id))
        super().__init__(filename)

    @property
    def term(self):
        return self['term']

    @property
    def voted_for(self):
        return self['voted_for']
