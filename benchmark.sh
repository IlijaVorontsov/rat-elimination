#!/bin/bash

# Variables
out=out.csv
uri=instances.uri
libmimalloc=/usr/local/lib/libmimalloc-debug.dylib

# check if libmimalloc is available
if [ ! -f $libmimalloc ]; then
    echo "libmimalloc.so not found"
    echo "Follow the instructions at https://github.com/microsoft/mimalloc"
    exit 1
fi

# Check if dependencies are installed
for cmd in wget xz sbva_wrapped drat-trim lrat-check rat-elimination; do
   if ! command -v $cmd &> /dev/null; then
       echo "$cmd could not be found"
       exit 1
   fi
done

# Download and extract files
wget --content-disposition -i $uri
xz -d *.xz

# Create CSV header
echo "Instance, verified, dimacs, lrat, additions, reused, deletions, total, marking, distributing, finishing" > $out

# Loop through each .cnf file
for file in *.cnf; do
    # Remove any prefix up to and including the first dash
    mv $file ${file#*-}
    file=${file#*-}
    
    # Run sbva_wrapped, drat-trim, and remove .drat file
    sbva_wrapped cadical $file ${file%.cnf}.drat
    drat-trim $file ${file%.cnf}.drat -L ${file%.cnf}.lrat
    rm ${file%.cnf}.drat
    
    # Capture rat-elimination stats
    rat_stats=$(../bin/rat-elimination $file ${file%.cnf}.lrat ${file%.cnf}.lrup 2>&1)
    
    # Check the lrat-check exit code
    ../bin/lrat-check $file ${file%.cnf}.lrup
    lrat_check_exit_code=$?
    
    # Append the results to the CSV file
    echo "$file, $lrat_check_exit_code, $rat_stats" >> $out

    mi_rat_stats=$(env LD_PRELOAD=$libmimalloc ../bin/rat-elimination $file ${file%.cnf}.lrat ${file%.cnf}.lrup 2>&1)
    # Check the lrat-check exit code (mimalloc won't make a difference here)
    #lrat-check $file ${file%.cnf}.lrup
    #lrat_check_exit_code=$?

    echo "$file (mimalloc), $lrat_check_exit_code, $mi_rat_stats" >> $out
    rm $file.cnf $file.lrat $file.lrup
done
