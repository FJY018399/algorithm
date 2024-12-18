#!/bin/bash
echo "Running pipeline simulation tests..."

# Compile the program
g++ -std=c++17 -Wall -Wextra main.cpp -o pipeline

# Function to get expected output
get_expected_output() {
    local test_file=$1
    local test_num=${test_file//[!0-9]/}

    if [ "$test_num" -le 4 ]; then
        sed -n "${test_num}p" expected_output.txt
    else
        grep "test${test_num}.txt:" expected_output_additional.txt | cut -d':' -f2 | tr -d ' '
    fi
}

# Run all test cases
failed_tests=0
total_tests=0

for test_file in test[1-7].txt; do
    if [ ! -f "$test_file" ]; then
        continue
    fi

    ((total_tests++))
    echo -e "\nRunning $test_file..."
    output=$(./pipeline < "$test_file")
    expected=$(get_expected_output "$test_file")

    echo "Output: $output"
    echo "Expected: $expected"

    if [ "$output" = "$expected" ]; then
        echo "✓ Test passed"
    else
        echo "✗ Test failed"
        ((failed_tests++))
    fi
done

echo -e "\nTest Summary:"
echo "Total tests: $total_tests"
echo "Failed tests: $failed_tests"
echo "Passed tests: $((total_tests - failed_tests))"
