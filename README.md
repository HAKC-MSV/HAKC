# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Set up

1. `ROOT=$PWD`
1. `git submodule update --init --recursive`

## Build LLVM 12
1. `cd llvm-project`
2. `git apply ../llvm-patches/*.patch`
3. `cd ..`
4. `mkdir cmake-build-hakc-llvm`
5. `cd cmake-build-hakc-llvm`
6. ```
   cmake -G Ninja \
   -DLLVM_ENABLE_PROJECTS='clang;lld;clang-tools-extra;llvm' \
   -DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
   -DCMAKE_BUILD_TYPE=RelWithDebInfo \
   -DCMAKE_C_COMPILER=/usr/bin/clang \
   -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
   -DLLVM_TARGETS_TO_BUILD='X86;AArch64' \
   -DLLVM_OPTIMIZED_TABLEGEN=True \
   -DLLVM_USE_LINKER=lld \
   -DLLVM_ENABLE_IDE=True \
   -DHAKC_LLVM=True ..
   ```
7. `cmake --build . --target install -j$(nproc)`


## Build the HAKC compiler pass
1. `cd $ROOT`
2. `mkdir cmake-build-hakc-pass-linux-{armv8,armv9,x86}`
3. `cd cmake-build-hakc-pass-linux-armv8`
4. ```
   cmake -G Ninja \
   -DHAKC_LINUX_ARMV8=True \
   -DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
   -DCMAKE_BUILD_TYPE=Relase \
   -DCMAKE_C_COMPILER=$(realpath ..)/install/bin/clang \
   -DCMAKE_CXX_COMPILER=$(realpath ..)/install/bin/clang++ ..
   ```
5. `cmake --build . -j$(nproc)`
6. Repeat steps 3-5 for the other directories created in step 2, but replacing `-DHAKC_LINUX_ARMV8=True` with 
    * `-DHAKC_LINUX_ARMV9=True` for `armv9`
    * `-DHAKC_LINUX_X86=True` for `x86`

## Build the Kernel

1. `export BUILD_TYPE=linux-armv8`
2. `cd $ROOT`
3. `mkdir -p build-$BUILD_TYPE/hakc-dag-analysis`
4. `cd linux`
5. ```
   env HAKC_ANALYSIS=dag \
   HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
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
   scripts/config --file $(realpath ../build-$BUILD_TYPE/.config) \
   -e CONFIG_HAKC \
   --set-str CONFIG_HAKC_PASS_PATH \
   $(realpath ../cmake-build-hakc-pass-$BUILD_TYPE)/HAKC-pass/lib/libHAKC-Compartmentalizer-$BUILD_TYPE.so \
   -e CONFIG_HAKC_ARM_V8 \
   -d CONFIG_HAKC_ALLOW_FAILED \
   -e CONFIG_HAKC_SIGN_PTR \
   -m CONFIG_ROSDEMO \
   -e CONFIG_DEBUG_INFO \
   -e CONFIG_DEBUG_INFO_SPLIT \
   -e CONFIG_DEBUG_INFO_DWARF4 \
   -e CONFIG_GDB_SCRIPTS \
   -e CONFIG_HAKC_ARM_V8_MEMORY \
   -d CONFIG_HAKC_ARM_V9 \
   -d CONFIG_HAKC_DEBUG_PRINT \
   -d CONFIG_HAKC_ALLOW_FAILED \
   -d CONFIG_HAKC_LOG_FAILURE
   ```
7. ```
   env HAKC_ANALYSIS=dag \
   HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
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
2. `python3 scripts/analysis/data-access-analysis.py -c build-$BUILD_TYPE/hakc-dag-analysis/dag.bin -r
   build-$BUILD_TYPE/hakc-dag-analysis --dag --filter_types --filter_mod_files`

## Create and apply compartmentalization modifications

1. `cd $ROOT`
2. ```
   sed "s+_KERNEL_SOURCE_+$(realpath linux)+g" scripts/ros2-demo/rosdemo-compartments.yml | \
   sed "s+_KERNEL_BUILD_+$(realpath build-$BUILD_TYPE)+g" > build-$BUILD_TYPE/hakc-dag-analysis/hakc-ros2-adjustments.yml
   ```
3. `python3 scripts/analysis/data-access-analysis.py -c build-$BUILD_TYPE/hakc-dag-analysis/dag.bin
   --adjust build-$BUILD_TYPE/hakc-dag-analysis/hakc-ros2-adjustments.yml`
   * This creates `build-$BUILD_TYPE/hakc-dag-analysis/dag-adjusted.bin`

## Output compartmentalization policy

1. `python3 scripts/analysis/data-access-analysis.py -c build-$BUILD_TYPE/hakc-dag-analysis/dag-adjusted.bin 
    --output_compart build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml`

## Compile kernel with compartments enforced

1. `cd linux`
2. ```
   env HAKC_ANALYSIS=compartmentalize \
   HAKC_COMPARTMENT_PATH=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml) \
   make \
   ARCH=arm64 \
   CROSS_COMPILE=aarch64-linux-gnu- \
   LLVM=1 \
   O=$(realpath ../build-$BUILD_TYPE) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=$BUILD_TYPE \
   -j$(nproc) clean
   ```
3. ```
   env HAKC_ANALYSIS=compartmentalize \
   HAKC_COMPARTMENT_PATH=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml) \
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

## Run the kernel in QEMU

