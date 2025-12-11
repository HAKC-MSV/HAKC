#!/bin/bash

#SBATCH -o logs/%A/ecs.out-%a
#SBATCH --mail-type=END
#SBATCH --mail-user=derrick.mckee@ll.mit.edu

# Let this job run until completion
#SBATCH --time=0

# CPU_CORES
#SBATCH -c 10

# Create Policies with 1 to 10000 symbols per compartment, increasing by 10 each time
#SBATCH --array 1-2000:10

# NB: These match the Slurm options above
CPU_CORES=10

NUM_SYMBOLS=$(($SLURM_ARRAY_TASK_ID))

source python/venv/bin/activate
export PYTHONPATH=HAKC/llvm-project/llvm/utils/hakc:$PYTHONPATH

FLEXC_DB_DIR=ecs/hakc-db-flexc-ecs-$NUM_SYMBOLS
WORKING_DB_DIR=/state/partition1/user/$USER/flexc-compartmentalizations/$FLEXC_DB_DIR
FINAL_DB_DIR=compartmentalizations/$FLEXC_DB_DIR

mkdir -p $(dirname $WORKING_DB_DIR)

python3 HAKC/python/analysis/flexc.py --db-dir cmake-build-hakc/linux/x86/hakc-db-flexc --output-db-dir $WORKING_DB_DIR --core-count $CPU_CORES ecs --symbols-per-compartment $NUM_SYMBOLS

mkdir -p $(dirname $FLEXC_DB_DIR)
mv $WORKING_DB_DIR $FINAL_DB_DIR
