target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCCustomTransfer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFunctionDefinition.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTransferFunction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/SingleFunctionCustomTransfer.cpp
        )