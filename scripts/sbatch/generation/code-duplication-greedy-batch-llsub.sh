#!/bin/bash

#SBATCH -o logs/%A/code-duplication-greedy-%a.out
#SBATCH --mail-type=END
#SBATCH --mail-user=derrick.mckee@ll.mit.edu

# Let this job run until completion
#SBATCH --time=0

# CPU_CORES
#SBATCH -c 10

# Create Policies with 10 to 2000 compartments increasing by 10
#SBATCH -a 10-2000:10

# NB: These match the Slurm options above
CPU_CORES=10

NUM_COMPARTMENTS=$(($SLURM_ARRAY_TASK_ID))

CURRENT_CODE_DUP_LEVEL=$1

source /etc/profile
module purge

modules_to_load=("conda/Python-ML-2025b-pytorch" "proxy-mitll")
echo "Loading modules"
for module_to_load in "${modules_to_load[@]}"; do
	echo "Loading $module_to_load"
	module load $module_to_load
done

export PYTHONPATH=$HAKC_ROOT/llvm-project/llvm/utils/hakc:$PYTHONPATH

ALGORITHM=greedy
ALGORITHM_OPTS="--max-compartments $NUM_COMPARTMENTS"
BASE_DB_NAME=hakc-db-$CURRENT_CODE_DUP_LEVEL
BASE_DB_DIR=$PWD/compartmentalizations/code-duplication/base-compartmentalizations/$BASE_DB_NAME
FLEXC_DB_DIR=code-duplication/duplication-count-$CURRENT_CODE_DUP_LEVEL/$ALGORITHM/$BASE_DB_NAME-$NUM_COMPARTMENTS
WORKING_DB_DIR=/state/partition1/user/$USER/compartmentalizations/$FLEXC_DB_DIR
FINAL_DB_DIR=compartmentalizations/$FLEXC_DB_DIR

mkdir -p $(dirname $WORKING_DB_DIR)

echo "Running python3 $HAKC_ROOT/python/analysis/flexc.py --db-dir $BASE_DB_DIR --output-db-dir $WORKING_DB_DIR --core-count $CPU_CORES $ALGORITHM $ALGORITHM_OPTS"

python3 $HAKC_ROOT/python/analysis/flexc.py --db-dir $BASE_DB_DIR --output-db-dir $WORKING_DB_DIR --core-count $CPU_CORES $ALGORITHM $ALGORITHM_OPTS

exit_code=$?

if [ $exit_code -eq 0 ]; then
  echo "Moving $WORKING_DB_DIR to $FINAL_DB_DIR"
  mkdir -p $(dirname $FINAL_DB_DIR)
  mv $WORKING_DB_DIR $FINAL_DB_DIR
fi

exit $exit_code
