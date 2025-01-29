#!/bin/bash
# build llvm and hakc with it 
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

mkdir $HAKC_LLVM_BUILD_PATH
cd $HAKC_LLVM_BUILD_PATH
echo "Building LLVM in $PWD"

cmake --fresh -G Ninja \
-DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;lld' \
-DCMAKE_INSTALL_PREFIX=$HAKC_INSTALL_PATH \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DCMAKE_C_COMPILER=clang \
-DCMAKE_CXX_COMPILER=clang++ \
-DLLVM_TARGETS_TO_BUILD='X86;AArch64;ARM' \
-DLLVM_USE_LINKER=lld \
-DLLVM_ENABLE_IDE=On \
-DLLVM_ENABLE_HAKC=On \
-DLLVM_BUILD_TESTS=On \
$HAKC_ROOT

cmake --build . -j$(nproc) --target install
