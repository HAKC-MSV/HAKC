cmake_minimum_required(VERSION 3.28)

project(HAKC)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE
            STRING "Build type (default RelWithDebInfo):" FORCE)
endif ()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fdiagnostics-color=always")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# need to make some changes here, since the HAKC pass will now be built alongside llvm 
if (LLVM_ENABLE_HAKC)
    # Force these values since our pass needs them
    set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra;lld;compiler-rt" CACHE STRING "HAKC Projects")
    set(LLVM_TARGETS_TO_BUILD "X86;AArch64;ARM" CACHE STRING "HAKC Archs")
    set(LLVM_ENABLE_IDE ON)
    set(LLVM_ENABLE_RTTI ON)
    set(LLVM_ENABLE_EH ON)
    set(HAKC_PYTHON_VENV ${CMAKE_SOURCE_DIR}/python/venv)
    add_subdirectory(llvm-project/llvm)
endif ()
