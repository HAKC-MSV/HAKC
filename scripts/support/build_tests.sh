#!/bin/bash

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH

rm -rf $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/lib/* $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/test/* $HAKC_ROOT/llvm-project/compiler-rt/test/hakc


# don't try to do a standalone build, it won't work probably 
# command to build llvm
# the arguments to enable the tests are as follows: 
# -DCOMPILER_RT_BUILD_HAKC=ON \
# -DCOMPILER_RT_INCLUDE_TESTS=ON \
cmake --fresh -G Ninja \
-DLLVM_ENABLE_PROJECTS='compiler-rt;clang;clang-tools-extra;lld' \
-DLLVM_ENABLE_RUNTIMES='' \
-DCMAKE_INSTALL_PREFIX=$HAKC_INSTALL_PATH \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DCMAKE_C_COMPILER=clang \
-DCMAKE_CXX_COMPILER=clang++ \
-DLLVM_TARGETS_TO_BUILD='X86;AArch64;ARM' \
-DLLVM_USE_LINKER=lld \
-DLLVM_ENABLE_IDE=True \
-DCOMPILER_RT_BUILD_HAKC=ON \
-DCOMPILER_RT_INCLUDE_TESTS=ON \
-DHAKC_LLVM=True $HAKC_ROOT

cmake --build . -j$(nproc) --target install

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
