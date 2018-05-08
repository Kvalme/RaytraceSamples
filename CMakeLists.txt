cmake_minimum_required(VERSION 3.8)

project(Baikal CXX C)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Threads REQUIRED)
find_package(OIIO REQUIRED)
find_package(OptiX REQUIRED)
find_package(CUDA 5.0 REQUIRED)
set(CUDA_NVRTC_ENABLED ON)

#Configure RadeonRays build
set(RR_EMBED_KERNELS ON CACHE BOOL "Embed CL kernels into binary module")
set(RR_ALLOW_CPU_DEVICES OFF CACHE BOOL "Allows CPU Devices")
set(RR_USE_OPENCL ON CACHE BOOL "Use OpenCL for GPU hit testing")
set(RR_USE_EMBREE OFF CACHE BOOL "Use Intel(R) Embree for CPU hit testing")
set(RR_USE_VULKAN OFF CACHE BOOL "Use vulkan for GPU hit testing")
set(RR_NO_TESTS ON CACHE BOOL "Don't add any unit tests and remove any test functionality from the library")
set(RR_ENABLE_STATIC OFF CACHE BOOL "Create static libraries rather than dynamic")
set(RR_SHARED_CALC OFF CACHE BOOL "Link Calc(compute abstraction layer) dynamically")
set(RR_ENABLE_RAYMASK $BAIKAL_ENABLE_RAYMASK CACHE BOOL "Enable ray masking in intersection kernels")
set(RR_TUTORIALS OFF CACHE BOOL "Add tutorials projects")
set(RR_SAFE_MATH OFF CACHE BOOL "use safe math")

add_subdirectory(Tools/externals/tinyobjloader-1.1.0)
add_subdirectory(RadeonRays_SDK)

add_subdirectory(RadeonRays_Samples)
add_subdirectory(OptiX_Samples)
