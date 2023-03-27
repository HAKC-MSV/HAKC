cmake -G Ninja \
  -DLLVM_ENABLE_PROJECTS='clang;lld;clang-tools-extra;llvm' \
  -DCMAKE_INSTALL_PREFIX=$PWD/install \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/clang \
  -DCMAKE_CXX_COMPILER=/usr/bin/clang++ \
  -DLLVM_TARGETS_TO_BUILD='X86;AArch64' \
  -DLLVM_OPTIMIZED_TABLEGEN=yes \
  -DLLVM_USE_LINKER=lld \
  -DLLVM_ENABLE_IDE=True \
  ../llvm

#  -DLLVM_ENABLE_RUNTIMES=compiler-rt \
