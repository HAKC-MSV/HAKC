# HAKC

Instructions for how to build all code and run the ROS2 demo in QEMU.

## Prerequisites

* `Binutils 2.33.1+`
* `aarch64-linux-gnu`

## Set up

1. `bash scripts/support/init.sh`
2. `python3 -m venv python/venv`
3. `source python/venv/bin/activate`
4. `python3 -m pip install -r llvm-project/llvm/utils/hakc/requirements.txt`

## Build LLVM

1. `bash scripts/support/build_llvm.sh`

### tl;dr

Creating the initial compartmentalization can be accomplished (assuming the python virtual
environment is still active) by

1. `llvm-project/llvm/utils/hakc/hakc-analysis-server-process --config cmake-build-hakc/linux/x86/hakc-server.yaml &`
2. `cd cmake-build-hakc`
3. `cmake --build . --target linux-x86-dag`

The kernel will be built in `cmake-build-hakc/linux/x86/analysis` and the database storing
the compartmentalization policy will be in `cmake-build-hakc/linux/x86/hakc-db`

## Build the Kernel

Note, these directions imply a valid HAKC configuration and policy server configuration is
created, and all paths are correct. See [docs/README.md](docs/README.md) for more
information.

1. `cd linux`
2. ```
   make \
   LLVM=1 \
   O=$(realpath ../cmake-build-hakc/linux/x86/analysis) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=hakc-linux-x86 \
   -j$(nproc) defconfig
   ```
3. ```
   `scripts/config --file $(realpath ../cmake-build-hakc/linux/x86/analysis/.config) \
   -e CONFIG_HAKC \
   --set-str CONFIG_HAKC_CONFIG_PATH \
   $(realpath ../cmake-build-hakc/linux/x86/analysis-config.yaml) \
   -e CONFIG_DEBUG_INFO_DWARF5 \
   -d CONFIG_DEBUG_INFO_NONE \
   -e CONFIG_DEBUG_INFO \
   -e CONFIG_DEBUG_INFO_COMPRESSED_NONE \
   -d CONFIG_DEBUG_INFO_COMPRESSED_ZLIB \
   -d CONFIG_DEBUG_INFO_COMPRESSED_ZSTD \
   -d CONFIG_DEBUG_INFO_REDUCED \
   -e CONFIG_DEBUG_INFO_SPLIT \
   -e CONFIG_GDB_SCRIPTS \
   -e CONFIG_HAKC_ALLOW_FAILED \
   -d CONFIG_HAKC_DEBUG_PRINT \
   -d CONFIG_HAKC_DEMO_LEAK \
   -d CONFIG_HAKC_KOBUKI_CHECKSUM_TWEAK \
   -d CONFIG_HAKC_LOG_FAILURE \
   -e CONFIG_HAKC_X86 \
   -e CONFIG_HAKC_X86_MEMORY \
   -e CONFIG_HAKC_X86_SIGN_NI \
   -d CONFIG_HAKC_XPAD_INSERT_COMMAND \
   -d CONFIG_HAKC_XPAD_RANDOMIZE \
   -e CONFIG_HAKC_LEAK_POINTER \
   -m CONFIG_ROSDEMO
   ```
4. ```
   make \
   LLVM=1 \
   O=$(realpath ../cmake-build-hakc/linux/x86/analysis) \
   CC=$(realpath ../install/bin/clang) \
   HOSTCC=$(realpath ../install/bin/clang) \
   LOCALVERSION=hakc-linux-x86 \
   -j$(nproc)
   ```

## Create Initial Compartmentalization

1. ```
   python3 llvm-project/llvm/utils/hakc/hakc-static-analysis \
   --dag-files-root cmake-build-hakc/linux/x86/hakc-dag-analysis \
   --create-dag --db-dir cmake-build-hakc/linux/x86/hakc-db
   ```

## Adjusting the compartmentalization for targeted compartmentalization applications

This is an example of how to adjust the initial compartmentalization to suit a specific
application. Adjusting a compartmentalization involves writing a YAML file that instructs
the static analysis Python script to adjust the base compartmentalization by placing
symbols together into a compartment, and which other symbols should be placed in the
permissive compartment (named the kernel compartment for historic reasons).  
The permissive compartment does not perform any checks, but instead ensures that a pointer
is valid before dereferencing it. This example is for the ROS 2 demo.

1. `cp -r cmake-build-hakc/linux/x86/hakc-db cmake-build-hakc/linux/x86/hakc-db-base`
2. ```
   python3 llvm-project/llvm/utils/hakc/hakc-static-analysis --db-dir \
   cmake-build-hakc/linux/x86/hakc-db --adjust \
   --adjust-path configs/compartmentalizations/linux/adjustments/linux-x86-adjustments.yml
    ```

## Build the protected kernel

1. `llvm-project/llvm/utils/hakc/hakc-policy-process --config 
cmake-build-hakc/linux/x86/hakc-policy-server.yaml &`
2. `cd cmake-build-hakc`
3. `cmake --build . --target linux-x86-compartmentalize-pass`

llvm-project/llvm/utils/hakc/hakc-policy-process --config  cmake-build-hakc/linux/x86/hakc-server.yaml

[//]: # (make O=/home/de29664/code/HAKC/cmake-build-hakc/linux/x86/compartmentalize LLVM=1 ARCH=x86 CC=/home/de29664/code/HAKC/cmake-build-hakc/llvm-project/llvm/bin/clang-21 HOSTCC=/home/de29664/code/HAKC/cmake-build-hakc/llvm-project/llvm/bin/clang-21 -j$&#40;nproc&#41;)


## Run all tests 
1. `cd cmake-build-hakc`
2. `cmake --build . --target check-hakc`

## Run an individual test 
1. `cd /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test\
&& /usr/bin/python3.10 /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/./bin/llvm-lit \
-a /home/al32163/HAKC/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0

python /home/al32163/HAKC/llvm-project/llvm/utils/hakc/hakc-analysis-server-process \
--config /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/yaml/hakc_server_config.yml \
--log-level DEBUG

cmake-build-hakc/llvm-project/llvm/bin/clang -O0 -g -S -emit-llvm -mllvm --enable-hakc -mllvm \
--hakc-config=/home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/yaml/hakc_config.yml \
-o /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/yaml/Output/hakc_analysis_test0.c.tmp \
/home/al32163/HAKC/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/hakc_analysis_test0.c

clear && python3 /home/al32163/HAKC/llvm-project/llvm/utils/hakc/hakc-static-analysis \
--dag-files-root /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/yaml/dag-analysis \
--create-dag --db-dir /home/al32163/HAKC/cmake-build-hakc/llvm-project/llvm/test/Transforms/Compartmentalization/hakc/tests/hakc_analysis_test0/yaml/hakc-db \
--adjust --adjust-path /home/al32163/HAKC/cmake-build-hakc/linux/x86/linux-x86-adjustments.yml --delete-existing-db --log-level DEBUG \
--core-count 100

clear && python3 /home/al32163/HAKC/llvm-project/llvm/utils/hakc/hakc-static-analysis \
--dag-path /home/al32163/HAKC/cmake-build-hakc/linux/x86/linux_kernel_dag.yml \
--create-dag --db-dir /home/al32163/HAKC/cmake-build-hakc/linux/x86/hakc-db \
--adjust --adjust-path /home/al32163/HAKC/cmake-build-hakc/linux/x86/linux-x86-adjustments.yml --delete-existing-db --log-level DEBUG \
--core-count 100


clear && python3 /home/al32163/HAKC/llvm-project/llvm/utils/hakc/hakc-server-process --config cmake-build-hakc/linux/x86/hakc-server.yaml --server-mode analysis --log-level DEBUG