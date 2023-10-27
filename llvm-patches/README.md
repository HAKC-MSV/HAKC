# Add kernel_user_ptr attribute in clang/LLVM
1. `git clone git@github.com:llvm/llvm-project.git`
2. `cd llvm-project`
3. `git checkout release/12.x`
4. `mkdir -p build-12.x/install`
5. `git apply /path/to/this/repo/HAKC-Annotator/*.patch`
6. `cd build-12.x; cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$(realpath install) -DLLVM_ENABLE_RUNTIMES="compiler-rt" -DLLVM_ENABLE_PROJECTS="clang;lld;clang-tool-extras" -DLLVM_PARALLEL_LINK_JOBS=8 -DLLVM_ENABLE_IDE=True -DLLVM_TARGETS_TO_BUILD="AArch64;WebAssembly;X86" -G Ninja ../llvm`
7. `cmake --build . -j$(nproc) -t install`
8. Build the HAKC-Compartmentalizer using `build-12.x/install/bin/clang-12`
9. Build the kernel using `build-12.x/install/bin/clang-12`
