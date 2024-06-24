# rat-elimination

C implementation of [Rebola-Pardo and Weissenbacher "RAT Elimination"](https://publik.tuwien.ac.at/files/publik_293387.pdf)

## Building

Run `make` to build the binary (`./bin/rat-elimination`).
Run `make bench` to build binary that prints benchmark information for the csv at the end. 
Run `make check` to check the binary on the test instances and verify the output with lrat-check.

## Usage

Run `./bin/rat-elimination [-v|-p] CNF_FILE LRAT_FILE  [OUTPUT_FILE]` If OUTPUT_FILE is not specified the result is written to `stdout`. 
The flags -v for verbose output (number of remaining rat-clauses to stderr), -p adds pivots to the output.



## Benchmarking

Requires `make bench`. Run `./benchmark.sh` together with the instances to download and run in the `instances.uri` file from the [GDB Benchmark Database](https://benchmark-database.de/).
