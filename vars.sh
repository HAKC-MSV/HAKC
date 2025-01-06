export HAKC_ROOT=$(git rev-parse --show-toplevel)
export HAKC_INSTALL_PATH=$HAKC_ROOT/install
export HAKC_LLVM_BUILD_PATH=$HAKC_ROOT/cmake-build-hakc-llvm
export HAKC_CLANG=$HAKC_INSTALL_PATH/bin/clang
export HAKC_PASS=$HAKC_INSTALL_PATH/lib/HAKC-Compartmentalizer.so
# export HAKC_TESTS_PATH_IN=$HAKC_ROOT/HAKC-pass/tests/llvm_tests/tests
# export HAKC_TESTS_PATH_OUT=$HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test
# export HAKC_PYTHON_PATH=$HAKC_ROOT/python
