import os
cur_dir = os.path.dirname(os.path.abspath(__file__))
with open(os.path.abspath(os.path.join(cur_dir, "VERSION.txt")), 'r') as f:
    __version__ = f.read().strip()

__registry__ = os.environ.get('CLIPPER_REGISTRY', 'clipper')
