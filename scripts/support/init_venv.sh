#!/bin/bash

source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

python3 -m venv $HAKC_ROOT/python/venv
source $HAKC_ROOT/python/venv/bin/activate
pip install -r $HAKC_ROOT/llvm-project/llvm/utils/hakc/requirements.txt
