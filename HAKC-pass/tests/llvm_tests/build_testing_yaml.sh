export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

# env HAKC_ANALYSIS=custom HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -fpass-plugin=$TEST_PASS -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
# env HAKC_ANALYSIS=custom HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKCPassMode=custom  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
# env HAKC_ARCH_CONFIG=x86config.yaml HAKC_COMPARTMENT_PATH=tests/hakc-test0.yml $TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKCPassMode=custom  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c
$TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -ArchYamlPath=x86config.yaml -mllvm -CompartmentYamlPath="tests/hakc-test0.yml" -mllvm -HAKCPassMode=custom  -g -S -emit-llvm -O2 -o tests/hakc-test0.c.ll -c tests/hakc-test0.c