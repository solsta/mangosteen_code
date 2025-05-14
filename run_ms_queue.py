import subprocess
import os
import argparse
import os
import re
import argparse
import statistics


# List of thread counts
#thread_counts = [4,8,16,24,30]
thread_counts = [1]
payload_sizes = [128]
#payload_sizes = [128,256,512,1024]
queue_types = ['C']
#thread_counts = [30]



def get_files_for_thread(thread_number, directory):
    pattern = re.compile(rf"out_{thread_number}_run\d+\.out")
    matching_files = [

        filename for filename in os.listdir(directory)

        if pattern.match(filename)

    ]

    return [os.path.join(directory, filename) for filename in matching_files]

def extract_throughput_lines(thread_number, directory):

    files = get_files_for_thread(thread_number, directory)

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


def run_benchmark(directory, num_runs, queue_type):
    cleanup_command = "./cleanup.sh"
    subprocess.run(cleanup_command, shell=True)
    for threads in thread_counts:
        for payload in payload_sizes:
            outdir = f"{directory}/{queue_type}/{payload}"
            os.makedirs(outdir, exist_ok=True)
            for run in range(0, num_runs):
                output_file = os.path.join(outdir, f"out_{threads}_run{run}.out")
                command = f"./build/examples/queue_benchmarks/queue_bench {queue_type} {threads} {payload}> {output_file}"
                print(f"Running: {command}")
                subprocess.run(command, shell=True)

                # Run cleanup after each experiment
                print(f"Running cleanup: {cleanup_command}")
                subprocess.run(cleanup_command, shell=True)


if __name__ == "__main__":

    # Parse command-line argument for output directory
    parser = argparse.ArgumentParser(description="Run experiments and store output in a specified directory.")
    parser.add_argument("outdir", type=str, help="Output directory to store result files")
    args = parser.parse_args()
    

    os.makedirs(args.outdir, exist_ok=True)
    num_runs = 5

    for queue_type in queue_types:
        outdir = f"{args.outdir}_{queue_type}"
        os.makedirs(outdir, exist_ok=True)
        run_benchmark(args.outdir, num_runs,queue_type)

    #for threads in thread_counts:
    #    extract_throughput_lines(threads, args.outdir)
