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

set(Python_EXECUTABLE "${HAKC_PYTHON_VENV}/bin/python")


add_subdirectory(llvm-project/llvm)

if (LLVM_PARALLEL_COMPILE_JOBS)
    set(FULL_CORE_COUNT ${LLVM_PARALLEL_COMPILE_JOBS})
else ()
    cmake_host_system_information(RESULT FULL_CORE_COUNT
            QUERY NUMBER_OF_LOGICAL_CORES)
endif ()

math(EXPR HAKC_LINUX_CORE_COUNT "${FULL_CORE_COUNT}")

message("-- HAKC is using up to ${HAKC_LINUX_CORE_COUNT} cores to build Linux")

list(APPEND LINUX_ARCHES_TO_BUILD x86)
list(APPEND LINUX_ARCH_MAKE_OPTS "")
set(HAKC_LINUX_BUILD_PATH ${CMAKE_BINARY_DIR}/linux CACHE PATH "Path to build Linux kernels" FORCE)

# HAKC Constants
set(hakc_configs_path       ${CMAKE_SOURCE_DIR}/llvm-project/llvm/utils/hakc/configs)

# top level HAKC settings
set(hakc_backing_type kuzu)
set(build_modes analysis;enforcement)
set(hakc_client_timeout 100)
set(hakc_max_connection_retries 1)
set(hakc_server_log_level ERROR)

foreach (arch make_opts IN ZIP_LISTS LINUX_ARCHES_TO_BUILD LINUX_ARCH_MAKE_OPTS)
    set(linux_build_dir ${HAKC_LINUX_BUILD_PATH}/${arch})

    foreach (build_mode ${build_modes})

        if (${build_mode} STREQUAL "analysis")
            set(hakc_server_timeout 100)
        else ()
            set(hakc_server_timeout 1000)
        endif ()

        set(LINUX_MAKE_OPTS O=${linux_build_dir}/${build_mode} LLVM=1 ARCH=${arch} CC=$<TARGET_FILE:clang> HOSTCC=$<TARGET_FILE:clang> ${make_opts})
        file(MAKE_DIRECTORY ${linux_build_dir}/${build_mode})

        set(hakc_build_dir ${linux_build_dir})

        set(hakc_socket_dir ${hakc_build_dir}/hakc-sockets)
        string(LENGTH "${hakc_socket_dir}/___" socket_path_length)
        if (socket_path_length GREATER_EQUAL 107)
            message(WARNING "hakc_socket_dir ${hakc_socket_dir} is too long")
        endif ()

        set(hakc_server_config_path ${hakc_build_dir}/${build_mode}/hakc-server-config.yaml)
        set(hakc_client_config_path ${hakc_build_dir}/${build_mode}/hakc-client-config.yaml)
        set(hakc_log_dir ${hakc_build_dir}/${build_mode}/hakc-logs)
        set(hakc_build_mode ${build_mode})
        set(hakc_server_core_count ${HAKC_LINUX_CORE_COUNT})
        set(hakc_backing_path ${hakc_build_dir}/hakc-db)
        set(hakc_adjustments_path ${hakc_build_dir}/${build_mode}/hakc-adjustments.yaml)
        set(hakc_analysis_core_count ${HAKC_LINUX_CORE_COUNT})

        configure_file(                 ${hakc_configs_path}/hakc-client-config.yaml.in
                                        ${hakc_build_dir}/${build_mode}/hakc-client-config.yaml)
        configure_file(                 ${CMAKE_SOURCE_DIR}/configs/hakc-modify-linux-config.sh.in
                                        ${hakc_build_dir}/${build_mode}/hakc-modify-linux-config.sh)
        configure_file(                 ${CMAKE_SOURCE_DIR}/configs/hakc-adjustments.yaml
                                        ${hakc_build_dir}/${build_mode}/hakc-adjustments.yaml)
        configure_file(                 ${hakc_configs_path}/hakc-config.yaml.in
                                        ${hakc_build_dir}/${build_mode}/hakc-config.yaml)
        configure_file(                 ${hakc_configs_path}/hakc-server-config.yaml.in
                                        ${hakc_build_dir}/${build_mode}/hakc-server-config.yaml)

        add_custom_target(hakc-linux-${arch}-${build_mode}-config
                BYPRODUCTS ${hakc_build_dir}/${build_mode}/.config
                DEPENDS ${hakc_configs_path}/hakc-modify-linux-config.sh.in
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Generating ${arch} Linux ${build_mode} config"
                COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} defconfig
        )
        add_custom_target(linux-${arch}-${build_mode}-pass
                BYPRODUCTS ${hakc_build_dir}/${build_mode}/vmlinux
                DEPENDS ${hakc_build_dir}/${build_mode}/hakc-config.yaml clang hakc-linux-${arch}-${build_mode}-config
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Running ${arch} Linux ${build_mode} pass"
                USES_TERMINAL
                COMMAND make clean
                COMMAND bash ${hakc_build_dir}/${build_mode}/hakc-modify-linux-config.sh
                COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
        )
        add_custom_target(linux-${arch}-${build_mode}-compilation-db
                BYPRODUCTS ${hakc_build_dir}/${build_mode}/compile_commands.json
                DEPENDS linux-${arch}-${build_mode}-pass
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Generating ${arch} Linux ${build_mode} compile_commands.json"
                COMMAND ./scripts/gen_compile_commands.py --output ${hakc_build_dir}/${build_mode}/compile_commands.json --directory ${hakc_build_dir}/${build_mode}
        )

    endforeach ()

endforeach ()
