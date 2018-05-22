set(COMMON_SOURCES
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    main.cpp
    ${COMMON_SOURCES}
)

add_executable(IdealReflect_OptiX ${SOURCES})

target_link_libraries(IdealReflect_OptiX PRIVATE tinyobjloader OpenImageIO::OpenImageIO Optix::Optix)
target_include_directories(IdealReflect_OptiX 
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    PRIVATE ../sutil
    PRIVATE ${CUDA_INCLUDE_DIRS}
    )

