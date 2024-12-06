#/bin/sh

mkdir -p build

CONFIG_FILE=config.yaml
CONFIG_FILE_IN=$CONFIG_FILE.in

SED_REPLACE="s,@PWD@,$PWD,g"

cat $CONFIG_FILE_IN | sed $SED_REPLACE > $CONFIG_FILE

$TEST_CLANG \
  -Xclang -load -Xclang $TEST_PASS \
  -fpass-plugin=$TEST_PASS \
  -mllvm --HAKC_CONFIG=$CONFIG_FILE \
  -g -S -emit-llvm -O2 -o build/test.bc -c test-0.c
