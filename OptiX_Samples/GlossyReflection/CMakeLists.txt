set(COMMON_SOURCES
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    glossy_reflection.cpp
    ${COMMON_SOURCES}
)

add_executable(GlossyReflect_OptiX ${SOURCES})

target_link_libraries(GlossyReflect_OptiX PRIVATE tinyobjloader OpenImageIO::OpenImageIO Optix::Optix)
target_include_directories(GlossyReflect_OptiX 
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    PRIVATE ../sutil
    PRIVATE ${CUDA_INCLUDE_DIRS}
    )

