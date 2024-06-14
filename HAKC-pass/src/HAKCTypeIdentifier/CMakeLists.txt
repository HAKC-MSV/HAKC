target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTypeIdentifier.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCDebugInfoProcessor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTypeInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCGlobalInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCSymbolInfo.cpp
)
