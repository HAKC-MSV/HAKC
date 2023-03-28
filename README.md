# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Set up

1. `ROOT=$PWD`
1. `git submodule update --init --recursive`

## Build LLVM 12
1. `cd llvm-project && mkdir -p build-12.x/install`
1. `git apply ../ARM-MTE/HAKC-Annotator/*.patch`
1. `cd build-12.x`
1. `../../scripts/llvm-cmake-configure.sh && ninja && ninja install`
1. `PATH=$ROOT/llvm-project/build-12.x/install:$PATH`

## Build the HAKC compiler pass
1. `cd $ROOT`
1. `cd ARM-MTE && mkdir build && cd build`
1. `cmake -DLT_LLVM_INSTALL_DIR=$ROOT/llvm-project/build-12.x/install -DPMC_LLVM_VERSION=12.0.1 ..`
1. `cmake --build . -j 8`

## Build the Kernel

1. `ln -s $ROOT/ARM-MTE/build/HAKC-Compartmentalizer/lib/libHAKC-Compartmentalizer.so scripts/hakc`
1. `cd linux`
1. Adjust LOCATION and BUILD_TYPE in build-ros2-demo-kernel.sh 
1. `../scripts/build-ros2-demo-kernel.sh defconfig`
1. `../scripts/build-ros2-demo-kernel.sh menuconfig`
1. HAKC Options:
  * Kernel Features -> ARMv8.5 architectural features
    * Memory Tagging Extension Support
    * Enable PAC and MTE kernel protections
      * Only: track failed HAKC accesses, sign pointers using the PAC/MTE
        Compartment context
  * Device Drivers -> ROS Demo malicious driver
    * be sure to use the `M` option for module not `Y` or `*` or it won't
      compile
  * Kernel hacking -> compile-time check and compiler options
      * compiler the kernel with debug info
      * produce split debuginfo in .dwo files
      * provide gdb scripts for kernel debugging
1. `export HAKC_ANALYSIS=dag`
1. `export HAKC_DAG_ANALYSIS_ROOT=$PWD/$BUILD_TYPE/hakc-dag-analysis` 
1. `../scripts/build-ros2-demo-kernel.sh build`



