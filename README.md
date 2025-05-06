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

### tl;dr

The following steps can be accomplished by

1. `cd cmake-build-hakc`
2. `cmake --build . --target linux-x86-dag`

## Build LLVM

1. `bash scripts/support/build_llvm.sh`

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
   -d CONFIG_HAKC_XPAD_RANDOMIZE
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
   python3 llvm-project/llvm/utils/hakc/hakc-policy-process \
   --dag-files-root cmake-build-hakc/linux/x86/hakc-dag-analysis \
   --create-dag --db-dir cmake-build-hakc/linux/x86/hakc-db
   ```
