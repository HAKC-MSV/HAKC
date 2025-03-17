#!/bin/bash
# build llvm and hakc with it 
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

mkdir -p $HAKC_LLVM_BUILD_PATH
cd $HAKC_LLVM_BUILD_PATH
CMAKE_INIT_CMD="
cmake -G Ninja
      -DCMAKE_INSTALL_PREFIX=$HAKC_INSTALL_PATH
      -DCMAKE_BUILD_TYPE=RelWithDebInfo
      $(grep -v '^#' $HAKC_ROOT/scripts/llvm-cmake.sh)
      $HAKC_ROOT
"
echo "Building LLVM in $PWD using command "
echo $CMAKE_INIT_CMD

exec $CMAKE_INIT_CMD

cmake --build . -j$(nproc) --target install
