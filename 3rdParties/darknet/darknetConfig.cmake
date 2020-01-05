

add_library(darknet::darknet STATIC IMPORTED)

set_property(TARGET darknet::darknet APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(darknet::darknet PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C"
  IMPORTED_LOCATION_RELEASE "${CMAKE_CURRENT_LIST_DIR}/lib/libdarknet.a"
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/include"
)

set(OPENMP 1)
if (${OPENMP})
    find_package(OpenMP REQUIRED)
    message(STATUS "OpenMP_FOUND = ${OpenMP_FOUND}")
    set_target_properties(darknet::darknet PROPERTIES
        IMPORTED_LINK_INTERFACE_LIBRARIES OpenMP::OpenMP_C
        )
        #IMPORTED_LINK_DEPENDENT_LIBRARIES OpenMP::OpenMP_C
        #INTERFACE_COMPILE_OPTIONS -fopenmp
        #INTERFACE_LINK_OPTIONS -fopenmp
endif()
