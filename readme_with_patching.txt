# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Prerequisites

* Binutils 2.33.1+,
* aarch64-linux-gnu

## Set up

1. `ROOT=$PWD`
1. `git submodule update --init --recursive`

## Configure which Linux version to use for HAKC
 
1. `git submodule init linux`
2. `git submodule update`
 
3. `cd linux`
4. `git checkout -- .`
5. `git clean -f -d`

6.
For v5: `git checkout v5.15`

For v6: `git checkout v6.6`

7. `cd scripts/patch-generation`

8.
For v5: `./apply_v515_patches.sh`

For v6: `./apply_patches.sh`

9. `cd $ROOT`

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
   -DLLVM_TARGETS_TO_BUILD='X86;AArch64;ARM' \
   -DLLVM_OPTIMIZED_TABLEGEN=True \
   -DLLVM_USE_LINKER=lld \
   -DLLVM_ENABLE_IDE=True \
   -DHAKC_LLVM=True ..
   ```
7. `cmake --build . --target install -j$(nproc)`


## Build the HAKC compiler pass
1. `cd $ROOT`
2. `mkdir cmake-build-hakc-pass-{linux-{armv8,armv9,x86},cheribsd-morello}`
3. `cd cmake-build-hakc-pass-linux-armv8`
4. ```
   cmake -G Ninja \
   -DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
   -DCMAKE_BUILD_TYPE=RelWithDebInfo \
   -DCMAKE_C_COMPILER=$(realpath ..)/install/bin/clang \
   -DCMAKE_CXX_COMPILER=$(realpath ..)/install/bin/clang++ \
   -DHAKC_LINUX_ARMV8=True \
   ..
   ```
5. `cmake --build . -j$(nproc) --target install`
6. Repeat steps 3-5 for the other directories created in step 2, but replacing `-DHAKC_LINUX_ARMV8=True` with 
    * `-DHAKC_LINUX_ARMV9=True` for `armv9`
    * `-DHAKC_LINUX_X86=True` for `x86`
    * `-DHAKC_CHERIBSD_MORELLO=True` for `Morello`

cmake -G Ninja \
-DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DCMAKE_C_COMPILER=$(realpath ..)/install/bin/clang \
-DCMAKE_CXX_COMPILER=$(realpath ..)/install/bin/clang++ \
-DHAKC_LINUX_X86=True \
..

## Build the Kernel

1. `export BUILD_TYPE=linux-armv8`
export BUILD_TYPE=linux-x86
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
env HAKC_ANALYSIS=dag \
HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
HAKC_SOURCE_PATH=$PWD \
HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
make \
LLVM=1 \
O=$(realpath ../build-$BUILD_TYPE) \
CC=$(realpath ../install/bin/clang) \
HOSTCC=$(realpath ../install/bin/clang) \
LOCALVERSION=$BUILD_TYPE \
-j$(nproc) hakc_x86_defconfig
6. ```
   scripts/config --file $(realpath ../build-$BUILD_TYPE/.config) \
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
scripts/config --file $(realpath ../build-$BUILD_TYPE/.config) \
-e CONFIG_HAKC \
--set-str CONFIG_HAKC_PASS_PATH \
$(realpath ../install/lib/libHAKC-Compartmentalizer-$BUILD_TYPE.so) \
-e CONFIG_HAKC_X86 \
-e CONFIG_HAKC_X86_SIGN_SSSE3=y \
-d CONFIG_HAKC_ARM_V9 \
-d CONFIG_HAKC_ARM_V8 \
-d CONFIG_HAKC_ALLOW_FAILED \
-e CONFIG_HAKC_SIGN_PTR \
-m CONFIG_ROSDEMO \
-e CONFIG_DEBUG_INFO \
-e CONFIG_DEBUG_INFO_SPLIT \
-e CONFIG_DEBUG_INFO_DWARF4 \
-e CONFIG_GDB_SCRIPTS \
-e CONFIG_HAKC_X86_MEMORY \
-d CONFIG_HAKC_DEBUG_PRINT \
-d CONFIG_HAKC_LOG_FAILURE \
-e CONFIG_BPF_SYSCALL \
-d CONFIG_KPROBES \
-e CONFIG_JUMP_LABEL \
-d CONFIG_STATIC_KEYS_SELFTEST \
-e CONFIG_SECCOMP \
-d CONFIG_SECCOMP_CACHE_DEBUG \
-e CONFIG_STACKPROTECTOR \
-e CONFIG_STACKPROTECTOR_STRONG \
-d CONFIG_SHADOW_CALL_STACK \
-e CONFIG_LTO_NONE \
-d CONFIG_LTO_CLANG_FULL \
-d CONFIG_LTO_CLANG_THIN \
-e CONFIG_COMPAT_32BIT_TIME \
-e CONFIG_VMAP_STACK \
-e CONFIG_RANDOMIZE_KSTACK_OFFSET \
-d CONFIG_RANDOMIZE_KSTACK_OFFSET_DEFAULT \
-d CONFIG_LOCK_EVENT_COUNTS \
-e CONFIG_RELR 

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
env HAKC_ANALYSIS=dag \
HAKC_DAG_ANALYSIS_ROOT=$(realpath ../build-$BUILD_TYPE/hakc-dag-analysis) \
HAKC_SOURCE_PATH=$PWD \
HAKC_BUILD_PATH=$(realpath ../build-$BUILD_TYPE) \
make \
LLVM=1 \
O=$(realpath ../build-$BUILD_TYPE) \
CC=$(realpath ../install/bin/clang) \
HOSTCC=$(realpath ../install/bin/clang) \
LOCALVERSION=$BUILD_TYPE \
-j$(nproc)

## DAG Analysis

1. `cd $ROOT`
2. 
python3 scripts/analysis/data-access-analysis.py --c-out 
build-$BUILD_TYPE/hakc-dag-analysis/dag.bin --create-dag --dag-files-root
   build-$BUILD_TYPE/hakc-dag-analysis


python3 scripts/analysis/data-access-analysis.py -c build-$BUILD_TYPE/hakc-dag-analysis/dag.bin -r build-$BUILD_TYPE/hakc-dag-analysis --dag --filter_types --filter_mod_files




## Apply compartmentalization modifications and output compartmentalization policy

1. `cd $ROOT`
2. `python3 scripts/analysis/data-access-analysis.py --c-in build-$BUILD_TYPE/hakc-dag-analysis/dag.bin
   --adjust --adjust-path scripts/ros2-demo/rosdemo-compartments.yml --output-yaml 
   --output-yaml-path build-$BUILD_TYPE/hakc-dag-analysis/hakc-compartments.yml`
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