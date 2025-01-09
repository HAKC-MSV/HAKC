#!/bin/bash
# to generate patches and ignore certain directories
# current hash is for llvm-project 19. TODO: make this dynamically grab the submodule hash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

git_patch='git apply'

cd $HAKC_ROOT/llvm-project

# if first command argument is -R, then revert the patches
if [ "$1" == "-R" ]; then
  git_patch="$git_patch -R"
fi

for patch in $(ls $HAKC_LLVM_PATCH_PATH/*.hakc.patch); do
  cmd_to_run="$git_patch $patch"
  echo "Applying patch $patch"
  exec_cmd_and_check_status "$cmd_to_run"
done
