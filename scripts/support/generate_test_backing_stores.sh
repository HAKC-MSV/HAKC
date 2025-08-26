#!/bin/bash
set -e
source $(git rev-parse --show-toplevel)/scripts/support/vars.sh

cd $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/test

function cleanup(){
  echo "Killing HAKC Static Analysis with pid: $pid"
  kill -9 $pid || true
  # kill off zombie threads
  pkill -f hakc-static-analysis || true
}

function generate_test_backing_store(){
      TEST_NAME=$1
      TEST_FILE_C=$TEST_NAME.c
      SOURCE_TEST_DIR=$HAKC_TEST_ROOT/hakc/tests/$TEST_NAME
      BUILD_TEST_DIR=$HAKC_LLVM_BUILD_PATH/llvm/test/Transforms/Compartmentalization/hakc/tests/$TEST_NAME
      mkdir -p $BUILD_TEST_DIR
      if [[ "$1" == *"temporal"* ]]; then
        echo "TEMPORAL_ANALYSIS Enabled"
        TEMPORAL_ANALYSIS=true
      else
        TEMPORAL_ANALYSIS=false
      fi

      echo "Generating test backing for $TEST_NAME with temporal analysis = $TEMPORAL_ANALYSIS"

      # 1. Emit LLVM
      cmd="$HAKC_CLANG -g -S -emit-llvm -o $BUILD_TEST_DIR/$TEST_NAME.orig.ll -c $SOURCE_TEST_DIR/$TEST_FILE_C"
      echo $cmd
      $cmd

      # 2. Generate DAG db using pass
      # 2.a use sed to get correct paths
      # need to replace pwd and pass mode (same as the test runner is doing) to generate valid yaml config file
      cat $HAKC_TEST_ROOT/hakc/configs/hakc_comp_config.yml.in \
        | sed 's,@BUILD_PATH@,'$BUILD_TEST_DIR/',g' \
        | sed 's,@HAKC_PASS_MODE@,RunDataAccessGraphAnalysis,g' \
        | sed 's,@HAKC_SOCKET_PATH@,'$BUILD_TEST_DIR/hakc_sock/',g' \
        | sed 's,@HAKC_DAG_ANALYSIS_PATH@,'$BUILD_TEST_DIR/dag-analysis/',g' \
        | sed 's,@HAKC_TEMPORAL_ANALYSIS@,'$TEMPORAL_ANALYSIS',g' \
        > $BUILD_TEST_DIR/hakc_dag_config.yml

      cmd="$HAKC_OPT -passes=hakc --hakc-config=$BUILD_TEST_DIR/hakc_dag_config.yml $BUILD_TEST_DIR/$TEST_NAME.orig.ll -o $BUILD_TEST_DIR/$TEST_NAME.orig.ll"
      echo $cmd
      $cmd

      python3 -m venv $(git rev-parse --show-toplevel)/python/venv && source $(git rev-parse --show-toplevel)/python/venv/bin/activate

      # 3. Generate yaml backing store
      # tracking the python pid because it sometimes won't close by itself
      cmd="python3 $HAKC_ROOT/llvm-project/llvm/utils/hakc/hakc-static-analysis --core-count 1\
        --dump-dag $SOURCE_TEST_DIR/db.yml \
        --create-dag --dag-files-root $BUILD_TEST_DIR/dag-analysis/ \
        --db-dir $BUILD_TEST_DIR/hakc-db --log-level=DEBUG"
      echo $cmd
      $cmd &

      pid=$!
      echo "Launched HAKC Static Analysis with pid: $pid"
      # wait for static analysis to finish
      wait $pid
      cleanup

      # save old backing store
      mv $SOURCE_TEST_DIR/db.yml.in $SOURCE_TEST_DIR/old_db.yml.in || true # allow script to continue running if this fails

      # 4. Now replace the build paths with @HAKC_TEST_DIR@
      cat $SOURCE_TEST_DIR/db.yml | sed "s,$SOURCE_TEST_DIR,@HAKC_TEST_DIR@,g" > $SOURCE_TEST_DIR/db.yml.in
      cat $SOURCE_TEST_DIR/db.yml.in
}

function generate_all_test_backing_stores(){
    echo "generate_all_test_backing_stores"
    for path in $HAKC_TEST_ROOT/hakc/tests/*; do
      test_name=$(basename $path)
      if [[ $test_name == *"test"* ]]; then
        echo $test_name
        generate_test_backing_store $test_name
      fi
    done
}

if [[ -n "$1" ]]; then
    trap cleanup SIGINT
    generate_test_backing_store $1
else

  while true; do
      echo -e "Are you surer you want to generate backing stores for all tests? [y/n]=n"
      read -p "" yn
      case $yn in
          [Yy]* ) generate_all_test_backing_stores; break;;
          [Nn]* ) exit;;
          * ) echo "Please answer yes or no.";;
      esac
  done

fi


