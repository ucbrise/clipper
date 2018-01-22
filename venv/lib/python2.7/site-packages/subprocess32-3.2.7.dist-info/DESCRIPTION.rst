This is a backport of the subprocess standard library module from
Python 3.2 & 3.3 for use on Python 2.
It includes bugfixes and some new features.  On POSIX systems it is
guaranteed to be reliable when used in threaded applications.
It includes timeout support from Python 3.3 but otherwise matches
3.2's API.  It has not been tested on Windows.

