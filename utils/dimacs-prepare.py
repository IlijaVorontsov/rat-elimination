#!/usr/bin/env python3
# \file dimacs-prepare.py
# \brief This script checks if dimacs format is correct
#        and orders literals by absolute value
# \author Ilija Vorontsov
# \version 1.0
# \date 2023

import argparse


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('input', help='input dimacs file')
    parser.add_argument('output', help='output dimacs file')
    args = parser.parse_args()

    with open(args.input, 'r') as f:
        lines = f.readlines()

    lines = [line for line in lines if not line.startswith('c')]
    assert lines[0].startswith('p cnf')
    max_var = int(lines[0].split(' ')[2])
    num_clauses = int(lines[0].split(' ')[3])
    assert len(lines) == num_clauses + 1
    lines = lines[1:]

    with open(args.output, 'w') as f:
        for line in lines:
            assert line[-2] == '0'
            literals = [int(x) for x in line.split(' ')[:-1]]
            literals = sorted(literals, key=lambda x: abs(x))
            assert abs(literals[-1]) <= max_var
            for literal in literals:
                f.write(str(literal) + ' ')
            f.write('0\n')


if __name__ == '__main__':
    main()
