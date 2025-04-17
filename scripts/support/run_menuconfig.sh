#!/bin/bash

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_BUILD_ROOT && \
  cmake --build . --target clang -j$(nproc)

cd $HAKC_ROOT/linux

if [ -f $HAKC_BUILD_ROOT/linux/x86/analysis/.config ]; then
  make O=$HAKC_BUILD_ROOT/linux/x86/analysis LLVM=1 ARCH=x86 CC=$HAKC_BUILD_ROOT/llvm-project/llvm/bin/clang HOSTCC=$HAKC_BUILD_ROOT/llvm-project/llvm/bin/clang -j$(nproc) \
  defconfig
fi

make O=$HAKC_BUILD_ROOT/linux/x86/analysis LLVM=1 ARCH=x86 CC=$HAKC_BUILD_ROOT/llvm-project/llvm/bin/clang HOSTCC=$HAKC_BUILD_ROOT/llvm-project/llvm/bin/clang -j$(nproc) \
menuconfig
