#!/bin/bash
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

rm -rf $HAKC_ROOT/llvm-project 
./init.sh
