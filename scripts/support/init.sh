#!/bin/bash

echo "Initializing in $PWD"
source scripts/support/vars.sh

if [[ ! -d "llvm_project" ]] || [[ ! "$(ls -A "llvm_project" 2>/dev/null)" ]]; then
  git submodule update --init --recursive
fi

echo "Creating Directories"
exec_cmd_and_check_status "mkdir -p $HAKC_LLVM_BUILD_PATH $HAKC_INSTALL_PATH"
