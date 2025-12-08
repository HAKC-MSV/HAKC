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

set(HAKC_PYTHON_Executable "${HAKC_PYTHON_VENV}/bin/python")
set(HAKC_SOCKET_ROOT ${CMAKE_BINARY_DIR}/hakc-sockets CACHE FILEPATH "Path to use for HAKC server sockets")

add_subdirectory(llvm-project/llvm)

if (LLVM_PARALLEL_COMPILE_JOBS)
    set(FULL_CORE_COUNT ${LLVM_PARALLEL_COMPILE_JOBS})
else ()
    cmake_host_system_information(RESULT FULL_CORE_COUNT
            QUERY NUMBER_OF_LOGICAL_CORES)
endif ()

math(EXPR HAKC_LINUX_CORE_COUNT "${FULL_CORE_COUNT}")

message("-- HAKC is using up to ${HAKC_LINUX_CORE_COUNT} cores to build Linux")
message("-- Using ${HAKC_SOCKET_ROOT} for the socket root")

list(APPEND LINUX_ARCHES_TO_BUILD x86)
list(APPEND LINUX_ARCH_MAKE_OPTS "")
set(HAKC_LINUX_BUILD_PATH ${CMAKE_BINARY_DIR}/linux CACHE PATH "Path to build Linux kernels" FORCE)

# HAKC Constants
set(hakc_path ${CMAKE_SOURCE_DIR}/llvm-project/llvm/utils/hakc)
set(hakc_configs_path ${hakc_path}/configs)

# top level HAKC settings
set(hakc_backing_type kuzu)
set(hakc_max_connection_retries 1)
set(hakc_server_log_level ERROR)
set(hakc_server_process ${hakc_path}/hakc-server-process)

foreach (arch make_opts IN ZIP_LISTS LINUX_ARCHES_TO_BUILD LINUX_ARCH_MAKE_OPTS)
    set(hakc_build_dir ${HAKC_LINUX_BUILD_PATH}/${arch})
    set(linux_build_dir ${hakc_build_dir}/build)

    set(LINUX_MAKE_OPTS O=${linux_build_dir} LLVM=1 ARCH=${arch} CC=$<TARGET_FILE:clang> HOSTCC=$<TARGET_FILE:clang> ${make_opts})
    file(MAKE_DIRECTORY ${linux_build_dir})

    set(hakc_socket_dir ${HAKC_SOCKET_ROOT})
    string(LENGTH "${hakc_socket_dir}/___" socket_path_length)
    if (socket_path_length GREATER_EQUAL 107)
        message(WARNING "hakc_socket_dir ${hakc_socket_dir} is too long")
    endif ()

    set(hakc_server_config_path ${hakc_build_dir}/hakc-server-config.yaml)
    set(hakc_client_config_path ${hakc_build_dir}/hakc-client-config.yaml)
    set(compilation_db_path ${hakc_build_dir}/compile_commands.json)
    set(hakc_log_dir ${hakc_build_dir}/hakc-logs)
    set(hakc_server_core_count ${HAKC_LINUX_CORE_COUNT})
    set(hakc_backing_path ${hakc_build_dir}/hakc-db)
    set(hakc_adjustments_path ${hakc_build_dir}/hakc-adjustments.yaml)
    set(hakc_analysis_core_count ${HAKC_LINUX_CORE_COUNT})

    configure_file(${hakc_configs_path}/hakc-client-config.yaml.in
            ${hakc_build_dir}/hakc-client-config.yaml)
    configure_file(${CMAKE_SOURCE_DIR}/configs/hakc-modify-linux-config.sh.in
            ${hakc_build_dir}/hakc-modify-linux-config.sh)
    configure_file(${CMAKE_SOURCE_DIR}/configs/hakc-adjustments.yaml
            ${hakc_build_dir}/hakc-adjustments.yaml)
    configure_file(${hakc_configs_path}/hakc-config.yaml.in
            ${hakc_build_dir}/hakc-config.yaml)
    configure_file(${hakc_configs_path}/hakc-server-config.yaml.in
            ${hakc_build_dir}/hakc-server-config.yaml)

    add_custom_target(hakc-linux-${arch}-config
            BYPRODUCTS ${linux_build_dir}/.config
            DEPENDS ${CMAKE_SOURCE_DIR}/configs/hakc-modify-linux-config.sh.in clang
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Generating ${arch} Linux config"
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} defconfig
            COMMAND bash ${hakc_build_dir}/hakc-modify-linux-config.sh
    )

    add_custom_target(linux-${arch}-compilation-db
            BYPRODUCTS ${compilation_db_path}
            DEPENDS hakc-linux-${arch}-config
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Generating ${compilation_db_path}"
            COMMAND ./scripts/config --file ${linux_build_dir}/.config -d CONFIG_HAKC
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
            COMMAND ./scripts/clang-tools/gen_compile_commands.py --output ${compilation_db_path} --directory ${linux_build_dir}
            COMMAND ./scripts/config --file ${linux_build_dir}/.config -e CONFIG_HAKC
            COMMAND rm -f ${linux_build_dir}/vmlinux
    )

    add_custom_target(hakc-linux-${arch}-analysis
            BYPRODUCTS ${hakc_backing_path}
            DEPENDS ${hakc_build_dir}/hakc-config.yaml clang hakc-linux-${arch}-config linux-${arch}-compilation-db
            WORKING_DIRECTORY ${hakc_build_dir}
            USES_TERMINAL
            COMMENT "Running ${arch} Linux Analysis"
            COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${hakc_path} ${HAKC_PYTHON_Executable}
            ${hakc_server_process} --config ${hakc_build_dir}/hakc-config.yaml --mode analysis
    )

    add_custom_target(hakc-linux-${arch}-enforcement
            BYPRODUCTS ${linux_build_dir}/vmlinux
            DEPENDS hakc-linux-${arch}-analysis
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            USES_TERMINAL
            COMMENT "Running ${arch} Linux Enforcement"
            COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${hakc_path} ${HAKC_PYTHON_Executable}
            ${hakc_server_process} --config ${hakc_build_dir}/hakc-config.yaml --mode enforcement
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
    )

endforeach ()
