#!/bin/bash
# clear old files and copy hakc source files to correct destination 
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

exec_cmd_and_check_status "rm -rf $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc"
exec_cmd_and_check_status "rm -rf $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc"
exec_cmd_and_check_status "rm -rf $HAKC_ROOT/llvm-project/compiler-rt/test/hakc"

# copy over libraries and tests 
exec_cmd_and_check_status "cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/lib/* $HAKC_ROOT/llvm-project/compiler-rt/lib/"
exec_cmd_and_check_status "cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/test/* $HAKC_ROOT/llvm-project/compiler-rt/test/"

exec_cmd_and_check_status "rm -rf $HAKC_ROOT/llvm-project/llvm/include/hakc"
exec_cmd_and_check_status "rm -rf $HAKC_ROOT/llvm-project/llvm/lib/hakc"

# copy over hakc includes and source code to be built with llvm 
exec_cmd_and_check_status "cp -r $HAKC_ROOT/HAKC-pass/inc* $HAKC_ROOT/llvm-project/llvm/include/hakc"
exec_cmd_and_check_status "cp -r $HAKC_ROOT/HAKC-pass/src* $HAKC_ROOT/llvm-project/llvm/lib/hakc"
