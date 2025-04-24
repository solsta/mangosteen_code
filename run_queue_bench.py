import subprocess

# List of thread counts
thread_counts = [4, 8, 16, 24, 30]

# Number of runs per thread count
num_runs = 5

# Run the command with the given parameters
for threads in thread_counts:
    for run in range(1, num_runs + 1):
        output_file = f"out_{threads}_run{run}.out"
        command = f"./build/examples/queue/queue {threads} > {output_file}"
        print(f"Running: {command}")
        subprocess.run(command, shell=True)

        # Run cleanup after each experiment
        cleanup_command = "./cleanup.sh"
        print(f"Running cleanup: {cleanup_command}")
        subprocess.run(cleanup_command, shell=True)
