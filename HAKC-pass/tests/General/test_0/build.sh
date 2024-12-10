#/bin/sh
export ROOT=$(git rev-parse --show-toplevel)
export TEST_PASS=$ROOT/install/lib/HAKC-Compartmentalizer.so
export HAKC_DAG_ROOT=$ROOT/HAKC-pass/tests/General/test_0/dag_analysis/_HAKC_BUILD_PATH_
mkdir -p build


$ROOT/install/bin/clang \
  -fpass-plugin=$TEST_PASS \
  -Xclang -load -Xclang $TEST_PASS \
  -mllvm -HAKC_CONFIG=test-0.yml \
  -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c

# to create dag: 
# python3 $ROOT/python/analysis/hakc-dag.py --db-dir hakc-dag-analysis/hakc-db --dag-files-root $HAKC_DAG_ROOT --core-count 1

# $ROOT/install/bin/clang -fpass-plugin=$TEST_PASS -Xclang -load -Xclang $TEST_PASS -mllvm -HAKC_CONFIG=test-0_comp.yml -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c

