#!/usr/bin/env bash

SUCCESS="++++ SUCCESS ++++"
FAIL="!!!! FAILED !!!!"

START_DIR=$PWD
for d in */; do
  cd $(realpath $START_DIR/$d)
  rm -rf build/
  echo "$d"
  ./build.sh
  if [ $? -eq 0 ]
  then
    printf '\tBuild: %s\n' "$SUCCESS"
  else
    printf '\tBuild: %s\n' "$FAIL"
    continue
  fi
  diff build/test.bc expected.bc
  error=$?
  if [ $error -eq 0 ]
  then
    printf '\tTest: %s\n' "$SUCCESS"
  else
    printf '\tTest: %s\n' "$FAIL"
  fi
done
