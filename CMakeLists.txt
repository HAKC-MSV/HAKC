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
option(HAKC_USE_PREBUILT_DB "Use a prebuilt/downloaded HAKC database instead of running analysis" OFF)

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
    set(db_dependency_file ${hakc_backing_path}/data.kz)
    set(hakc_adjustments_path ${hakc_build_dir}/hakc-adjustments.yaml)
    set(hakc_analysis_core_count ${HAKC_LINUX_CORE_COUNT})

    set(ROOTFS_DIR ${hakc_build_dir}/rootfs)
    set(USERSPACE_IMG ${hakc_build_dir}/rootfs.img)

    message(STATUS "db_dependency_file is ${db_dependency_file}")

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

    if (NOT HAKC_USE_PREBUILT_DB)
        add_custom_command(
                OUTPUT ${db_dependency_file}
                WORKING_DIRECTORY ${hakc_build_dir}
                DEPENDS ${_hakc_clang_dep} linux-${arch}-compilation-db
                USES_TERMINAL
                COMMENT "Running ${arch} Linux Analysis"
                COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${hakc_path} ${HAKC_PYTHON_Executable}
                ${HAKC_SERVER_PROCESS_PATH} --config ${hakc_main_config_path} --mode analysis
        )

        add_custom_target(hakc-linux-${arch}-analysis
                DEPENDS ${hakc_main_config_path} ${_hakc_clang_dep} hakc-linux-${arch}-config linux-${arch}-compilation-db ${db_dependency_file}
        )
    else ()
        # In prebuilt mode: no build rule exists for the DB; it must be present at build time.
        add_custom_target(hakc-linux-${arch}-analysis
                COMMAND ${CMAKE_COMMAND} -E echo "HAKC_USE_PREBUILT_DB=ON: skipping analysis; expecting ${db_dependency_file} to exist."
        )
    endif ()

    add_custom_command(
            OUTPUT ${hardened_kernel_install_path}/${hakc_kernel_name}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            DEPENDS linux-${arch}-compilation-db ${_hakc_clang_dep} ${db_dependency_file}
            USES_TERMINAL
            COMMENT "Building the ${arch} HAKC Linux kernel"
            COMMAND ${CMAKE_COMMAND} -E echo "Checking for prebuilt DB: ${db_dependency_file}"
            COMMAND test -f ${db_dependency_file}
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
            OUTPUT ${USERSPACE_IMG}
            COMMAND ${CMAKE_COMMAND} -E rm -rf ${ROOTFS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory
            ${ROOTFS_DIR}
            ${ROOTFS_DIR}/etc/ros2-demo
            ${ROOTFS_DIR}/proc
            ${ROOTFS_DIR}/sys
            ${ROOTFS_DIR}/dev
            ${ROOTFS_DIR}/tmp
            ${ROOTFS_DIR}/var
            ${ROOTFS_DIR}/root
            ${ROOTFS_DIR}/home
            ${ROOTFS_DIR}/opt
            ${ROOTFS_DIR}/lib
            ${ROOTFS_DIR}/lib64

            COMMAND ${CMAKE_COMMAND} -E copy ${BUSYBOX_EXECUTABLE} ${ROOTFS_DIR}/bin/busybox
            COMMAND chmod 755 ${ROOTFS_DIR}/bin/busybox

            # Install BusyBox applets as symlinks in /bin
            COMMAND ${CMAKE_COMMAND} -E chdir ${ROOTFS_DIR}/bin ./busybox --install -s .
            COMMAND ${CMAKE_COMMAND} -E chdir ${ROOTFS_DIR}/bin sh -c "find . -type l | xargs -I{} ln -sf ./busybox {}"

            # Copy the rosdemo code
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/scripts/ros2-demo ${ROOTFS_DIR}/etc/ros2-demo
            COMMAND chmod +x ${ROOTFS_DIR}/etc/ros2-demo/run-rosdemo.sh

            # Build the exploit binary
            COMMAND ${HAKC_CC} -o ${ROOTFS_DIR}/etc/ros2-demo/hakc-demo-exploit ${CMAKE_SOURCE_DIR}/scripts/ros2-demo/hakc-demo-exploit.c

            # Use ldd to find and copy library dependencies dynamically
            # Extract library paths from ldd output and copy them (following symlinks)
            COMMAND sh -c "ldd ${ROOTFS_DIR}/etc/ros2-demo/hakc-demo-exploit | grep '=> \/' | awk '{ print \$3 }' | xargs -I {} cp -L {} ${ROOTFS_DIR}/lib/"

            # Copy the dynamic linker (ld-linux-*.so.*)
            # It appears in ldd output but without '=>', so handle it separately
            COMMAND sh -c "ldd ${ROOTFS_DIR}/etc/ros2-demo/hakc-demo-exploit | grep -E '^[[:space:]]*/lib' | awk '{print \$1}' | xargs -I {} sh -c 'mkdir -p ${ROOTFS_DIR}/\$(dirname \"{}\") && cp -L \"{}\" ${ROOTFS_DIR}/\"{}\"'"

            # Copy the kernel modules
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${hardened_kernel_install_path}/lib/modules ${ROOTFS_DIR}/lib/modules

            # Copy the init script
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/scripts/kernel-userspace-scripts/init ${ROOTFS_DIR}/sbin/init
            COMMAND chmod +x ${ROOTFS_DIR}/sbin/init

            COMMAND sh -c "SIZE=\$(du -sm ${ROOTFS_DIR} | awk '{print int(\$1 * 2.0)}')M && truncate -s \$SIZE ${USERSPACE_IMG} && mkfs.ext4 -F -d ${ROOTFS_DIR} ${USERSPACE_IMG}"

            DEPENDS
            ${hardened_kernel_install_path}/${hakc_kernel_name}
            ${CMAKE_SOURCE_DIR}/scripts/ros2-demo/hakc-demo-exploit.c
            ${CMAKE_SOURCE_DIR}/scripts/ros2-demo/run-rosdemo.sh
            ${CMAKE_SOURCE_DIR}/scripts/kernel-userspace-scripts/init
            ${_hakc_clang_dep}
            WORKING_DIRECTORY ${hakc_build_dir}
            VERBATIM
    )

    # High-level target that always builds the initramfs
    add_custom_target(hakc-${arch}-userspace
            DEPENDS ${USERSPACE_IMG} hakc-linux-${arch}-enforcement
    )

    add_custom_target(run-${arch}-hakc-kernel
            COMMAND qemu-system-${arch} -kernel ${hardened_kernel_install_path}/${hakc_kernel_name} -drive file=${USERSPACE_IMG},format=raw,if=virtio -m 16G -append \"console=ttyS0 init=/sbin/init root=/dev/vda rw hakc_test=rosdemo nokaslr\" -nographic
            DEPENDS hakc-linux-${arch}-enforcement hakc-${arch}-userspace
            USES_TERMINAL
            WORKING_DIRECTORY ${hakc_build_dir}
    )

endforeach ()
