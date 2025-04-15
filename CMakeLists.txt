cmake_minimum_required(VERSION 3.28)

project(HAKC)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fdiagnostics-color=always")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(LLVM_ENABLE_HAKC ON)
set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra;lld;compiler-rt" CACHE STRING "HAKC Projects")
set(LLVM_TARGETS_TO_BUILD "X86;AArch64;ARM" CACHE STRING "HAKC Archs")
set(LLVM_ENABLE_IDE ON)
set(LLVM_ENABLE_RTTI ON)
set(LLVM_ENABLE_EH ON)
set(HAKC_PYTHON_VENV ${CMAKE_SOURCE_DIR}/python/venv)

add_subdirectory(llvm-project/llvm)

if (LLVM_PARALLEL_COMPILE_JOBS)
    set(HAKC_LINUX_CORE_COUNT ${LLVM_PARALLEL_COMPILE_JOBS})
else ()
    cmake_host_system_information(RESULT HAKC_LINUX_CORE_COUNT
            QUERY NUMBER_OF_LOGICAL_CORES)
endif ()

message("-- HAKC is using ${HAKC_LINUX_CORE_COUNT} cores to build Linux")

list(APPEND LINUX_ARCHES_TO_BUILD x86)
list(APPEND LINUX_ARCH_MAKE_OPTS "")

foreach (arch make_opts IN ZIP_LISTS LINUX_ARCHES_TO_BUILD LINUX_ARCH_MAKE_OPTS)
    set(linux_build_path ${HAKC_LINUX_BUILD_PATH}/${arch})
    file(MAKE_DIRECTORY ${linux_build_path}/analysis ${linux_build_path}/hakc-dag-analysis)
    configure_file(${CMAKE_SOURCE_DIR}/configs/compartmentalizations/linux/${arch}/analysis-config.yaml.in
            ${linux_build_path}/analysis-config.yaml
    )
    add_custom_target(linux-${arch}-analysis-pass
            BYPRODUCTS ${linux_build_path}/analysis/vmlinux
            DEPENDS ${linux_build_path}/analysis-config.yaml clang
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Running ${arch} Linux analysis pass"
            USES_TERMINAL
            COMMAND make O=${linux_build_path}/analysis LLVM=1 ARCH=${arch} CC=$<TARGET_FILE:clang> HOSTCC=$<TARGET_FILE:clang> ${make_opts} -j${HAKC_LINUX_CORE_COUNT}
    )
    add_custom_target(linux-${arch}-compilation-db
            BYPRODUCTS ${linux_build_path}/compile_commands.json
            DEPENDS linux-${arch}-analysis-pass
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Generating ${arch} Linux compile_commands.json"
            COMMAND ./scripts/gen_compile_commands.py --output ${linux_build_path}/compile_commands.json --directory ${linux_build_path}/analysis
    )
endforeach ()
