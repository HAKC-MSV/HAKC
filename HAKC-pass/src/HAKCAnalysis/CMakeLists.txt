target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/CommonHAKCAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCModuleAnalysis.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/ManagedHAKCPointer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCPointerManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCOstream.cpp
)
