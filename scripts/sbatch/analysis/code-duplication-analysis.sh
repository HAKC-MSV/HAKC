#!/bin/bash

#SBATCH -o logs/%A/code-duplication-greedy-%a.out
#SBATCH --mail-type=END
#SBATCH --mail-user=derrick.mckee@ll.mit.edu

# Let this job run until completion
#SBATCH --time=0

# CPU_CORES
#SBATCH -c 10

# Create Policies with 20 to 2000 compartments
#SBATCH --array 10-2000:10

ALGO_NAME=greedy
NUM_COMPARTMENTS=$(($SLURM_ARRAY_TASK_ID))
DUP_SYMBOL_COUNT=$1

source /etc/profile
module purge

modules_to_load=("conda/Python-ML-2025b-pytorch" "proxy-mitll")
echo "Loading modules"
for module_to_load in "${modules_to_load[@]}"; do
	echo "Loading $module_to_load"
	module load $module_to_load
done

SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"
ORIG_DIR=$PWD
cd "$SCRIPT_DIR"
HAKC_ROOT=$(git rev-parse --show-toplevel)
cd "$ORIG_DIR"

export PYTHONPATH=$HAKC_ROOT/llvm-project/llvm/utils/hakc:$PYTHONPATH

FLEXC_DB_DIR=$ALGO_NAME/hakc-db-$DUP_SYMBOL_COUNT-$NUM_COMPARTMENTS
INITIAL_DB_DIR=$ORIG_DIR/compartmentalizations/code-duplication/$FLEXC_DB_DIR

FINAL_DB_PARENT_DIR=/state/partition1/user/$USER/flexc-compartmentalizations
FINAL_DB_DIR=$FINAL_DB_PARENT_DIR/$FLEXC_DB_DIR

ANALYSIS_DIR=$ORIG_DIR/flexc-analysis/code-duplication/$ALGO_NAME/duplication-count-$DUP_SYMBOL_COUNT
ANALYSIS_OUTPUT=$ANALYSIS_DIR/flexc-analysis-$DUP_SYMBOL_COUNT-$NUM_COMPARTMENTS.yml

echo "Making directory $ANALYSIS_DIR"
mkdir -p $ANALYSIS_DIR
echo "Making directory $(dirname $FINAL_DB_DIR)"
mkdir -p $(dirname $FINAL_DB_DIR)

echo "Copying database from $INITIAL_DB_DIR to $(dirname $FINAL_DB_DIR)"
cp -r $INITIAL_DB_DIR $(dirname $FINAL_DB_DIR)

echo "Executing python3 $HAKC_ROOT/python/analysis/flexc-analysis.py --db-dir $FINAL_DB_DIR --analysis-output $ANALYSIS_OUTPUT --vulnerability-analysis --input $HAKC_ROOT/flexc/compartment-evaluation/linux/escalation-structs.yml $HAKC_ROOT/flexc/compartment-evaluation/linux/vulnerable-symbols.yml"

python3 $HAKC_ROOT/python/analysis/flexc-analysis.py --db-dir $FINAL_DB_DIR --analysis-output $ANALYSIS_OUTPUT --vulnerability-analysis --input $HAKC_ROOT/flexc/compartment-evaluation/linux/escalation-structs.yml $HAKC_ROOT/flexc/compartment-evaluation/linux/vulnerable-symbols.yml

exit_code=$?
rm -rf $FINAL_DB_DIR

exit $exit_code
