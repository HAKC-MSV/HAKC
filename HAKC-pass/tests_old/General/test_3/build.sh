#/bin/sh

mkdir -p build

env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=test-0.yml \
  $TEST_CLANG \
  -fexperimental-new-pass-manager \
  -fpass-plugin=$TEST_PASS -I /home/al32163/hakc/HAKC/linux/tools/include \
  -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c
