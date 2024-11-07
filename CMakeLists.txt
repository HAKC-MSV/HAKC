cmake_minimum_required(VERSION 3.28)

project(HAKC)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

if(HAKC_LLVM)
    set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra;lld" CACHE STRING "HAKC Projects")
    set(LLVM_ENABLE_RUNTIMES "compiler-rt" CACHE STRING "HAKC Runtimes")
    set(LLVM_TARGETS_TO_BUILD "X86;AArch64;ARM" CACHE STRING "HAKC Archs")
    set(LLVM_ENABLE_IDE True CACHE BOOL "HAKC Enable IDE")
    add_subdirectory(llvm-project/llvm)
else()
    set(HAKC_PASS_LIB "HAKC-Compartmentalizer")

    cmake_host_system_information(RESULT Ncpu QUERY NUMBER_OF_PHYSICAL_CORES)

    add_subdirectory(kuzu)
    add_subdirectory(HAKC-pass)
endif()
