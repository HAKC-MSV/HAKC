## Build the HAKC compiler pass
export ROOT=$(git rev-parse --show-toplevel)

cd $ROOT

rm -rf $ROOT/cmake-build-hakc-pass-linux-x86/*

cd $ROOT/cmake-build-hakc-pass-linux-x86

cmake -G Ninja \
-DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DCMAKE_C_COMPILER=$(realpath ..)/install/bin/clang \
-DCMAKE_CXX_COMPILER=$(realpath ..)/install/bin/clang++ \
-DHAKC_LINUX_X86=True \
..

cmake --build . -j$(nproc) --target install