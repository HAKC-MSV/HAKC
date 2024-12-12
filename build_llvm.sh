#/bin/sh
source vars.sh

# rm -rf $HAKC_ROOT/cmake-build-hakc-llvm/
# mkdir $HAKC_ROOT/cmake-build-hakc-llvm/
cd $HAKC_ROOT/cmake-build-hakc-llvm/

# strange, but note that enable_runtimes is set to '' (don't put compiler-rt here)
cmake -G Ninja \
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

cmake --build . -j$(nproc) --target install

