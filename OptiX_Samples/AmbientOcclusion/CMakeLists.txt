set(COMMON_SOURCES
#    ../Common/utils.h
#    ../Common/utils.cpp
    ../Common/scene.h
    ../Common/scene.cpp
)

set(SOURCES 
    main.cpp
    ${COMMON_SOURCES}
)

add_executable(AmbientOcclusion_OptiX ${SOURCES})

target_link_libraries(AmbientOcclusion_OptiX PRIVATE tinyobjloader OpenImageIO::OpenImageIO Optix::Optix)
target_include_directories(AmbientOcclusion_OptiX 
    PRIVATE ../Common
    PRIVATE ${CMAKE_SOURCE_DIR}/Tools/externals/tinyobjloader-1.1.0
    PRIVATE ../sutil
    PRIVATE ${CUDA_INCLUDE_DIRS}
    )

