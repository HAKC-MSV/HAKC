#!/usr/bin/env bash

SUCCESS="++++ SUCCESS ++++"
FAIL="!!!! FAILED !!!!"
WARN="---- WARN ----"

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
    printf '\tBuild: %s\n' "$WARN"
    continue
  fi
  sed -i 's\git@g53gitlab.llan.ll.mit.edu:inherently-secure/\https://github.com/llvm/\g' build/test.bc
  sed -i 's\/data/gitlab-runner/builds/qrXUKGR5/0/inherently-secure/HAKC/HAKC-pass/tests/\/home/de29664/code/HAKC/HAKC-pass/tests/\g' build/test.bc
  diff build/test.bc expected.bc
  error=$?
  if [ $error -eq 0 ]
  then
    printf '\tTest: %s\n' "$SUCCESS"
  else
    if test -f expected_alt.bc; then
      diff build/test.bc expected_alt.bc
      error2=$?
      if [ $error2 -eq 0 ]
      then
        printf '\tTest: %s\n' "$SUCCESS"
        continue
      else
        printf '\tTest: %s\n' "$FAIL"
        exit 1
      fi
    fi
    printf '\tTest: %s\n' "$FAIL"
    exit 1
  fi
done

exit 0
