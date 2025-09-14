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
    set(FULL_CORE_COUNT ${LLVM_PARALLEL_COMPILE_JOBS})
else ()
    cmake_host_system_information(RESULT FULL_CORE_COUNT
            QUERY NUMBER_OF_LOGICAL_CORES)
endif ()

math(EXPR HAKC_ANALYSIS_CORE_COUNT "${FULL_CORE_COUNT}")
math(EXPR HAKC_COMPARTMENTALIZE_CORE_COUNT "(((${FULL_CORE_COUNT} + 1)/ 2) - 1)")
if (HAKC_COMPARTMENTALIZE_COUNT_COUNT EQUAL 0)
    math(EXPR HAKC_COMPARTMENTALIZE_CORE_COUNT "1")
endif ()

message("-- HAKC is using up to ${HAKC_ANALYSIS_CORE_COUNT} cores to build Linux")

list(APPEND LINUX_ARCHES_TO_BUILD x86)
list(APPEND LINUX_ARCH_MAKE_OPTS "")
set(HAKC_LINUX_BUILD_PATH ${CMAKE_BINARY_DIR}/linux CACHE PATH "Path to build Linux kernels" FORCE)

foreach (arch make_opts IN ZIP_LISTS LINUX_ARCHES_TO_BUILD LINUX_ARCH_MAKE_OPTS)
    set(linux_build_path ${HAKC_LINUX_BUILD_PATH}/${arch})
    file(MAKE_DIRECTORY ${linux_build_path}/hakc-dag-analysis)
    set(HAKC_SERVER_CONFIG_PATH ${linux_build_path}/hakc-server.yaml)
    set(HAKC_ADJUSTMENTS_PATH ${linux_build_path}/linux-x86-adjustments.yaml)

    configure_file(${CMAKE_SOURCE_DIR}/configs/server/hakc-server.yaml.in
            ${linux_build_path}/hakc-server.yaml
    )

    set(build_modes analysis;compartmentalize)
    set(hakc_config_modes RunDataAccessGraphAnalysis;RunCompartmentalization)
    foreach (build_mode hakc_config_mode IN ZIP_LISTS build_modes hakc_config_modes)
        set(LINUX_MAKE_OPTS O=${linux_build_path}/${build_mode} LLVM=1 ARCH=${arch} CC=$<TARGET_FILE:clang> HOSTCC=$<TARGET_FILE:clang> ${make_opts})
        set(HAKC_CONFIG_MODE ${hakc_config_mode} )
        file(MAKE_DIRECTORY ${linux_build_path}/${build_mode})
        configure_file(${CMAKE_SOURCE_DIR}/configs/compartmentalizations/linux/${arch}/analysis-config.yaml.in
                ${linux_build_path}/${build_mode}-config.yaml
        )
        configure_file(${CMAKE_SOURCE_DIR}/configs/compartmentalizations/linux/adjustments/linux-x86-adjustments.yaml
                ${linux_build_path}/linux-x86-adjustments.yaml
        )
        configure_file(${CMAKE_SOURCE_DIR}/scripts/config-edits/${arch}/modify-config.sh.in
                ${linux_build_path}/modify-${build_mode}-config.sh
        )
        if (${build_mode} STREQUAL "analysis")
            set(HAKC_LINUX_CORE_COUNT ${HAKC_ANALYSIS_CORE_COUNT})
        else ()
            set(HAKC_LINUX_CORE_COUNT ${HAKC_COMPARTMENTALIZE_CORE_COUNT})
        endif ()

        add_custom_target(hakc-linux-${arch}-${build_mode}-config
                BYPRODUCTS ${linux_build_path}/${build_mode}/.config
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Generating ${arch} ${build_mode} Linux config"
                DEPENDS ${CMAKE_SOURCE_DIR}/scripts/config-edits/${arch}/modify-config.sh.in
                COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT} defconfig
                COMMAND bash ${linux_build_path}/modify-${build_mode}-config.sh
        )

        add_custom_target(linux-${arch}-${build_mode}-pass
                BYPRODUCTS ${linux_build_path}/${build_mode}/vmlinux
                DEPENDS ${linux_build_path}/${build_mode}-config.yaml clang hakc-linux-${arch}-${build_mode}-config
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Running ${arch} Linux ${build_mode} pass"
                USES_TERMINAL
                COMMAND make ${LINUX_MAKE_OPTS} -j${HAKC_LINUX_CORE_COUNT}
        )
    endforeach ()
    add_custom_target(linux-${arch}-dag
            DEPENDS linux-${arch}-analysis-pass
            COMMENT "Generating Initial ${arch} Linux Compartmentalization"
    )

    add_custom_target(linux-${arch}-compilation-db
            BYPRODUCTS ${linux_build_path}/compile_commands.json
            DEPENDS linux-${arch}-analysis-pass
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
            COMMENT "Generating ${arch} Linux compile_commands.json"
            COMMAND ./scripts/gen_compile_commands.py --output ${linux_build_path}/compile_commands.json --directory ${linux_build_path}/analysis
    )
endforeach ()
