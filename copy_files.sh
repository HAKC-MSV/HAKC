#/bin/sh
# clear old files and copy hakc source files to correct destination 
source vars.sh

cd $HAKC_LLVM_BUILD_PATH

rm -rf $HAKC_LLVM_BUILD_PATH/llvm-project/llvm/projects/compiler-rt/test/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

# copy over libraries and tests 
cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/lib/* $HAKC_ROOT/llvm-project/compiler-rt/lib/
cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/test/* $HAKC_ROOT/llvm-project/compiler-rt/test/

rm -rf $HAKC_ROOT/llvm-project/llvm/include/hakc
rm -rf $HAKC_ROOT/llvm-project/llvm/lib/hakc

# copy over hakc includes and source code to be built with llvm 
cp -r $HAKC_ROOT/HAKC-pass/HAKC-pass-old/inc* $HAKC_ROOT/llvm-project/llvm/include/hakc
cp -r $HAKC_ROOT/HAKC-pass/HAKC-pass-old/src* $HAKC_ROOT/llvm-project/llvm/lib/hakc
