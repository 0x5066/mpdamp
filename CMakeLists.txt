cmake_minimum_required(VERSION 3.5)

project(mpdamp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Set compiler flags for debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")

add_executable(mpdamp
    main.cpp
    fft.cpp
    fft.h
    mpdamp.cpp
    mpdamp.h
    visualizer.cpp
    visualizer.h
    draw_numtext.cpp
    draw_numtext.h
    bitmaptext.h
    bitmaptext.cpp
)

target_link_libraries(mpdamp Qt5::Core Qt5::Widgets mpdclient)

# Set the build type to Debug to apply the debug flags
set(CMAKE_BUILD_TYPE Debug)
