#!/bin/bash

echo "Initializing in $PWD"
source scripts/support/vars.sh

echo "Creating Directories"
exec_cmd_and_check_status "mkdir -p $HAKC_LLVM_BUILD_PATH $HAKC_INSTALL_PATH"
