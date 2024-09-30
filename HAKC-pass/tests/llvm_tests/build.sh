

ROOT=/home/al32163/hakc/HAKC/
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so


# copy test case to llvm compiler-rt xray test directory 
cp tests/hakc-base-test.cpp ../../../llvm-project/compiler-rt/test/xray/TestCases/Posix/
cp tests/hakc-test0.c ../../../llvm-project/compiler-rt/test/xray/TestCases/Posix/
cp tests/hakc-test0.yml ../../../llvm-project/compiler-rt/test/xray/TestCases/Posix/

# make tests
cd ../../../llvm-project/compiler-rt-build && make check-xray
llvm-lit test/xray/X86_64LinuxConfig/TestCases/Posix/hakc-base-test.cpp
llvm-lit test/xray/X86_64LinuxConfig/TestCases/Posix/hakc-test0.c