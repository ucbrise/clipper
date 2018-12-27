from random import shuffle
import sys
import argparse

parser = argparse.ArgumentParser(description="Colorize stdin; (optionally) add a tag.")
parser.add_argument("--tag", type=str, help="Optional tag")

args = parser.parse_args()
tag = "[{}]".format(args.tag) if args.tag else ""

ALL_COLORS = [
    # "\u001b[30m", # Black
    "\u001b[31m",  # Red
    "\u001b[32m",  # Green
    "\u001b[33m",  # Yellow
    "\u001b[34m",  # Blue
    "\u001b[35m",  # Magenta
    "\u001b[36m",  # Cyan
]

shuffle(ALL_COLORS)

COLOR = ALL_COLORS[0]

for line in sys.stdin:
    print(
        "{begin_color} {tag} {line} {end_color}".format(
            begin_color=COLOR,
            tag=tag,
            line=line.strip(),
            end_color="\u001b[0m",  # Reset
        )
    )
