class ClipperException(Exception):
    pass


class UnconnectedException(Exception):
    def __init__(self, *args):
        message = ("No connection to Clipper cluster. Call ClipperConnection.connect to "
                   "connect to an existing cluster or ClipperConnnection.start_clipper to "
                   "create a new one")
        self.message = message
        super(UnconnectedException, self).__init__(message, *args)
