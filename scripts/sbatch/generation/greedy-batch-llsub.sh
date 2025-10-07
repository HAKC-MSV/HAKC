#!/bin/bash

#SBATCH -o logs/%A/greedy.out-%a
#SBATCH --mail-type=END
#SBATCH --mail-user=derrick.mckee@ll.mit.edu

# Let this job run until completion
#SBATCH --time=0

# CPU_CORES
#SBATCH -c 10

# Create Policies with 20 to 2000 compartments
#SBATCH -a 4280-4289

# NB: These match the Slurm options above
CPU_CORES=10

NUM_COMPARTMENTS=$(($SLURM_ARRAY_TASK_ID))

#NUM_COMPARTMENTS=20

source venv/bin/activate
export PYTHONPATH=HAKC/llvm-project/llvm/utils/hakc:$PYTHONPATH

FLEXC_DB_DIR=hakc-db-flexc-greedy-$NUM_COMPARTMENTS
WORKING_DB_DIR=/state/partition1/user/$USER/flexc-compartmentalizations/$FLEXC_DB_DIR
FINAL_DB_DIR=compartmentalizations/$FLEXC_DB_DIR

python3 HAKC/python/analysis/flexc.py --db-dir HAKC/cmake-build-hakc/linux/x86/hakc-db-flexc --output-db-dir $WORKING_DB_DIR --core-count $CPU_CORES greedy --max-compartments $NUM_COMPARTMENTS

mv $WORKING_DB_DIR $FINAL_DB_DIR
