#!/bin/bash

source vars.sh

echo "Updating submodules"
exec_cmd_and_check_status "git submodule update --init --recursive"
echo "Creating Directories"
exec_cmd_and_check_status "mkdir -p $HAKC_INSTALL_PATH $HAKC_LLVM_PATCH_PATH $HAKC_LLVM_BUILD_PATH $HAKC_INSTALL_PATH"
#echo "Applying Patches"
#exec_cmd_and_check_status "bash apply_patches.sh"
echo "Installing Hooks"
for hook in $(ls $HAKC_ROOT/git_hooks); do
  exec_cmd_and_check_status ln -s $HAKC_ROOT/git_hooks/$hook $HAKC_ROOT/.git/hooks/$hook
done
