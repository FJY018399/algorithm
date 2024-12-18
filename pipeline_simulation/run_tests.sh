#!/bin/bash
echo "Running pipeline simulation tests..."

# Compile the program
g++ -std=c++17 -Wall -Wextra main.cpp -o pipeline

# Run each test case and compare with expected output
test_files=(test1.txt test2.txt test3.txt test4.txt)
expected_outputs=(13 7 15 14)

for i in "${!test_files[@]}"; do
    echo -e "\nTest Case $((i+1)):"
    output=$(./pipeline < "${test_files[$i]}")
    echo "Output: $output (Expected: ${expected_outputs[$i]})"
    if [ "$output" -eq "${expected_outputs[$i]}" ]; then
        echo "✓ Test passed"
    else
        echo "✗ Test failed"
    fi
done
