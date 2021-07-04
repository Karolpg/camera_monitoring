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
#  INTERFACE_COMPILE_DEFINITIONS GPU=1
#  INTERFACE_COMPILE_DEFINITIONS OCL=1
)

target_compile_definitions(darknet_ocl::darknet_ocl INTERFACE GPU=1
                                                    INTERFACE OCL=1)
