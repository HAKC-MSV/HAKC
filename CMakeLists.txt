cmake_minimum_required(VERSION 3.4.3)

project(HAKC)

if(HAKC_LLVM)
    add_subdirectory(llvm-project/llvm)
else()
    add_subdirectory(HAKC-pass)
endif()