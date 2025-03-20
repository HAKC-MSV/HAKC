# This lists the options that should be common with all builds
# What is missing is CMAKE_INSTALL_DIR CMAKE_BUILD_TYPE
-DLLVM_ENABLE_PROJECTS='clang;clang-tools-extra;lld'
-DCMAKE_C_COMPILER=clang
-DCMAKE_CXX_COMPILER=clang++
-DLLVM_TARGETS_TO_BUILD='X86;AArch64;ARM'
-DLLVM_USE_LINKER=lld
-DLLVM_ENABLE_IDE=On
-DLLVM_ENABLE_HAKC=On
-DLLVM_BUILD_TESTS=On
