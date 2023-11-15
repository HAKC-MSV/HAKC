#!/usr/bin/env bash

START_DIR=$PWD
REPO_INSTALL_DIR=$(realpath ../../install)
CLANG_PATH=$(realpath $REPO_INSTALL_DIR/bin/clang)
PASS_PATH=$(realpath $REPO_INSTALL_DIR/lib/libHAKC-Compartmentalizer-linux-x86.so)

echo "CLANG_PATH=$CLANG_PATH"
echo "PASS_PATH=$PASS_PATH"

for d in */; do
  cd $(realpath $START_DIR/$d)
  echo "$d"
  if [ -f all-tests.sh ]; then
    env TEST_CLANG=$CLANG_PATH TEST_PASS=$PASS_PATH ./all-tests.sh
  else
    echo "Skipping..."
  fi
done
