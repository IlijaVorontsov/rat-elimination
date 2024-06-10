#!/bin/bash

# Variables
out=out.txt
uri=instances.uri

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
    rat_stats=$(rat-elimination $file ${file%.cnf}.lrat ${file%.cnf}.lrup 2>&1)
    
    # Check the lrat-check exit code
    lrat-check $file ${file%.cnf}.lrup
    lrat_check_exit_code=$?
    
    # Append the results to the CSV file
    echo "$file, $lrat_check_exit_code, $rat_stats" >> $out
    rm $file ${file%.cnf}.lrat ${file%.cnf}.lrup
done
