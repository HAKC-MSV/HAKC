cmake_minimum_required(VERSION 3.28)

project(HAKC)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE
            STRING "Build type (default RelWithDebInfo):" FORCE)
endif ()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fdiagnostics-color=always")

if(HAKC_LLVM)
    # Force these values since our pass needs them
    set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra;lld;compiler-rt" CACHE STRING "HAKC Projects")
    set(LLVM_TARGETS_TO_BUILD "X86;AArch64;ARM" CACHE STRING "HAKC Archs")
    set(LLVM_ENABLE_IDE ON)
    set(LLVM_ENABLE_RTTI ON)
    set(LLVM_ENABLE_EH ON)
    add_subdirectory(llvm-project/llvm)
else()
    set(HAKC_PASS_LIB "HAKC-Compartmentalizer" CACHE STRING "HAKC Pass Name")
    set(BUILD_PYTHON True CACHE BOOL "Build Kuzu Python bindings")

    cmake_host_system_information(RESULT Ncpu QUERY NUMBER_OF_PHYSICAL_CORES)

    #===============================================================================
    # 1. VERIFY LLVM INSTALLATION DIR
    #===============================================================================
    # Set this to a valid LLVM installation dir
    if (NOT DEFINED LT_LLVM_INSTALL_DIR)
        set(LT_LLVM_INSTALL_DIR ${CMAKE_INSTALL_PREFIX} CACHE PATH "LLVM installation directory")
    endif (NOT DEFINED LT_LLVM_INSTALL_DIR)

    # A bit of a sanity checking
    set(LT_LLVM_INCLUDE_DIR "${LT_LLVM_INSTALL_DIR}/include/llvm")
    if (NOT EXISTS "${LT_LLVM_INCLUDE_DIR}")
        message(FATAL_ERROR
                " LT_LLVM_INSTALL_DIR (${LT_LLVM_INCLUDE_DIR}) is invalid.")
    endif ()

    set(LT_LLVM_CMAKE_FILE "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/LLVMConfig.cmake")
    if (NOT EXISTS "${LT_LLVM_CMAKE_FILE}")
        message(FATAL_ERROR
                " LT_LLVM_CMAKE_FILE (${LT_LLVM_CMAKE_FILE}) is invalid.")
    endif ()

    #===============================================================================
    # 2. LOAD LLVM CONFIGURATION
    #    For more: http://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
    #===============================================================================
    # Add the location of LLVMConfig.cmake to CMake search paths (so that
    # find_package can locate it)
    list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

    find_package(LLVM REQUIRED CONFIG)

    message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
    message(STATUS "Using LLVMConfig.cmake in: ${LT_LLVM_INSTALL_DIR}")

    include(LLVMConfig)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    add_subdirectory(kuzu)
    add_subdirectory(HAKC-pass)
endif()
