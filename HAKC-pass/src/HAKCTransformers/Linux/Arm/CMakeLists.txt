 if (HAKC_LINUX_ARMV8)
     target_sources(HAKC-Compartmentalizer PRIVATE
             ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTransformerLinuxArmV8.cpp
             )
 elseif (HAKC_LINUX_ARMV9)
     target_sources(HAKC-Compartmentalizer PRIVATE
             ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTransformerLinuxArmV9.cpp
             )
 endif ()