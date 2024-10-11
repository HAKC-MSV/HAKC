export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

# env HAKC_ANALYSIS=custom HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
# env HAKC_ANALYSIS=custom HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -mypass_option=abc  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
# $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -ArchYamlPath=x86config.yaml -mllvm -CompartmentYamlPath="tests/hakc-test0.yml" -mllvm -HAKCPassMode=compartmentalize  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
clear

# clean test directory 
rm -rf $ROOT/llvm-project/compiler-rt-build/test/hakc/X86_64LinuxConfig/TestCases/Output/*

# copy test case to llvm compiler-rt xray test directory 
mkdir $ROOT/llvm-project/compiler-rt/test
cp -rf tests/* $ROOT/llvm-project/compiler-rt/test/hakc/TestCases

# cd $ROOT/llvm-project/compiler-rt-build && llvm-lit test/hakc/X86_64LinuxConfig/TestCases/hakc-test0.c
# env HAKC_DEBUG_NAME=foo $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -ArchYamlPath=x86config.yaml -mllvm -CompartmentYamlPath="tests/hakc-test0.yml" -mllvm -HAKCPassMode=compartmentalize  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
$TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=x86config.yaml -mllvm -HAKC_COMPARTMENT_PATH="tests/hakc-test0.yml" -mllvm -HAKC_ANALYSIS=custom  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c