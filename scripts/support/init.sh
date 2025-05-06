#!/bin/bash

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

source $HAKC_ROOT/scripts/support/init_venv.sh

echo "Updating submodules"
exec_cmd_and_check_status "git submodule update --init --recursive"
echo "Creating Directories"
exec_cmd_and_check_status "mkdir -p $HAKC_LLVM_BUILD_PATH $HAKC_INSTALL_PATH"
