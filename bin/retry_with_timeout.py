import argparse
import subprocess
import sys
import time


def timeout_to_float(s):
    assert s == "inf" or s[-1] in ["s", "m", "h"]
    if s == "inf":
        return float("inf")
    num, unit = float(s[:-1]), s[-1]

    if unit == "s":
        return float(num)
    elif unit == "m":
        return float(num * 60)
    else:
        return float(num * 60 * 60)


parser = argparse.ArgumentParser(
    description="Run a subprocess with timeout and retry.",
    usage="""
python retry_with_timeout.py --retry 2 --timeout 10m -- python flaky.py
""",
)
parser.add_argument(
    "--retry", type=int, default=0, help="number of retry, default to 0"
)
parser.add_argument(
    "--timeout", type=str, default="inf", help="timeout in the unit of {s, m, h}"
)

args, command_to_run = parser.parse_known_args()

if command_to_run[0] != "--":
    parser.print_help()
    sys.exit(1)
else:
    command_to_run = command_to_run[1:]

try:
    timeout = timeout_to_float(args.timeout)
except AssertionError:
    parser.print_help()
    sys.exit(1)


def run_once_with_timeout(command_to_run, timeout):
    proc = subprocess.Popen(
        command_to_run, stdout=sys.stdout, stderr=subprocess.STDOUT
    )
    start = time.time()
    while True:
        proc.poll()
        return_code = proc.returncode
        if return_code is not None:
            return return_code
        else:
            duration = time.time() - start
            if duration > timeout:
                proc.kill()
                return 1



for try_num in range(args.retry + 1):
    print(
        "Starting Trial {try_num} with timeout {timeout} seconds".format(
            try_num=try_num, timeout=timeout
        )
    )
    return_code = run_once_with_timeout(command_to_run, timeout)
    if return_code == 0:
        print("Success!")
        sys.exit(0)

print("All retry failed.")
sys.exit(1)
