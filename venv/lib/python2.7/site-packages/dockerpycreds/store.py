import json
import os
import subprocess

import six

from . import constants
from . import errors


class Store(object):
    def __init__(self, program):
        """ Create a store object that acts as an interface to
            perform the basic operations for storing, retrieving
            and erasing credentials using `program`.
        """
        self.program = constants.PROGRAM_PREFIX + program

    def get(self, server):
        """ Retrieve credentials for `server`. If no credentials are found,
            a `StoreError` will be raised.
        """
        if not isinstance(server, six.binary_type):
            server = server.encode('utf-8')
        data = self._execute('get', server)
        return json.loads(data.decode('utf-8'))

    def store(self, server, username, secret):
        """ Store credentials for `server`. Raises a `StoreError` if an error
            occurs.
        """
        data_input = json.dumps({
            'ServerURL': server,
            'Username': username,
            'Secret': secret
        }).encode('utf-8')
        return self._execute('store', data_input)

    def erase(self, server):
        """ Erase credentials for `server`. Raises a `StoreError` if an error
            occurs.
        """
        if not isinstance(server, six.binary_type):
            server = server.encode('utf-8')
        self._execute('erase', server)

    def _execute(self, subcmd, data_input):
        output = None
        try:
            if six.PY3:
                output = subprocess.check_output(
                    [self.program, subcmd], input=data_input
                )
            else:
                process = subprocess.Popen(
                    [self.program, subcmd], stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE
                )
                output, err = process.communicate(data_input)
                if process.returncode != 0:
                    raise subprocess.CalledProcessError(
                        returncode=process.returncode, cmd='', output=output
                    )
        except subprocess.CalledProcessError as e:
            raise errors.process_store_error(e, self.program)
        except OSError as e:
            if e.errno == os.errno.ENOENT:
                raise errors.StoreError(
                    '{0} not installed or not available in PATH'.format(
                        self.program
                    )
                )
            else:
                raise errors.StoreError(
                    'Unexpected OS error "{0}", errno={1}'.format(
                        e.strerror, e.errno
                    )
                )
        return output
