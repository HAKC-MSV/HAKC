export ROOT=$(git rev-parse --show-toplevel)
export TEST_CLANG=$ROOT/install/bin/clang
export TEST_PASS=$ROOT/install/lib/libHAKC-Compartmentalizer-linux-x86.so

$TEST_CLANG -fexperimental-new-pass-manager -Xclang -load -Xclang $TEST_PASS -fpass-plugin=$TEST_PASS -mllvm -HAKC_ARCH_CONFIG=$ROOT/configs/linux_x86_config.yaml -mllvm -HAKC_COMPARTMENT_PATH=$ROOT/HAKC-pass/tests/hakc-test0.yml -mllvm -HAKC_ANALYSIS=custom -mllvm -HAKC_DEBUG_NAME=foo -g -S -emit-llvm -O2 -o $ROOT/HAKC-pass/tests/hakc-test0.c.ll -c $ROOT/HAKC-pass/tests/hakc-test0.c