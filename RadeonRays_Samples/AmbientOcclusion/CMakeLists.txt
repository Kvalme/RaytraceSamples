set(COMMON_SOURCES
    ../Common/utils.h
    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    main.cpp
    ${COMMON_SOURCES}
)

add_executable(AmbientOcclusion ${SOURCES})
target_link_libraries(AmbientOcclusion PRIVATE RadeonRays tinyobjloader OpenImageIO::OpenImageIO)
target_include_directories(AmbientOcclusion 
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    )

