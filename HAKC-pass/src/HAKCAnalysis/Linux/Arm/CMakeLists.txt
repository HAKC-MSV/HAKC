target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCModuleAnalysisLinuxArm.cpp
)

if (HAKC_LINUX_ARMV8)
    target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionAnalysisLinuxArmV8.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/HAKCModuleAnalysisLinuxArmV8.cpp
    )
elseif (HAKC_LINUX_ARMV9)
    target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionAnalysisLinuxArmV9.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/HAKCModuleAnalysisLinuxArmV9.cpp
    )
endif ()
