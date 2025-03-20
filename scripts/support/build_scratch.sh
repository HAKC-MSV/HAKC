#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

$HAKC_SUPPORT_SCRIPTS_PATH/init.sh # initialize git submodules
$HAKC_SUPPORT_SCRIPTS_PATH/copy_files.sh # copy over HAKC pass files to llvm-project
$HAKC_SUPPORT_SCRIPTS_PATH/apply_patches.sh # apply the required for HAKC pass patches to llvm-project
$HAKC_SUPPORT_SCRIPTS_PATH/build_llvm.sh # build HAKC pass, tests, and llvm-project together
$HAKC_SUPPORT_SCRIPTS_PATH/run_tests.sh # execute HAKC tests to ensure that build succeeded
