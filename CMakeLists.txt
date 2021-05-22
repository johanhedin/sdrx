cmake_minimum_required(VERSION 3.10)

#
# Run with make VERBOSE=1 to see how things are build
#

project(Sdrx CXX)

# We use C++17 stuff
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Import our local modules
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# Default build type is Debug. Run cmake with -DCMAKE_BUILD_TYPE=Release to
# build in release mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Our compiler flags. We run with optimization even in debug mode to be able
# to run in "real time"
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

#message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

add_executable(sdrx sdrx.cpp)

target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR})

target_link_libraries(sdrx m)
target_link_libraries(sdrx rtlsdr)

find_package(POPT REQUIRED)
if (POPT_FOUND)
    include_directories(${POPT_INCLUDE_DIRS})
    target_link_libraries (sdrx ${POPT_LIBRARIES})
endif(POPT_FOUND)

find_package(Threads REQUIRED)
if (Threads_FOUND)
    include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
    target_link_libraries (sdrx Threads::Threads)
endif(Threads_FOUND)

find_package(FFTW REQUIRED)
if (FFTW_FLOAT_LIB_FOUND)
    include_directories(${FFTW_INCLUDE_DIRS})
    target_link_libraries (sdrx ${FFTW_FLOAT_LIB})
endif(FFTW_FLOAT_LIB_FOUND)

find_package(LIBUSB REQUIRED)
if (LIBUSB_FOUND)
    include_directories(${LIBUSB_INCLUDE_DIRS})
    target_link_libraries (sdrx ${LIBUSB_LIBRARIES})
endif(LIBUSB_FOUND)

find_package(ALSA REQUIRED)
if (ALSA_FOUND)
    include_directories(${ALSA_INCLUDE_DIRS})
    target_link_libraries (sdrx ${ALSA_LIBRARIES})
endif(ALSA_FOUND)
