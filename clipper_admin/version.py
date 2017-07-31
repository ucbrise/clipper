import sys
import os

with open(os.path.abspath("../VERSION.txt"), 'r') as f:
    version = f.read().strip()
