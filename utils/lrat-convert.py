#!/usr/bin/env python3
import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('lrat', help='lrat file')
    parser.add_argument('output', help='output drat file')
    args = parser.parse_args()

    with open(args.lrat, 'r') as f:
        lines = f.readlines()

    with open(args.output, 'w') as f:
        for line in lines:
            # remove the first number and everything after the first 0
            line = line.split(' 0')[0].split(' ')[1:]
            line = ' '.join(line)
            f.write(line + ' 0\n')


if __name__ == '__main__':
    main()
