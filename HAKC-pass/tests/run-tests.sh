#!/usr/bin/env bash

START_DIR=$PWD
REPO_INSTALL_DIR=$(realpath ../../install)

# CLANG_PATH set when running from CI/CD
if [[ -z "${CLANG_PATH}" ]]; then
  CLANG_PATH_FOR_SCRIPT=$(realpath $REPO_INSTALL_DIR/bin/clang)
else
  # use it when set
  CLANG_PATH_FOR_SCRIPT="${CLANG_PATH}/bin/clang"
fi

PASS_PATH=$(realpath $REPO_INSTALL_DIR/lib/libHAKC-Compartmentalizer-linux-x86.so)

echo "CLANG_PATH=$CLANG_PATH_FOR_SCRIPT"
echo "PASS_PATH=$PASS_PATH"

for d in */; do
  cd $(realpath $START_DIR/$d)
  echo "$d"
  if [ -f all-tests.sh ]; then
    env TEST_CLANG=$CLANG_PATH_FOR_SCRIPT TEST_PASS=$PASS_PATH ./all-tests.sh
    if [ $? -eq 1 ];
    then
      exit 1
    fi
  else
    echo "Skipping..."
  fi
done
