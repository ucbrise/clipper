class ClipperException(Exception):
    """A generic exception indicating that Clipper encountered a problem."""

    def __init__(self, msg, *args):
        self.msg = msg
        super(Exception, self).__init__(msg, *args)


class UnconnectedException(ClipperException):
    """A ``ClipperConnection`` instance must be connected to a Clipper cluster to issue this command."""

    def __init__(self, *args):
        message = (
            "No connection to Clipper cluster. Call ClipperConnection.connect to "
            "connect to an existing cluster or ClipperConnnection.start_clipper to "
            "create a new one")
        self.message = message
        super(UnconnectedException, self).__init__(message, *args)
