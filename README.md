# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Prerequisites

* Binutils 2.33.1+,
* aarch64-linux-gnu

## Set up

1. `bash scripts/support/init.sh`

## Build LLVM

1. `bash scripts/support/build-llvm.sh`

## Build the Kernel

1. `export BUILD_TYPE=linux-x86`
2. `cd $ROOT`
3. `mkdir -p build-$BUILD_TYPE/hakc-dag-analysis`
4. `cd linux`
5. ```
   env HAKC_ANALYSIS=dag \
   HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
   HAKC_SOURCE_PATH=$PWD \
   HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
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
   --set-str CONFIG_HAKC_PASS_PATH \
   $(realpath ../install/lib/libHAKC-Compartmentalizer-$BUILD_TYPE.so) \
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
   -d CONFIG_HAKC_LOG_FAILURE \
   -d CONFIG_HAKC_ARM_V9 \
   -e CONFIG_HAKC_ARM_V8 \
   -e CONFIG_HAKC_ARM_V8_MEMORY
   ```
7. ```
   env HAKC_ANALYSIS=dag \
   HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
   HAKC_SOURCE_PATH=$PWD \
   HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
   make \
   ARCH=arm64 \
   CROSS_COMPILE=aarch64-linux-gnu- \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(nproc)
   ```

## DAG Analysis

1. `cd $ROOT`
2. ```
   python3 python/analysis/hakc-dag.py \
   --c-out build-$BUILD_TYPE/hakc-dag-analysis/dag.bin \ 
   --create-dag --dag-files-root build-$BUILD_TYPE/hakc-dag-analysis \ 
   --core-count $(( $(nproc) * 9 / 10 ))
   ```

## Apply compartmentalization modifications and output compartmentalization policy

1. `cd $ROOT`
2. ```
   python3 python/analysis/hakc-dag.py \
   --c-in build-$BUILD_TYPE/hakc-dag-analysis/dag.bin \
   --adjust --adjust-path scripts/ros2-demo/rosdemo-compartments.yml \ 
   --output-yaml \ 
   --output-yaml-path build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml
   ``` 
    * This creates `build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml` which is
      the compartmentalization policy that will be used to build a protected kernel.

## Compile kernel with compartments enforced

1. `cd linux`
2. ```
   env HAKC_ANALYSIS=compartmentalize \
   HAKC_COMPARTMENT_PATH=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml) \
   HAKC_SOURCE_PATH=$PWD \
   HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
   make \
   ARCH=arm64 \
   CROSS_COMPILE=aarch64-linux-gnu- \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(( $(nproc) * 9 / 10 )) clean
   ```
3. ```
   env HAKC_ANALYSIS=compartmentalize \
   HAKC_COMPARTMENT_PATH=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml) \
   HAKC_SOURCE_PATH=$PWD \
   HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
   make \
   ARCH=arm64 \
   CROSS_COMPILE=aarch64-linux-gnu- \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(( $(nproc) * 9 / 10 )) 
   ```

## Run the kernel in QEMU
