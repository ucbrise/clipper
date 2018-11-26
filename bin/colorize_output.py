from colorama import Fore
from random import shuffle
import sys
import argparse

parser = argparse.ArgumentParser(description='Colorize stdin; (optionally) add a tag.')
parser.add_argument('--tag', type=str,
                    help='Optional tag')

args = parser.parse_args()
tag = '[{}]'.format(args.tag) if args.tag else None

ALL_COLORS = [
  Fore.BLACK,
  Fore.RED,
  Fore.GREEN,
  Fore.YELLOW,
  Fore.BLUE,
  Fore.MAGENTA,
  Fore.CYAN
]

shuffle(ALL_COLORS)

COLOR = ALL_COLORS[0]

print(COLOR)

for line in sys.stdin:
    if tag:
        print(tag, end=' ')
    print(line.strip()) # strip the ending \n

print(Fore.RESET)