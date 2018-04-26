set(COMMON_SOURCES
    ../Common/utils.h
    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    glossy_reflection.cpp
    ${COMMON_SOURCES}
)

add_executable(GlossyReflection ${SOURCES})
target_link_libraries(GlossyReflection PRIVATE RadeonRays tinyobjloader OpenImageIO::OpenImageIO)
target_include_directories(GlossyReflection
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    )

