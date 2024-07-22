cmake_minimum_required(VERSION 3.5)

project(mpdamp)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Manually specify FFTW library and include directories
find_library(FFTW_LIB fftw3 REQUIRED)
find_path(FFTW_INCLUDE_DIR fftw3.h REQUIRED)

include_directories(${FFTW_INCLUDE_DIR})

# Set compiler flags for debugging
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")

add_executable(mpdamp
    main.cpp
    mpdamp.cpp
    mpdamp.h
    visualizer.cpp
    visualizer.h
    draw_numtext.cpp
    draw_numtext.h
)

target_link_libraries(mpdamp Qt5::Core Qt5::Widgets ${FFTW_LIB} mpdclient)

# Set the build type to Debug to apply the debug flags
set(CMAKE_BUILD_TYPE Debug)
