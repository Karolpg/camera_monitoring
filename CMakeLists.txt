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


cmake_minimum_required(VERSION 3.16)
set(TARGET_NAME camera_monitoring)
project(${TARGET_NAME})

set(SOURCES src/main.cpp
            src/ColorGenerator.cpp
            src/ColorGenerator.h
            src/Config.cpp
            src/Config.h
            src/Detector.cpp
            src/Detector.h
            src/DirUtils.cpp
            src/DirUtils.h
            src/Frame.h
            src/FrameController.cpp
            src/FrameController.h
            src/HttpCommunication.cpp
            src/HttpCommunication.h
            src/ImgUtils.cpp
            src/ImgUtils.h
            src/Letters.cpp
            src/Letters.h
            src/MovementAnalyzer.cpp
            src/MovementAnalyzer.h
            src/PngTools.cpp
            src/PngTools.h
            src/ProcessUtils.cpp
            src/ProcessUtils.h
            src/SlackCommunication.cpp
            src/SlackCommunication.h
            src/SlackFileTypes.cpp
            src/SlackFileTypes.h
            src/SlackSubscriber.cpp
            src/SlackSubscriber.h
            src/Timer.h
            src/TimeUtils.cpp
            src/TimeUtils.h
            src/VideoGrabber.cpp
            src/VideoGrabber.h
            src/VideoRecorder.cpp
            src/VideoRecorder.h)

add_executable(${TARGET_NAME} ${SOURCES})

##################################### PNG
find_package(PNG REQUIRED)
if (NOT PNG_FOUND)
    message(FATAL_ERROR "Please install: sudo apt install libpng-dev")
endif()

##################################### Darknet
set(darknetType opencl) #choose from (normal, gpu, opencl)
set(darknetProj )
if("${darknetType}" STREQUAL "gpu")
    find_package(darknet_gpu REQUIRED PATHS ${CMAKE_CURRENT_LIST_DIR}/3rdParties)
    set(darknetProj darknet_gpu::darknet_gpu)

elseif("${darknetType}" STREQUAL "opencl")
    find_package(darknet_opencl REQUIRED PATHS ${CMAKE_CURRENT_LIST_DIR}/3rdParties)
    set(darknetProj darknet_ocl::darknet_ocl)

else()
    find_package(darknet REQUIRED PATHS ${CMAKE_CURRENT_LIST_DIR}/3rdParties)
    set(darknetProj darknet::darknet)

endif()

##################################### OpenMP
find_package(OpenMP)

##################################### CURL
find_package(CURL REQUIRED)
if (NOT CURL_FOUND)
    message(FATAL_ERROR "Please install: sudo apt install libcurl4-openssl-dev")
endif()

##################################### RapidJSON
find_package(RapidJSON)
if (NOT RapidJSON_FOUND)
    message(FATAL_ERROR "Please install: sudo apt install rapidjson-dev")
endif()

target_link_libraries(${TARGET_NAME} gstreamer-1.0 
                                     gobject-2.0 
                                     glib-2.0
                                     gstaudio-1.0
                                     gstvideo-1.0
                                     #gstbase-1.0
                                     pthread
                                     PNG::PNG
                                     ${darknetProj}
                                     OpenMP::OpenMP_CXX
                                     CURL::libcurl)

target_include_directories(${TARGET_NAME} PRIVATE /usr/include/gstreamer-1.0 
                                                  /usr/include/glib-2.0 
                                                  /usr/lib/${CMAKE_SYSTEM_PROCESSOR}-linux-gnu/glib-2.0/include
                                                  )

target_compile_options(${TARGET_NAME} PRIVATE -fpermissive)

set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17)

#  pkg-config --cflags --libs gstreamer-1.0
