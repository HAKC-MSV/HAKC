#/bin/sh
export ROOT=$(git rev-parse --show-toplevel)
export TEST_PASS=$ROOT/install/lib/HAKC-Compartmentalizer.so
mkdir -p build

$ROOT/install/bin/clang \
  -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS \
  -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c
