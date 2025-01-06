cmake_minimum_required(VERSION 3.4.3)
project(HAKC-Pass VERSION 2.0.0)

# Set the LLVM header and library paths
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})
add_definitions(${LLVM_DEFINITIONS})

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# LLVM is normally built without RTTI. Be consistent with that.
if (NOT LLVM_ENABLE_RTTI)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif ()

# Set the build directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${HAKC_PASS_LIB}/lib")

set(HAKC_SOURCES "")
add_subdirectory(src)

message(STATUS "HAKC_SOURCES ${HAKC_SOURCES}")

add_llvm_library(${HAKC_PASS_LIB} MODULE ${HAKC_SOURCES})
set_target_properties(${HAKC_PASS_LIB} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${HAKC_PASS_LIB} PRIVATE inc ${PROJECT_SOURCE_DIR}/kuzu/src/include)

target_link_libraries(${HAKC_PASS_LIB} kuzu)
