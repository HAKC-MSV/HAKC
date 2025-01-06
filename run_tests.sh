#!/bin/bash
source vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig

if [[ -n "$1" ]]; then
    # llvm-lit -v TestCases/Posix/hakc-test$1.c
    if [[ -n "$2" ]]; then
        echo TestCases/Posix/hakc_test$1/hakc_test$1.c
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout $2 TestCases/Posix/hakc_test$1/hakc_test$1.c
    else
        echo TestCases/Posix/hakc_test$1/hakc_test$1.c
        $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 15 TestCases/Posix/hakc_test$1/hakc_test$1.c
    fi

else
    $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/bin/llvm-lit -a --timeout 60 .
fi
