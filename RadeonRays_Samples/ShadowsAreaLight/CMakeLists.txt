set(COMMON_SOURCES
    ../Common/utils.h
    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    shadows_area_light.cpp
    ${COMMON_SOURCES}
)

add_executable(ShadowsAreaLight ${SOURCES})
target_link_libraries(ShadowsAreaLight PRIVATE RadeonRays tinyobjloader OpenImageIO::OpenImageIO)
target_include_directories(ShadowsAreaLight
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    )

