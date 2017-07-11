import os
import sys
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath(cur_dir))

import nested_mock_module as nmm
"""
A local module that is used by a predict function. This module is used to
test the functionality of exporting modules directly (by making them
accessible to one's path).
"""

COEFFICIENT = 2 * nmm.COEFFICIENT
