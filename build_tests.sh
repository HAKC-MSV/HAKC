#/bin/sh
source vars.sh

cd $HAKC_ROOT/cmake-build-hakc-llvm

rm -rf $HAKC_ROOT/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
rm -rf $HAKC_ROOT/llvm-project/compiler-rt/test/hakc

cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/lib/* $HAKC_ROOT/llvm-project/compiler-rt/lib/hakc
cp -r $HAKC_ROOT/HAKC-pass/tests/compiler-rt/test/* $HAKC_ROOT/llvm-project/compiler-rt/test/hakc


# don't try to do a standalone build, it won't work probably 
# command to build llvm
# the arguments to enable the tests are as follows: 
# -DCOMPILER_RT_BUILD_HAKC=ON \
# -DCOMPILER_RT_INCLUDE_TESTS=ON \
cmake --fresh -G Ninja \
-DLLVM_ENABLE_PROJECTS='compiler-rt;clang;clang-tools-extra;lld' \
-DLLVM_ENABLE_RUNTIMES='' \
-DCMAKE_INSTALL_PREFIX=$(realpath ..)/install \
-DCMAKE_BUILD_TYPE=RelWithDebInfo \
-DCMAKE_C_COMPILER=/usr/bin/clang-18 \
-DCMAKE_CXX_COMPILER=/usr/bin/clang++-18 \
-DLLVM_TARGETS_TO_BUILD='X86;AArch64;ARM' \
-DLLVM_USE_LINKER=lld \
-DLLVM_ENABLE_IDE=True \
-DCOMPILER_RT_BUILD_HAKC=ON \
-DCOMPILER_RT_INCLUDE_TESTS=ON \
-DHAKC_LLVM=True ..  

# ninja 
cmake --build . -j$(nproc) --target install

# check that the hakc folder was actually created 
# ls $HAKC_ROOT/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test
# ls $HAKC_ROOT/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig/TestCases/Posix/Output

cd $HAKC_ROOT/cmake-build-hakc-llvm/llvm-project/llvm/projects/compiler-rt/test/hakc/X86_64LinuxConfig
llvm-lit .
