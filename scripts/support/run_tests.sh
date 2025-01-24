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
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 15 Transforms/Compartmentalization/hakc_test$1/hakc_test$1.c
    fi

else
    $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 60 .
fi
