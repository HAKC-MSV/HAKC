#!/bin/bash

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

rm -rf $HAKC_LLVM_BUILD_PATH
mkdir $HAKC_LLVM_BUILD_PATH
# rm -rf $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

mkdir $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
mkdir $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/lib/hakc/* $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/test/hakc/* $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

# don't try to do a standalone build, it won't work probably 
$HAKC_ROOT/scripts/support/build_llvm.sh

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

# go to test folder
# cd /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig
# run test 
# llvm-lit -a TestCases/Posix/hakc_test0/hakc_test0.c

# run manually 
# cd /home/al32163/hakc/HAKC_CURR/HAKC-pass/tests/compiler-rt/test/hakc/TestCases/Posix/hakc_test0
 "%clangxx_hakc -fpass-plugin=%HAKC_TEST_PASS -Xclang -load -Xclang %HAKC_TEST_PASS -mllvm -HAKC_CONFIG=%HAKC_YAML_CONFIG_DAG -g -S -emit-llvm -O2 -o %t.dag.ll -c %s"
# /home/al32163/hakc/HAKC_CURR/install/bin/clang -mllvm --HAKC_CONFIG -mllvm hakc_test0_clang.yml -g -S -emit-llvm -o aoeu.ll -c hakc_test0.c 
# /home/al32163/hakc/HAKC_CURR/install/bin/opt --HAKC_CONFIG hakc_test0_adjustments.yml aoeu.bc
+ /home/al32163/hakc/HAKC_CURR/install/bin/clang++  --HAKC_CONFIG=/home/al32163/hakc/HAKC_CURR/llvm-project/compiler-rt/test/hakc/TestCases/Posix/hakc_test0/hakc_test0_config.yml_DAG -g -S -emit-llvm -O2 -o /home/al32163/hakc/HAKC_CURR/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig/TestCases/Posix/hakc_test0/Output/hakc_test0.c.tmp.dag.ll -c /home/al32163/hakc/HAKC_CURR/llvm-project/compiler-rt/test/hakc/TestCases/Posix/hakc_test0/hakc_test0.c 

# in install/bin 
# ./clang /home/al32163/hakc/HAKC_CURR/HAKC-pass/tests/compiler-rt/test/hakc/TestCases/Posix/hakc_test0/hakc_test0.c

# invoke via opt
# ./opt -print-passes
# /home/al32163/hakc/HAKC_CURR/install/bin/opt --HAKC_CONFIG hakc_test0_adjustments.yml aoeu.dag.ll
