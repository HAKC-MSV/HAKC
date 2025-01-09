export HAKC_LLVM_COMMIT='7ba7d8e2f7b6445b60679da826210cdde29eaf8b'

export HAKC_ROOT=$(git rev-parse --show-toplevel)
export HAKC_INSTALL_PATH=$HAKC_ROOT/install
export HAKC_LLVM_SOURCE_PATH=$HAKC_ROOT/llvm-project
export HAKC_PATCH_PATH=$HAKC_ROOT/patches
export HAKC_LLVM_PATCH_PATH=$HAKC_PATCH_PATH/llvm-patches
export HAKC_LLVM_BUILD_PATH=$HAKC_ROOT/cmake-build-hakc-llvm
export HAKC_CLANG=$HAKC_INSTALL_PATH/bin/clang
export HAKC_SUPPORT_SCRIPTS_PATH=$HAKC_ROOT/scripts/support

exec_cmd_and_check_status () {
  cmd_to_run="$@"
  $cmd_to_run
  exit_status=$?
  if [ $exit_status -ne 0 ]; then
    echo "Error running $cmd_to_run"
    exit $exit_status
  fi
}
