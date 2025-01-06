add_subdirectory(HAKCTransformers)
add_subdirectory(HAKCAnalysis)
add_subdirectory(HAKCTypeIdentifier)
add_subdirectory(HAKCFunctionDefinition)
add_subdirectory(HAKCCompartmentalizationPolicy)
add_subdirectory(HAKCSystem)

set(HAKC_SOURCES ${HAKC_SOURCES}
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCPass.cpp
        PARENT_SCOPE
)
