#!/bin/bash

BUILD_TYPE=_ros2_demo
LOCATION=/home/na28772/Code/linux-env/QEMO-Ros2/$BUILD_TYPE/boot

mkdir -p $LOCATION

while [ -n "$1" ]; do
  case "$1" in
    clean) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
           CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
           -j$(nproc) clean ;;
    menuconfig) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
                CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
                -j$(nproc) menuconfig ;;
    build) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
           CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
           -j$(nproc) ;;
    install) make O=$BUILD_TYPE LOCALVERSION=$BUILD_TYPE ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LLVM=1 \
             CC=clang HOSTCC=clang INSTALL_MOD_PATH=$LOCATION INSTALL_PATH=$LOCATION \
             modules_install ;;
    *) echo "Options are: clean, menuconfig, build, install" ;;
  esac
  shift
done
