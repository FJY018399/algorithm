#!/bin/bash
echo "Running pipeline simulation tests..."

while IFS= read -r line; do
    if [[ $line == \#* ]]; then
        echo -e "\n$line"
        continue
    fi
    echo "$line" | ./pipeline
done < test_cases.txt
