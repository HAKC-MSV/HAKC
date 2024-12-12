export HAKC_ROOT=$(git rev-parse --show-toplevel)
export HAKC_CLANG=$HAKC_ROOT/install/bin/clang
export HAKC_PASS=$HAKC_ROOT/install/lib/HAKC-Compartmentalizer.so
export HAKC_TESTS_PATH_IN=$HAKC_ROOT/HAKC-pass/tests/llvm_tests/tests
export HAKC_TESTS_PATH_OUT=$HAKC_ROOT/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test
export HAKC_PYTHON_PATH=$HAKC_ROOT/python
