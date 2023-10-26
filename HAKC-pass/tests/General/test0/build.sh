#/bin/sh

mkdir -p build

env HAKC_ANALYSIS=compartmentalize HAKC_COMPARTMENT_PATH=test-0.yml \
  /home/de29664/code/llvm-project/cmake-build-12.x/install-12.x/bin/clang \
  -fexperimental-new-pass-manager \
  -fpass-plugin=/home/de29664/code/ARM-MTE/cmake-build-x86-hakc-clang-12/HAKC-Compartmentalizer/lib/libHAKC-Compartmentalizer.so \
  -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c