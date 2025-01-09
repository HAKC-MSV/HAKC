#!/bin/bash
## Build the HAKC compiler pass
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh
# git submodule update --init --recursive

rm -rf $HAKC_ROOT/cmake-build-hakc-pass/
mkdir $HAKC_ROOT/cmake-build-hakc-pass/
cd $HAKC_ROOT/cmake-build-hakc-pass

cmake -G Ninja \
-DCMAKE_INSTALL_PREFIX=$HAKC_ROOT/install \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
..

cmake --build . -j$(nproc) --target install
