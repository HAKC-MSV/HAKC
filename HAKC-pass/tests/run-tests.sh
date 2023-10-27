#!/usr/bin/env bash

START_DIR=$PWD
for d in */; do
  cd $(realpath $START_DIR/$d)
  echo "$d"
  ./all-tests.sh
done
