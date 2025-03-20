#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

rm -rf $HAKC_ROOT/cmake-build-hakc-llvm
mkdir -p $HAKC_ROOT/cmake-build-hakc-llvm

./build_tests.sh
