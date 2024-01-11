cmake_minimum_required(VERSION 3.10)

#
# Run with make VERBOSE=1 to see how things are build
#

#
# Check here for how to use external project: https://github.com/ogdf/ogdf/issues/22
#

project(Sdrx CXX)

# We use C++17 stuff
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS TRUE)

# Import our local modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# Default build type is Release. Run cmake with -DCMAKE_BUILD_TYPE=Debug to
# build in debug mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release. Change to debug with -DCMAKE_BUILD_TYPE=Debug")
endif(NOT CMAKE_BUILD_TYPE)

# Our compiler flags. We run with optimization even in debug mode to be able
# to run in "real time".
# Extra flags for reporting around vectorization are:
#   -ftree-vectorize -fopt-info-vec-missed -ftree-vectorizer-verbose=7
#   -fopt-info-vec -fopt-info-vec-note
set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native  -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_C_FLAGS "-Wall -Wextra -march=native  -mtune=native")
set(CMAKE_C_FLAGS_DEBUG "-g -O2")
set(CMAKE_C_FLAGS_RELEASE "-O3")

# Add submodule libairspy (airspyone_host) as a subdirectory to our project.
# The EXCLUDE_FROM_ALL option allows us to cherry pick the airspy-static
# target later on since we are only interested in libairspy and not the host
# tools
add_subdirectory(libairspy/libairspy EXCLUDE_FROM_ALL)

# Add submodule librtlsdr as a subdirectory to our project.
# The EXCLUDE_FROM_ALL option will allow us to cherry pick the rtlsdr_static
# target later on since we are only interested in librtlsdr and not the other
# tools. We also enable the DETACH_KERNEL_DRIVER option.
option(DETACH_KERNEL_DRIVER "" ON)
add_subdirectory(librtlsdr EXCLUDE_FROM_ALL)

add_library(r820dev src/r820_dev.cpp src/rtl_dev.cpp src/airspy_dev.cpp)

add_executable(sdrx src/sdrx.cpp)
add_executable(dts EXCLUDE_FROM_ALL src/dts.cpp)

# Add our project root and the linked libairspy/librtlsdr as include directories
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)

target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)

target_link_libraries(sdrx r820dev)
target_link_libraries(sdrx m)
target_link_libraries(sdrx airspy-static)
target_link_libraries(sdrx rtlsdr_static)

target_link_libraries(dts r820dev)
target_link_libraries(dts m)
target_link_libraries(dts airspy-static)
target_link_libraries(dts rtlsdr_static)

# Look for all required packages
find_package(POPT REQUIRED)
if (POPT_FOUND)
    include_directories(${POPT_INCLUDE_DIRS})
    target_link_libraries (sdrx ${POPT_LIBRARIES})
    target_link_libraries (dts ${POPT_LIBRARIES})
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

find_package(PkgConfig REQUIRED)
if (PkgConfig_FOUND)
    pkg_check_modules(SIGC2 REQUIRED sigc++-2.0)
    include_directories(${SIGC2_INCLUDE_DIRS})
    target_link_libraries(sdrx ${SIGC2_LIBRARIES})
    target_link_libraries(dts ${SIGC2_LIBRARIES})
endif(PkgConfig_FOUND)

#find_package(Volk REQUIRED)
#if (VOLK_FOUND)
#    include_directories(${VOLK_INCLUDE_DIRS})
#    target_link_libraries (sdrx ${VOLK_LIBRARIES})
#endif(VOLK_FOUND)
