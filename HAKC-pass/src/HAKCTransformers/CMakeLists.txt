target_sources(HAKC-Compartmentalizer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/HAKCTransformer.cpp
        )

if (HAKC_CHERIBSD)
    add_subdirectory(CheriBSD)
else ()
    add_subdirectory(Linux)
endif ()