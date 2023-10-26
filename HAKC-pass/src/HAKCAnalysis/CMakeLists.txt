target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CommonHAKCAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCModuleAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ManagedHAKCPointer.cpp
        )

if (HAKC_CHERIBSD)
    add_subdirectory(CheriBSD)
else ()
    add_subdirectory(Linux)
endif ()