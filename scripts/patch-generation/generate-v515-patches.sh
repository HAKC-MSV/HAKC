#!/bin/bash

env VERSION_COMMIT=8bb7eca972ad531c9b149c0a51ab43a417385813 bash \
  $(git rev-parse --show-toplevel)/scripts/patch-generation/generate_patches.sh
