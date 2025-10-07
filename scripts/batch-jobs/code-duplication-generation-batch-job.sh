#!/bin/bash

is_job_running() {
    local job_id=$1
    squeue -j "$job_id" > /dev/null 2>&1
    return $?
}

PATH_TO_SCRIPT="$1"

for i in $(seq 2 20); do
  echo "Running sbatch $PATH_TO_SCRIPT $i"
  JOB_ID=$(sbatch $PATH_TO_SCRIPT $i | awk '{print $NF}')
  echo "Submitted job with ID: $JOB_ID"

  # Wait for the job to complete
  while is_job_running "$JOB_ID"; do
      echo "Job $JOB_ID is still running..."
      sleep 60
  done

done
