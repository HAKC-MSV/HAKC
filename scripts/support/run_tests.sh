#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test

if [[ -n "$1" ]]; then
    # llvm-lit -v TestCases/Posix/hakc-test$1.c
    if [[ -n "$2" ]]; then
        echo Transforms/Compartmentalization/hakc_test$1/hakc_test$1.c 
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout $2 Transforms/Compartmentalization/hakc_test$1/hakc_test$1.c
    else
        echo Transforms/Compartmentalization/hakc_test$1/hakc_test$1.c
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 45 Transforms/Compartmentalization/hakc_test$1/hakc_test$1.c
    fi

else
    $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 60 .
fi


# $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 15 Transforms/Compartmentalization/hakc_test0/hakc_test0.c
# # make dag analysis yaml  
# $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/install/bin/opt -passes=hakc --HAKC_CONFIG=$(dirname /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c)/$(basename /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c .c)_config.yml_DAG /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.dag.ll -o /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.ll
# # make dag / comp db 
# to save compartmentalization json 
# python3 ~/hakc/HAKC_CURR/python/analysis/hakc-dag.py --dump-dag /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/backing.json --create-dag --dag-files-root /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/dag_analysis/_HAKC_SOURCE_PATH_ --db-dir /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc-db
# to save compartmentalization yaml 
# clear && python3 ~/hakc/HAKC_CURR/python/analysis/hakc-dag.py --dump-dag /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/backing.yaml --create-dag --dag-files-root /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/dag_analysis/_HAKC_SOURCE_PATH_ --db-dir /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc-db

# clear && python3 /home/al32163/hakc/HAKC_CURR/python/analysis/hakc-policy-process.py --config /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_policy_config.yml --log-level DEBUG
# $(realpath /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/../../../../../)/install/bin/opt -passes=hakc --HAKC_CONFIG=$(dirname /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c)/$(basename /home/al32163/hakc/HAKC_CURR/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/hakc_test0.c .c)_config.yml_COMP /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.dag.ll -o /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/test/Transforms/Compartmentalization/hakc_test0/Output/hakc_test0.c.tmp.ll
