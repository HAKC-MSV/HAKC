export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

clear

# clean test directory 
rm -rf $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/Output/*

# copy test case to llvm compiler-rt xray test directory 
cp -rf tests/* $ROOT/llvm-project/compiler-rt/test/hakc/TestCases

# make tests
cd $ROOT/llvm-project/compiler-rt-build && make check-hakc
llvm-lit test/hakc/X86_64LinuxConfig/