# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Prerequisites

* `Binutils 2.33.1+`
* `aarch64-linux-gnu`

## Set up

1. `bash scripts/support/init.sh`

## Build LLVM

1. `bash scripts/support/build_llvm.sh`

## Build the Kernel

Note, these directions imply a valid HAKC configuration and policy server configuration is
created, and all paths are correct. See [docs/README.md](docs/README.md) for more
information.

1. `export BUILD_TYPE=linux-x86`
3. `mkdir -p build-$BUILD_TYPE/hakc-dag-analysis`
4. `cd linux`
5. ```
   make \
   ARCH=arm64 \
   CROSS_COMPILE=aarch64-linux-gnu- \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(nproc) defconfig
   ```
6. ```
   `scripts/config --file $(realpath ../`build-$BUILD_TYPE/.config) \
   -e CONFIG_HAKC \
   --set-str CONFIG_HAKC_CONFIG_PATH \
   $(realpath ../configs/compartmentalizations/linux/linux_x86_config.yaml) \
   -d CONFIG_WERROR \
   -d CONFIG_HAKC_ALLOW_FAILED \
   -e CONFIG_HAKC_SIGN_PTR \
   -m CONFIG_ROSDEMO \
   -e CONFIG_DEBUG_INFO \
   -e CONFIG_DEBUG_INFO_SPLIT \
   -e CONFIG_DEBUG_INFO_DWARF4 \
   -e CONFIG_GDB_SCRIPTS \
   -d CONFIG_HAKC_DEBUG_PRINT \
   -d CONFIG_HAKC_ALLOW_FAILED \
   -d CONFIG_HAKC_LOG_FAILURE
   ```
7. ```
   make \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(nproc)
   ```
