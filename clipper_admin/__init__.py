import sys
if sys.version_info >= (3, 0):
    sys.stdout.write(
        "Sorry, clipper_admin requires Python 2.x, but you are running Python 3.x\n"
    )
    sys.exit(1)

from clipper_manager import Clipper
