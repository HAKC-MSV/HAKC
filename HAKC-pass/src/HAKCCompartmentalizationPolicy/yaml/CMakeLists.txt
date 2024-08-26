target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlClique.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlCompartment.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlCompartmentalizationPolicy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlSymbol.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlScope.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCYamlType.cpp
)
