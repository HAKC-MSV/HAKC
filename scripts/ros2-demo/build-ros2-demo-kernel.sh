#!/bin/bash

mkdir -p $LOCATION

while [ -n "$1" ]; do
  case "$1" in
    clean) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
           CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
           -j$(nproc) clean ;;
    defconfig) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
                CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
                -j$(nproc) defconfig ;;
    menuconfig) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
                CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
                -j$(nproc) menuconfig ;;
    build) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
           CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
           -j$(nproc) ;;
    install) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
             CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
             modules_install ;;
    *) echo "Options are: defconfig, menuconfig, clean, build, install" ;;
  esac
  shift
done
