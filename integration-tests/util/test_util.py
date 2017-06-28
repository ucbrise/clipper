"""
A local module that is used by a predict function. This file is used to
test the functionality of exporting modules not found in conda or pip.
"""

import os
import sys
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(cur_dir))
import nested_test_util as ntu

COEFFICIENT = 2 * ntu.COEFFICIENT
