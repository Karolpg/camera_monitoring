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
            src/Frame.h
            src/FrameControler.cpp
            src/FrameControler.h
            src/HttpCommunication.cpp
            src/HttpCommunication.h
            src/Letters.cpp
            src/Letters.h
            src/PngTools.cpp
            src/PngTools.h
            src/SlackCommunication.cpp
            src/SlackCommunication.h
            src/SlackFileTypes.cpp
            src/SlackFileTypes.h
            src/SlackSubscriber.cpp
            src/SlackSubscriber.h
            src/VideoGrabber.cpp
            src/VideoGrabber.h
            src/VideoRecorder.cpp
            src/VideoRecorder.h)

add_executable(${TARGET_NAME} ${SOURCES})

find_package(PNG REQUIRED)
find_package(darknet REQUIRED PATHS ${CMAKE_CURRENT_LIST_DIR}/3rdParties)
find_package(OpenMP)
find_package(CURL REQUIRED)

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
                                     darknet::darknet
                                     OpenMP::OpenMP_CXX
                                     CURL::libcurl)

target_include_directories(${TARGET_NAME} PRIVATE /usr/include/gstreamer-1.0 
                                                  /usr/include/glib-2.0 
                                                  /usr/lib/x86_64-linux-gnu/glib-2.0/include
                                                  )

target_compile_options(${TARGET_NAME} PRIVATE -fpermissive)

set_target_properties(${TARGET_NAME} PROPERTIES CXX_STANDARD 17)

#  pkg-config --cflags --libs gstreamer-1.0
