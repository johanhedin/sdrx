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

# Add submodule libairspy (airspyone_host) as a subdirectory to our project.
# The EXCLUDE_FROM_ALL option allows us to cherry pick the airspy-static
# target later on since we are only interested in libairspy and not the host
# tools
add_subdirectory(libairspy/libairspy EXCLUDE_FROM_ALL)

add_executable(sdrx sdrx.cpp)
add_executable(dts EXCLUDE_FROM_ALL dts.cpp rtl_dev.cpp airspy_dev.cpp)

# Add our project root and the linked libairspy as include directories
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)

target_link_libraries(sdrx m)
target_link_libraries(sdrx airspy-static)
target_link_libraries(dts airspy-static)

# Look for all required packages
find_package(POPT REQUIRED)
if (POPT_FOUND)
    include_directories(${POPT_INCLUDE_DIRS})
    target_link_libraries (sdrx ${POPT_LIBRARIES})
endif(POPT_FOUND)

find_package(Threads REQUIRED)
if (Threads_FOUND)
    include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
    target_link_libraries (sdrx Threads::Threads)
    target_link_libraries (dts Threads::Threads)
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
    target_link_libraries (dts ${LIBUSB_LIBRARIES})
endif(LIBUSB_FOUND)

find_package(ALSA REQUIRED)
if (ALSA_FOUND)
    include_directories(${ALSA_INCLUDE_DIRS})
    target_link_libraries (sdrx ${ALSA_LIBRARIES})
endif(ALSA_FOUND)

find_package(RTLSDR REQUIRED)
if (RTLSDR_FOUND)
    include_directories(${RTLSDR_INCLUDE_DIRS})
    target_link_libraries (sdrx ${RTLSDR_LIBRARIES})
    target_link_libraries (dts ${RTLSDR_LIBRARIES})
endif(RTLSDR_FOUND)
