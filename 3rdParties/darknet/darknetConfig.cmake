#
# The MIT License (MIT)
#
# Copyright 2020 Karolpg
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), 
# to deal in the Software without restriction, including without limitation the rights to #use, copy, modify, merge, publish, distribute, sublicense, 
# and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR #COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, 
# WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

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
