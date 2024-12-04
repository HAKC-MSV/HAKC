set(HAKC_SOURCES ${HAKC_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTypeIdentifier.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTypeInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCGlobalInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCSymbolInfo.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCIndirectCallSource.cpp

        PARENT_SCOPE
)
