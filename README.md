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
1. `cd linux && make defconfig`
1. Adjust LOCATION in build-ros2-demo-kernel.sh 
