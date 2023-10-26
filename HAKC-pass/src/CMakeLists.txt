target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCCompartment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCPass.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCSymbol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCSystemInformation.cpp
        )
add_subdirectory(HAKCTransformers)
add_subdirectory(HAKCAnalysis)
add_subdirectory(HAKCTypeIdentifier)
add_subdirectory(HAKCFunctionDefinition)