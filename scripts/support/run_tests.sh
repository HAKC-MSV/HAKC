#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test && clear

if [[ -n "$1" ]]; then
    if [[ -n "$2" ]]; then
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a Transforms/Compartmentalization/hakc/tests/hakc_test$1/hakc_test$1.c
    else
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a Transforms/Compartmentalization/hakc/tests/hakc_test$1/hakc_test$1.c
    fi

else
    echo "Running all tests" 
    echo "$HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a  Transforms/Compartmentalization/hakc/tests/hakc_test*"
    $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a  Transforms/Compartmentalization/hakc/tests/hakc_test*
fi
