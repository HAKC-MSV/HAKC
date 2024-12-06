## Build the HAKC compiler pass
export ROOT=$(git rev-parse --show-toplevel)
# git submodule update --init --recursive

cd $ROOT

rm -rf $ROOT/cmake-build-hakc-pass-linux-x86/*

cd $ROOT/cmake-build-hakc-pass-linux-x86

cmake -G Ninja \
-DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
..

cmake --build . -j$(nproc) --target install