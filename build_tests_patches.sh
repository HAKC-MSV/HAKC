#/bin/sh
source vars.sh
# git diff --output config-ix.cmake.patch config-ix.cmake

cd $HAKC_ROOT/llvm-project 

# apply patches to llvm-project/compiler-rt 
# git apply -R patch to revert the patch 
git apply $HAKC_ROOT/HAKC-pass/tests/patches/llvm-project_compiler-rt_cmake_config-ix.cmake.patch
git apply $HAKC_ROOT/HAKC-pass/tests/patches/llvm-project_compiler-rt_CMakeLists.txt.patch
git apply $HAKC_ROOT/HAKC-pass/tests/patches/llvm-project_compiler-rt_cmake_Modules_AllSupportedArchDefs.cmake.patch
git apply $HAKC_ROOT/HAKC-pass/tests/patches/llvm-project_compiler-rt_test_CMakeLists.txt.patch
