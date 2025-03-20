#!/bin/bash


env VERSION_COMMIT=ffc253263a1375a65fa6c9f62a893e9767fbebfa bash \
    $(git rev-parse --show-toplevel)/scripts/patch-generation/generate_patches.sh
