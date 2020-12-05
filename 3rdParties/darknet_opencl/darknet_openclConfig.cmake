
add_library(darknet_ocl::darknet_ocl STATIC IMPORTED)

set_property(TARGET darknet_ocl::darknet_ocl APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(darknet_ocl::darknet_ocl PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/libdarknet.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
)

find_package(clBLAS REQUIRED)
find_package(OpenCL REQUIRED)
message(STATUS "OpenCL_FOUND = ${OpenCL_FOUND}")
message(STATUS "clBLAS_FOUND = ${clBLAS_FOUND}")
set_target_properties(darknet_ocl::darknet_ocl PROPERTIES
    IMPORTED_LINK_INTERFACE_LIBRARIES OpenCL::OpenCL 
    IMPORTED_LINK_INTERFACE_LIBRARIES clBLAS
    INTERFACE_COMPILE_DEFINITIONS GPU=1
    )
