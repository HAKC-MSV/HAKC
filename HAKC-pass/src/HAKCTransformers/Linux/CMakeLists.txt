target_sources(HAKC-Compartmentalizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTransformerLinux.cpp
        )

add_subdirectory(CustomTransfers)

if (HAKC_LINUX_X86)
    add_subdirectory(X86)
elseif (HAKC_LINUX_ARMV8 OR HAKC_LINUX_ARMV9)
    add_subdirectory(Arm)
endif ()