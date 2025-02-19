#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test

if [[ -n "$1" ]]; then
    # 1. Emit LLVM
    $HAKC_CLANG -g -S -emit-llvm -o $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.orig.ll -c $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.c
    # 2. Generate DAG db using pass 
    # 2.a use sed to get correct paths 
    # need to replace pwd and pass mode (same as the test runner is doing) to generate valild yaml config file 
    cat $HAKC_TEST_ROOT/configs/hakc_comp_config.yml.in | sed 's,@PWD@,'$HAKC_TEST_ROOT/hakc_test$1',g' | sed 's,@PASS_MODE@,RunDataAccessGraphAnalysis,g' > $HAKC_TEST_ROOT/hakc_test$1/hakc_dag_config.yml
    $HAKC_OPT -passes=hakc --HAKC_CONFIG=$HAKC_TEST_ROOT/hakc_test$1/hakc_dag_config.yml $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.orig.ll -o $HAKC_TEST_ROOT/hakc_test$1/hakc_test$1.tmp.ll
    python3 -m venv $(git rev-parse --show-toplevel)/python/venv && source $(git rev-parse --show-toplevel)/python/venv/bin/activate
    # 3. Generate yaml backing store using hakc-dag.py
    python3 $HAKC_ROOT/python/analysis/hakc-dag.py --dump-dag $HAKC_TEST_ROOT/configs/backing_stores/backing_$1.yml --create-dag --dag-files-root $HAKC_TEST_ROOT/hakc_test$1/dag_analysis/_HAKC_SOURCE_PATH_ --db-dir $HAKC_TEST_ROOT/hakc_test$1/hakc-db 
fi


# $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 15 Transforms/Compartmentalization/hakc_test0/hakc_test0.c
# # make dag analysis yaml  
# $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/install/bin/opt -passes=hakc --HAKC_CONFIG=$(dirname /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c)/$(basename /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c .c)_config.yml_DAG /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.dag.ll -o /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.ll
# # make dag / comp db 
# python3 ~/hakc/HAKC_CURR/python/analysis/hakc-dag.py --dump-dag /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/backing.json --create-dag --dag-files-root /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/dag_analysis/_HAKC_SOURCE_PATH_ --db-dir /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc-db
# python3 $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/python/analysis/hakc-policy-process.py --config $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/HAKC-pass/configs/hakc-policy-config.yaml --log-level DEBUG & 
# $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/install/bin/opt -passes=hakc --HAKC_CONFIG=$(dirname /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c)/$(basename /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c .c)_config.yml_COMP /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.dag.ll -o /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.ll
