

add_library(darknet_gpu::darknet_gpu STATIC IMPORTED)

if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    set(darknetGpuArchSufix _aarch64)
endif()

set_property(TARGET darknet_gpu::darknet_gpu APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(darknet_gpu::darknet_gpu PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/libdarknet${darknetGpuArchSufix}.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
)

find_package(CUDAToolkit REQUIRED)
set_target_properties(darknet_gpu::darknet_gpu PROPERTIES
  IMPORTED_LINK_INTERFACE_LIBRARIES "CUDA::cudart;CUDA::cublas;CUDA::curand"
  INTERFACE_COMPILE_DEFINITIONS GPU=1
)
