#/bin/sh
source vars.sh

rm -rf $HAKC_ROOT/cmake-build-hakc-llvm
mkdir -p $HAKC_ROOT/cmake-build-hakc-llvm

./build_tests.sh