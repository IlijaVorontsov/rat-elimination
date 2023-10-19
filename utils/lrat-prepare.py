#!/usr/bin/env python3
# checks if the literals are ordered by absolute value

import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='input lrat file')
    parser.add_argument('output', help='output lrat file')
    args = parser.parse_args()

    with open(args.input, 'r') as f:
        lines = f.readlines()

    # delete lines with 'd'
    lines = [line for line in lines if 'd' not in line]

    # order lines by indices
    lines = sorted(lines, key=lambda x: int(x.split(' ')[0]))

    with open(args.output, 'w') as f:
        for line in lines:
            index = line.split(' ')[0]
            literals = [int(x) for x in line.split(' 0')[0].split(' ')[1:]]
            literals = sorted(literals, key=lambda x: abs(x))
            rest = line.split(' 0')[1]

            f.write(index + (' ' if len(literals) != 0 else '') + ' '.join([str(x)
                    for x in literals]) + ' 0' + rest + ' 0\n')


if __name__ == '__main__':
    main()
