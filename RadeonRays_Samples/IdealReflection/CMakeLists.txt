set(COMMON_SOURCES
    ../Common/utils.h
    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    ideal_reflection.cpp
    ${COMMON_SOURCES}
)

add_executable(IdealReflection ${SOURCES})
target_link_libraries(IdealReflection PRIVATE RadeonRays tinyobjloader OpenImageIO::OpenImageIO)
target_include_directories(IdealReflection
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    )

