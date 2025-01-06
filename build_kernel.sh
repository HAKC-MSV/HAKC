source vars.sh
export BUILD_TYPE=linux-x86
cd $ROOT
mkdir -p build-$BUILD_TYPE/hakc-dag-analysis
cd linux

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
