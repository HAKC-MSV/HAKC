#!/bin/bash

is_job_running() {
    local job_id=$1
    squeue -j "$job_id" > /dev/null 2>&1
    return $?
}

PATH_TO_SCRIPT="$1"

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
ORIG_DIR=$PWD
echo "Moving from $ORIG_DIR to $SCRIPT_DIR"
cd "$SCRIPT_DIR"
HAKC_ROOT=$(git rev-parse --show-toplevel)
echo "Found HAKC_ROOT $HAKC_ROOT"
cd "$ORIG_DIR"

for i in $(seq 2 20); do
  echo "Running env HAKC_ROOT=$HAKC_ROOT sbatch $PATH_TO_SCRIPT $i"
  JOB_ID=$(env HAKC_ROOT=$HAKC_ROOT sbatch $PATH_TO_SCRIPT $i | awk '{print $NF}')
  if [ -z "$JOB_ID" ]; then
      echo "Error: Failed to submit job or retrieve job ID"
      exit 1
  fi
  echo "Submitted job with ID: $JOB_ID"

  # Wait for the job to complete
  while is_job_running "$JOB_ID"; do
      echo "Job $JOB_ID is still running..."
      sleep 60
  done

done
