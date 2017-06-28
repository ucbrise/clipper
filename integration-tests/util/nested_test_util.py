"""
This file is used to test the functionality of exporting modules not found
in conda or pip. This module is imported by a separate module, `test_util`.
A successful Python function deployment that uses `test_util` must
recursively import this module as well.
"""

COEFFICIENT = 3
