target_sources(HAKC-Compartmentalizer-${HAKC_BUILD_TYPE} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCCompartmentalizationPolicy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCCompartment.cpp
)

add_subdirectory(yaml)
