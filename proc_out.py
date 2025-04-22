import os

import re
import argparse
import statistics

def get_files_for_thread(thread_number):

    # Directory to scan

    directory = "test_out"

    

    # Regex pattern to match filenames like out_<thread>_run<run>.out

    pattern = re.compile(rf"out_{thread_number}_run\d+\.out")



    # Filter files in 'test_out' that match the specified thread number

    matching_files = [

        filename for filename in os.listdir(directory)

        if pattern.match(filename)

    ]



    # Return full paths (optional) or just filenames

    return [os.path.join(directory, filename) for filename in matching_files]

def extract_throughput_lines(thread_number):

    files = get_files_for_thread(thread_number)

    throughput_lines = []
    print("GOT files list")
    for file in files:
        print(file)
    throughput_values = []

    for file_path in files:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
        #with open(file_path, 'r') as f:

            for line in f:

                if "Throughput" in line:
                    #print(line)
                    #throughput_lines.append(line.strip())
                    match = re.search(r"Throughput:\s+([\d.]+)\s+ops/sec", line)
                    throughput_value = float(match.group(1))
                    throughput_values.append(throughput_value)

                    print(f"Extracted value: {throughput_value}")

                    break  # Stop after finding the first matching line

    median_value = statistics.median(throughput_values)
    print(f"Median throughput: {median_value}")

# Example usage:
if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="Extract throughput lines for a specific thread count.")

    parser.add_argument("threads", type=int, help="Number of threads (e.g., 4, 8, 16, ...)")

    args = parser.parse_args()



    extract_throughput_lines(args.threads)
