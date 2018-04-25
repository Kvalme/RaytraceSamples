set(COMMON_SOURCES
    ../Common/utils.h
    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    shadows_point_light.cpp
    ${COMMON_SOURCES}
)

add_executable(ShadowsPointLight ${SOURCES})
target_link_libraries(ShadowsPointLight PRIVATE RadeonRays tinyobjloader OpenImageIO::OpenImageIO)
target_include_directories(ShadowsPointLight
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    )

