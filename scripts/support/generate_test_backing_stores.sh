#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test

if [[ -n "$1" ]]; then
    # 1. Emit LLVM
    $HAKC_CLANG -g -S -emit-llvm -o $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/hakc_test$1.orig.ll -c $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.c
    # 2. Generate DAG db using pass 
    # 2.a use sed to get correct paths 
    # need to replace pwd and pass mode (same as the test runner is doing) to generate valid yaml config file
    cat $HAKC_TEST_ROOT/configs/hakc_comp_config.yml.in | sed 's,@BUILD_PATH@,'$HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1',g' | sed 's,@PASS_MODE@,RunDataAccessGraphAnalysis,g' > $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/hakc_dag_config.yml
    $HAKC_OPT -passes=hakc --hakc-config=$HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/hakc_dag_config.yml $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/hakc_test$1.orig.ll -o $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.tmp.ll
    python3 -m venv $(git rev-parse --show-toplevel)/python/venv && source $(git rev-parse --show-toplevel)/python/venv/bin/activate
    # 3. Generate yaml backing store using hakc-dag.py
    python3 $HAKC_ROOT/python/analysis/hakc-dag.py --dump-dag $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/backing_$1.yml --create-dag --dag-files-root $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/dag_analysis/_HAKC_SOURCE_PATH_ --db-dir $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test$1/hakc-db
fi
