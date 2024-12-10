export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

clear

# clean test directory 
rm -rf $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/Output/*

# copy test case to llvm compiler-rt xray test directory 
mkdir $ROOT/llvm-project/compiler-rt/test
cp -rf tests/* $ROOT/llvm-project/compiler-rt/test/hakc/TestCases

# $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/HAKC-pass/config/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$ROOT/HAKC-pass/tests/llvm_tests/tests/hakc-test0.yml -mllvm -HAKC_ANALYSIS=compartmentalize -mllvm -HAKC_DEBUG_NAME=foo -g -S -emit-llvm -O2 -o $ROOT/HAKC-pass/tests/llvm_tests/tests/hakc-test0.c.ll -c $ROOT/HAKC-pass/tests/llvm_tests/tests/hakc-test0.c

# llvm-lit $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/hakc-test0.c

# $TEST_CLANG TEST_PASS=/home/al32163/hakc/HAKC/install/lib/HAKC-Compartmentalizer.so ./build.sh
# $TEST_CLANG ./build.sh