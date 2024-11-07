target_sources(HAKC-Compartmentalizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCPass.cpp
)
add_subdirectory(HAKCTransformers)
add_subdirectory(HAKCAnalysis)
add_subdirectory(HAKCTypeIdentifier)
add_subdirectory(HAKCFunctionDefinition)
add_subdirectory(HAKCCompartmentalizationPolicy)
add_subdirectory(HAKCSystem)
