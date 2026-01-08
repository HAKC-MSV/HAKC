#!/bin/bash

#SBATCH -o logs/%A/ecs-%a.out
#SBATCH --mail-type=END
#SBATCH --mail-user=derrick.mckee@ll.mit.edu

# Let this job run until completion
#SBATCH --time=0

# CPU_CORES
#SBATCH -c 10

# Create Policies with 20 to 2000 compartments
#SBATCH --array 1-2000:10

ALGO_NAME=ecs
NUM_COMPARTMENTS=$(($SLURM_ARRAY_TASK_ID))

source python/venv/bin/activate
export PYTHONPATH=HAKC/llvm-project/llvm/utils/hakc:$PYTHONPATH

FLEXC_DB_DIR=$ALGO_NAME/hakc-db-flexc-$ALGO_NAME-$NUM_COMPARTMENTS
INITIAL_DB_DIR=compartmentalizations/$FLEXC_DB_DIR

FINAL_DB_PARENT_DIR=/state/partition1/user/$USER/flexc-compartmentalizations
FINAL_DB_DIR=$FINAL_DB_PARENT_DIR/$FLEXC_DB_DIR

ANALYSIS_DIR=flexc-analysis/$ALGO_NAME
ANALYSIS_OUTPUT=$ANALYSIS_DIR/flexc-analysis-$NUM_COMPARTMENTS.yml

mkdir -p $ANALYSIS_DIR
mkdir -p $FINAL_DB_PARENT_DIR

cp -r $INITIAL_DB_DIR $FINAL_DB_DIR

python3 HAKC/python/analysis/flexc-analysis.py --db-dir $FINAL_DB_DIR --analysis-output $ANALYSIS_OUTPUT --input HAKC/analysis/linux/escalation-structs.yml HAKC/analysis/linux/vulnerable-symbols.yml 

rm -rf $FINAL_DB_DIR
