set(OptiX_DIR "C:/ProgramData/NVIDIA Corporation/OptiX SDK 5.0.1")

find_library(OPTIX_LIBRARY NAMES optix.1 PATH_SUFFIXES lib64 PATHS "${OptiX_DIR}/lib64")
find_library(OPTIX_U_LIBRARY NAMES optixu.1 PATH_SUFFIXES lib64 PATHS "${OptiX_DIR}/lib64")
find_path(OPTIX_INCLUDE_DIR NAMES optix.h PATHS "${OptiX_DIR}/include")

find_package_handle_standard_args(Optix DEFAULT_MSG OPTIX_LIBRARY OPTIX_INCLUDE_DIR)

add_library(Optix::Optix UNKNOWN IMPORTED)

set_target_properties(Optix::Optix PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OPTIX_INCLUDE_DIR}")

set_property(TARGET Optix::Optix APPEND PROPERTY IMPORTED_LOCATION "${OPTIX_LIBRARY}")
