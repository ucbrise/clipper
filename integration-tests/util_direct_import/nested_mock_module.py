"""
This file is used to test the functionality of exporting modules not found
in conda or pip. This module is imported by a separate module, `modck_module`.
A successful Python function deployment that uses `mock_module` must
recursively import this module as well.
"""

COEFFICIENT = 3
