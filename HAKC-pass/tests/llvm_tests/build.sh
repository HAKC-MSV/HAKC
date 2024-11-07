export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

clear

# clean test directory 
rm -rf $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/Output/*

# copy test case to llvm compiler-rt xray test directory 
mkdir $ROOT/llvm-project/compiler-rt/test
cp -rf tests/* $ROOT/llvm-project/compiler-rt/test/hakc/TestCases

# make tests
cd $ROOT/llvm-project/compiler-rt-build && make check-hakc
llvm-lit test/hakc/X86_64LinuxConfig/
# llvm-lit $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/hakc-test0.c

# apply hakc individually (for debugging purposes, code refactor)
# env HAKC_ANALYSIS=custom HAKC_COMPARTMENT_PATH=hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o hakc-test0.c.ll -c hakc-test0.c