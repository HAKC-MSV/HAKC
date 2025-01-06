#/bin/sh

mkdir -p build

env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=phi.yml \
$TEST_CLANG \
  -fexperimental-new-pass-manager \
  -fpass-plugin=$TEST_PASS \
  -g -S -emit-llvm -O2 -o build/test.bc -c test.c
