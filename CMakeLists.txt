cmake_minimum_required(VERSION 3.4.3)

project(HAKC)

if(HAKC_LLVM)
    add_subdirectory(llvm-project/llvm)
else()
    if (HAKC_CHERIBSD_MORELLO)
        set(HAKC_BUILD_TYPE cheribsd-morello)
    elseif (HAKC_LINUX_X86)
        set(HAKC_BUILD_TYPE linux-x86)
    elseif (HAKC_LINUX_ARMV8)
        set(HAKC_BUILD_TYPE linux-armv8)
    elseif (HAKC_LINUX_ARMV9)
        set(HAKC_BUILD_TYPE linux-armv9)
    else ()
        message(FATAL_ERROR
                "Must Specify Target HAKC Architecture")
    endif ()

    set(HAKC_PASS_LIB "HAKC-Compartmentalizer-${HAKC_BUILD_TYPE}")

    cmake_host_system_information(RESULT Ncpu QUERY NUMBER_OF_PHYSICAL_CORES)

    add_subdirectory(HAKC-pass)

    if (NOT HAKC_CHERIBSD_MORELLO)
        set(LINUX_BUILD_PATH "${CMAKE_SOURCE_DIR}/build-${HAKC_BUILD_TYPE}"
            CACHE PATH "Path where the Linux kernel will build to"
        )

        set(HAKC_DAG_DIR ${LINUX_BUILD_PATH}/hakc-dag-analysis)

        list(APPEND LINUX_ENV_VARS
                "HAKC_DAG_ANALYSIS_ROOT=${HAKC_DAG_DIR}"
                "HAKC_SOURCE_PATH=${CMAKE_SOURCE_DIR}/linux"
                "HAKC_BUILD_PATH=${LINUX_BUILD_PATH}"
                "BUILD_TYPE=${HAKC_BUILD_TYPE}"
                "HAKC_COMPARTMENT_PATH=${HAKC_DAG_DIR}/hakc-compartments.yml"
        )

        list(APPEND LINUX_MAKE_FLAGS
                "LLVM=1"
                "CC=${CMAKE_SOURCE_DIR}/install/bin/clang"
                "HOSTCC=${CMAKE_SOURCE_DIR}/install/bin/clang"
                "LOCALVERSION=${HAKC_BUILD_TYPE}"
                "O=${LINUX_BUILD_PATH}"
                "-j${Ncpu}"
        )

        if (HAKC_LINUX_ARMV8 OR HAKC_LINUX_ARMV9)
            list(APPEND LINUX_MAKE_FLAGS
                    "ARCH=arm64"
                    "CROSS_COMPILE=aarch64-linux-gnu-"
            )
        endif(HAKC_LINUX_ARMV8 OR HAKC_LINUX_ARMV9)

        list(APPEND LINUX_CONFIG_ADJUSTMENTS
                "-e" "CONFIG_HAKC"
                "--set-str" "CONFIG_HAKC_PASS_PATH"
                "${CMAKE_SOURCE_DIR}/install/lib/libHAKC-Compartmentalizer-${HAKC_BUILD_TYPE}.so"
        )

        if (HAKC_LINUX_ARMV8)
            list(APPEND LINUX_CONFIG_ADJUSTMENTS
                    "-e" "CONFIG_HAKC_ARM_V8"
                    "-e" "CONFIG_HAKC_ARM_V8_MEMORY"
                    "-d" "CONFIG_HAKC_ARM_V9"
            )
        endif(HAKC_LINUX_ARMV8)

        if (HAKC_LINUX_ARMV9)
            list(APPEND LINUX_CONFIG_ADJUSTMENTS
                    "-e" "CONFIG_HAKC_ARM_V9"
            )
        endif(HAKC_LINUX_ARMV9)

        if (HAKC_LINUX_X86)
            list(APPEND LINUX_CONFIG_ADJUSTMENTS
                    "-e" "CONFIG_CRYPTO_SHA1"
                    "-e" "CONFIG_CRYPTO_SHA256"
                    "-e" "CONFIG_CRYPTO_SHA256_SSSE3"
                    "-e" "CONFIG_CRYPTO_SHA1_SSSE3"
                    "-e" "CONFIG_AS_SHA256_NI"
                    "-e" "CONFIG_HAKC_X86"
                    "-e" "CONFIG_HAKC_X86_SIGN_NI"
                    "-d" "CONFIG_HAKC_X86_SIGN_SSSE3"
                    "-e" "CONFIG_HAKC_X86_MEMORY"
                    "-d" "CONFIG_HAKC_X86_BTREE"
            )
        endif(HAKC_LINUX_X86)

        list(APPEND LINUX_CONFIG_ADJUSTMENTS
                "-d" "CONFIG_HAKC_ALLOW_FAILED"
                "-e" "CONFIG_HAKC_SIGN_PTR"
                "-d" "CONFIG_HAKC_DEBUG_PRINT"
                "-d" "CONFIG_HAKC_LOG_FAILURE"
                "-m" "CONFIG_ROSDEMO"
                "-e" "CONFIG_DEBUG_INFO"
                "-e" "CONFIG_DEBUG_INFO_SPLIT"
                "-e" "CONFIG_DEBUG_INFO_DWARF4"
                "-e" "CONFIG_GDB_SCRIPTS"
                "-d" "CONFIG_DEBUG_INFO_REDUCED"
                "-d" "CONFIG_DEBUG_INFO_COMPRESSED"
        )

        add_custom_target(linux-config
                env ${LINUX_ENV_VARS} make ${LINUX_MAKE_FLAGS} defconfig
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Generating KConfig for ${HAKC_BUILD_TYPE}"
        )

        add_custom_target(linux-adjust-config
                scripts/config --file ${LINUX_BUILD_PATH}/.config ${LINUX_CONFIG_ADJUSTMENTS}
                BYPRODUCTS ${LINUX_BUILD_PATH}/.config
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Adjusting KConfig for ${HAKC_BUILD_TYPE}"
        )

        add_custom_target(linux-dag
                env HAKC_ANALYSIS=dag ${LINUX_ENV_VARS} make ${LINUX_MAKE_FLAGS}
                COMMAND env ${LINUX_ENV_VARS} make ${LINUX_MAKE_FLAGS} clean
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                BYPRODUCTS ${HAKC_DAG_DIR}
                COMMENT "Generating DAG YAML files for ${HAKC_BUILD_TYPE}"
        )

        add_custom_target(linux-clean
                env ${LINUX_ENV_VARS} make ${LINUX_MAKE_FLAGS} clean
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                COMMENT "Cleaning ${LINUX_BUILD_PATH}"
        )

        add_custom_target(linux-compartments
            python3 python/analysis/data-access-analysis.py
                -c ${HAKC_DAG_DIR}/dag.bin
                -r ${HAKC_DAG_DIR}
                --dag --filter_types --filter_mod_files
                BYPRODUCTS ${HAKC_DAG_DIR}/dag.bin
                COMMENT "Generating Base Compartments for ${HAKC_BUILD_TYPE}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        add_custom_target(linux-adjust-compartments
                python3 python/analysis/data-access-analysis.py
                -c ${HAKC_DAG_DIR}/dag.bin
                --adjust scripts/ros2-demo/rosdemo-compartments.yml
                BYPRODUCTS ${HAKC_DAG_DIR}/dag-adjusted.bin
                COMMENT "Adjusting Compartments for ${HAKC_BUILD_TYPE}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        add_custom_target(linux-output-policy
                python3 python/analysis/data-access-analysis.py
                -c ${HAKC_DAG_DIR}/dag-adjusted.bin
                --output_compart ${HAKC_DAG_DIR}/hakc-compartments.yml
                BYPRODUCTS ${HAKC_DAG_DIR}/hakc-compartments.yml
                COMMENT "Outputting Compartmentalization Policy for ${HAKC_BUILD_TYPE}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )

        add_custom_target(linux-hakc
                env HAKC_ANALYSIS=compartmentalize ${LINUX_ENV_VARS} make ${LINUX_MAKE_FLAGS}
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/linux
                BYPRODUCTS ${LINUX_BUILD_PATH}/vmlinux
                COMMENT "Building HAKC Protected kernel for ${HAKC_BUILD_TYPE}"
        )

        add_dependencies(linux-config ${HAKC_PASS_LIB})
        add_dependencies(linux-dag linux-adjust-config)
        add_dependencies(linux-adjust-config linux-config)
        add_dependencies(linux-compartments linux-dag)
        add_dependencies(linux-adjust-compartments linux-compartments)
        add_dependencies(linux-output-policy linux-adjust-compartments)
        add_dependencies(linux-hakc linux-output-policy)

    endif(NOT HAKC_CHERIBSD_MORELLO)

endif()