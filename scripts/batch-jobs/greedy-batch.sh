#!/bin/bash

#SBATCH -o logs/greedy.out-%A-%a

# CPU_CORES
#SBATCH -c 10

# Create Policies with 20 to 2000 compartments
#SBATCH -a 2-200

# NB: These match the Slurm options above
CPU_CORES=10

NUM_COMPARTMENTS=$(($SLURM_ARRAY_TASK_ID * 10))

source venv/bin/activate
export PYTHONPATH=

python3 HAKC/python/analysis/flexc.py --db-dir HAKC/cmake-build-hakc/linux/x86/hakc-db-flexc --output-db-dir compartmentalizations/hakc-db-flexc-greedy-$NUM_COMPARTMENTS --core-count $CPU_CORES greedy --max-compartments $NUM_COMPARTMENTS
