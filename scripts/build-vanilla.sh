#!/bin/bash

BUILD_TYPE=_vanilla-beagle
LOCATION=/home/na28772/Code/linux-env/beagle/$BUILD_TYPE/boot

make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
  CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
  -j$(nproc) clean

#cp arch/arm64/configs/bb.org_defconfig _vanilla-beagle/.config
#
#make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
#  CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
#  -j$(nproc) \
#  menuconfig

make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
  CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
  -j$(nproc) \

make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
  CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
  modules_install
