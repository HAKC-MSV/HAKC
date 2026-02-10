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

set(BUSYBOX_EXECUTABLE "" CACHE FILEPATH "Path to busybox binary")

if (NOT BUSYBOX_EXECUTABLE)
    find_program(BUSYBOX_EXECUTABLE busybox)

    if (NOT BUSYBOX_EXECUTABLE AND EXISTS "/usr/bin/busybox")
        set(BUSYBOX_EXECUTABLE "/usr/bin/busybox" CACHE FILEPATH "Path to busybox binary" FORCE)
    endif ()
endif ()

if (NOT BUSYBOX_EXECUTABLE)
    message(WARNING "BusyBox not found. Set BUSYBOX_EXECUTABLE=/usr/bin/busybox or another path.")
    set(BUSYBOX_EXECUTABLE "busybox" CACHE FILEPATH "Path to busybox binary" FORCE)
else ()
    message(STATUS "Using BusyBox at: ${BUSYBOX_EXECUTABLE}")
endif ()

add_subdirectory(llvm-project/llvm)

# If user did not set HAKC_CC, default to the built clang target.
# We'll also record whether we're using the default so we can conditionally add deps.
set(HAKC_CC_IS_DEFAULT FALSE)

if (NOT DEFINED HAKC_CC OR HAKC_CC STREQUAL "")
    set(HAKC_CC_IS_DEFAULT TRUE)
    # Use a real path (not a generator expr) for custom commands.
    # TARGET_FILE is evaluated at generate/build time, so we keep it as a generator expression,
    # but DO NOT store it in the cache.
    set(HAKC_CC "$<TARGET_FILE:clang>")
else ()
    # User provided a compiler path; treat as non-default.
    set(HAKC_CC_IS_DEFAULT FALSE)
endif ()

# Cache the user-facing value only if it was user-provided, or store a readable default string.
# (Avoid caching raw generator expressions if you can.)
set(HAKC_CC "${HAKC_CC}" CACHE FILEPATH "Path to Clang used for Linux builds")

set(_hakc_clang_dep "")
if (HAKC_CC_IS_DEFAULT)
    set(_hakc_clang_dep clang)
endif ()

set(HAKC_ANALYSIS_CORES CACHE STRING "Number of cores to use for analysis")
if (HAKC_ANALYSIS_CORES)
    set(FULL_CORE_COUNT ${HAKC_ANALYSIS_CORES})
else ()
    cmake_host_system_information(RESULT FULL_CORE_COUNT
            QUERY NUMBER_OF_LOGICAL_CORES)
endif ()

math(EXPR HAKC_LINUX_CORE_COUNT "${FULL_CORE_COUNT}")

message(STATUS "HAKC is using up to ${HAKC_LINUX_CORE_COUNT} cores to build Linux")
message(STATUS "Using ${HAKC_SOCKET_ROOT} for the socket root")

list(APPEND LINUX_ARCHES_TO_BUILD x86_64)
list(APPEND LINUX_ARCH_MAKE_OPTS "")
set(HAKC_LINUX_BUILD_PATH ${CMAKE_BINARY_DIR}/linux CACHE PATH "Path to build Linux kernels")

# HAKC Constants
set(hakc_path "${CMAKE_CURRENT_LIST_DIR}/llvm-project/llvm/utils/hakc")
set(hakc_configs_path "${hakc_path}/configs")

set(HAKC_SERVER_PROCESS_PATH "${hakc_path}/hakc-server-process"
        CACHE FILEPATH "Path to HAKC Server" FORCE)

message(STATUS "Using HAKC_SERVER_PROCESS_PATH=${HAKC_SERVER_PROCESS_PATH}")

# top level HAKC settings
set(hakc_backing_type kuzu)
set(hakc_max_connection_retries 1)
set(hakc_server_log_level ERROR)
set(hakc_kernel_ext "-hakc")
set(hakc_kernel_version "6.14.0")
set(hakc_kernel_name "vmlinuz-${hakc_kernel_version}${hakc_kernel_ext}")

foreach (arch make_opts IN ZIP_LISTS LINUX_ARCHES_TO_BUILD LINUX_ARCH_MAKE_OPTS)
    set(hakc_build_dir ${HAKC_LINUX_BUILD_PATH}/${arch})
    set(linux_build_dir ${hakc_build_dir}/build)
    set(hardened_kernel_install_path ${hakc_build_dir}/hakc-kernel)
    set(kernel_config_path ${linux_build_dir}/.config)

    set(LINUX_MAKE_OPTS
            O=${linux_build_dir}
            LOCALVERSION=${hakc_kernel_ext}
            LLVM=1
            ARCH=${arch}
            "CC=${HAKC_CC}"
            "HOSTCC=${HAKC_CC}"
            ${make_opts}
    )
    file(MAKE_DIRECTORY ${linux_build_dir})

    set(hakc_socket_dir ${HAKC_SOCKET_ROOT})
    string(LENGTH "${hakc_socket_dir}/___" socket_path_length)
    if (socket_path_length GREATER_EQUAL 107)
        message(WARNING "hakc_socket_dir ${hakc_socket_dir} is too long")
    endif ()

    set(hakc_main_config_path ${hakc_build_dir}/hakc-config.yaml)
    set(hakc_server_config_path ${hakc_build_dir}/hakc-server-config.yaml)
    set(hakc_client_config_path ${hakc_build_dir}/hakc-client-config.yaml)
    set(compilation_db_path ${hakc_build_dir}/compile_commands.json)
    set(hakc_log_dir ${hakc_build_dir}/hakc-logs)
    set(hakc_server_core_count ${HAKC_LINUX_CORE_COUNT})
    set(hakc_backing_path ${hakc_build_dir}/hakc-db)
    set(hakc_adjustments_path ${hakc_build_dir}/hakc-adjustments.yaml)
    set(hakc_analysis_core_count ${HAKC_LINUX_CORE_COUNT})

    set(ROOTFS_DIR ${hakc_build_dir}/rootfs)
    set(INITRAMFS_IMG ${hakc_build_dir}/rootfs.cpio.gz)

    configure_file(${hakc_configs_path}/hakc-client-config.yaml.in
            ${hakc_build_dir}/hakc-client-config.yaml)
    configure_file(${CMAKE_SOURCE_DIR}/configs/hakc-modify-linux-config.sh.in
            ${hakc_build_dir}/hakc-modify-linux-config.sh)
    configure_file(${CMAKE_SOURCE_DIR}/configs/hakc-adjustments.yaml
            ${hakc_build_dir}/hakc-adjustments.yaml)
    configure_file(${hakc_configs_path}/hakc-config.yaml.in
            ${hakc_main_config_path})
    configure_file(${hakc_configs_path}/hakc-server-config.yaml.in
            ${hakc_build_dir}/hakc-server-config.yaml)

    add_custom_command(
            OUTPUT ${kernel_config_path}
            DEPENDS ${CMAKE_SOURCE_DIR}/configs/hakc-modify-linux-config.sh.in
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Generating ${arch} Linux config"
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} defconfig
            COMMAND bash ${hakc_build_dir}/hakc-modify-linux-config.sh
    )

    add_custom_target(hakc-linux-${arch}-config
            DEPENDS ${kernel_config_path}
    )

    add_custom_command(
            OUTPUT ${compilation_db_path}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            DEPENDS hakc-linux-${arch}-config ${_hakc_clang_dep}
            COMMENT "Generating ${compilation_db_path}"
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} clean
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
            COMMAND ${CMAKE_SOURCE_DIR}/linux/scripts/clang-tools/gen_compile_commands.py -d ${linux_build_dir} -o ${compilation_db_path}
    )

    add_custom_target(linux-${arch}-compilation-db
            DEPENDS ${compilation_db_path} hakc-linux-${arch}-config ${_hakc_clang_dep}
    )

    add_custom_command(
            OUTPUT ${hakc_backing_path}
            WORKING_DIRECTORY ${hakc_build_dir}
            DEPENDS ${_hakc_clang_dep}
            USES_TERMINAL
            COMMENT "Running ${arch} Linux Analysis"
            COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${hakc_path} ${HAKC_PYTHON_Executable}
            ${HAKC_SERVER_PROCESS_PATH} --config ${hakc_main_config_path} --mode analysis
    )

    add_custom_target(hakc-linux-${arch}-analysis
            DEPENDS ${hakc_main_config_path} ${_hakc_clang_dep} hakc-linux-${arch}-config linux-${arch}-compilation-db ${hakc_backing_path}
    )

    add_custom_command(
            OUTPUT ${hardened_kernel_install_path}/${hakc_kernel_name}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            DEPENDS linux-${arch}-compilation-db ${_hakc_clang_dep}
            USES_TERMINAL
            COMMENT "Building the ${arch} HAKC Linux kernel"
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} modules_prepare scripts
            COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${hakc_path} ${HAKC_PYTHON_Executable}
            ${HAKC_SERVER_PROCESS_PATH} --config ${hakc_main_config_path} --mode enforcement
            COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
            COMMAND make INSTALL_PATH=${hardened_kernel_install_path} INSTALL_MOD_PATH=${hardened_kernel_install_path} ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} install modules_install
            COMMAND rm -rf ${hardened_kernel_install_path}/lib/modules/${hakc_kernel_version}${hakc_kernel_ext}/build
    )

    add_custom_target(hakc-linux-${arch}-enforcement
            DEPENDS linux-${arch}-compilation-db ${hardened_kernel_install_path}/${hakc_kernel_name} ${_hakc_clang_dep}
    )

    add_custom_command(
            OUTPUT ${INITRAMFS_IMG}
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${ROOTFS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory
            ${ROOTFS_DIR}
            ${ROOTFS_DIR}/bin
            ${ROOTFS_DIR}/sbin
            ${ROOTFS_DIR}/proc
            ${ROOTFS_DIR}/sys
            ${ROOTFS_DIR}/dev
            ${ROOTFS_DIR}/etc
            ${ROOTFS_DIR}/lib
            COMMAND ${CMAKE_COMMAND} -E copy ${BUSYBOX_EXECUTABLE} ${ROOTFS_DIR}/bin/busybox
            COMMAND chmod 755 ${ROOTFS_DIR}/bin/busybox

            # Install BusyBox applets as symlinks in /bin
            COMMAND ${CMAKE_COMMAND} -E chdir ${ROOTFS_DIR}/bin ./busybox --install -s .
            COMMAND ${CMAKE_COMMAND} -E chdir ${ROOTFS_DIR}/bin sh -c "find . -type l | xargs -I{} ln -sf ./busybox {}"

            # Create a minimal init script
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/scripts/kernel-userspace-scripts/init ${ROOTFS_DIR}
            COMMAND chmod +x ${ROOTFS_DIR}/init

            COMMAND ${CMAKE_COMMAND} -E copy_directory ${hardened_kernel_install_path}/lib/modules ${ROOTFS_DIR}/lib/modules


            COMMAND ${CMAKE_COMMAND} -E chdir ${ROOTFS_DIR}
            sh -c "find . | cpio -H newc -o | gzip -9 > ${INITRAMFS_IMG}"

            DEPENDS ${BUSYBOX_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/kernel-userspace-scripts/init ${hardened_kernel_install_path}/${hakc_kernel_name}
            WORKING_DIRECTORY ${hakc_build_dir}
            VERBATIM
    )

    # High-level target that always builds the initramfs
    add_custom_target(hakc-${arch}-userspace
            DEPENDS ${INITRAMFS_IMG} hakc-linux-${arch}-enforcement
    )

    add_custom_target(run-${arch}-hakc-kernel
            COMMAND qemu-system-${arch} -kernel ${hardened_kernel_install_path}/${hakc_kernel_name} -initrd ${INITRAMFS_IMG} -m 16G -append \"console=ttyS0 rdinit=/init\" -nographic
            DEPENDS hakc-linux-${arch}-enforcement hakc-${arch}-userspace
            USES_TERMINAL
            WORKING_DIRECTORY ${hakc_build_dir}
    )

endforeach ()
