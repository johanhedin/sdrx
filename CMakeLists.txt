cmake_minimum_required(VERSION 3.10...3.27)

#
# Run with make VERBOSE=1 to see how things are build
#

#
# Check here for how to use external project: https://github.com/ogdf/ogdf/issues/22
#

# TODO: - Refactor to be more in line with cte/ctl.
#       - Add option to build with gprof (add -pg as both compile and link flag).
#       - Only use pkg-config to find dependencies

project(sdrx C CXX)

# We use C++20 stuff
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# This property is needed to handle the uninstall custom target added by
# librtlsdr. It can probably be removed if librtlsdr adds a guard for
# the uninstall target
set_property(GLOBAL PROPERTY ALLOW_DUPLICATE_CUSTOM_TARGETS TRUE)

# Import our local modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

# Default build type is Release. Run cmake with -DCMAKE_BUILD_TYPE=Debug to
# build in debug mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release. Change to debug with -DCMAKE_BUILD_TYPE=Debug")
endif(NOT CMAKE_BUILD_TYPE)

# We need the PkgConfig cmake module to be able to find libraries we depend on
find_package(PkgConfig REQUIRED)

# Use GNUInstallDirs to get standard install directories for include, lib, bin and so on
include(GNUInstallDirs)

# Create a uninstall target if it does not already exist. We need this before
# any add_subdirectory so that uninstall targets from them do not propagate here.
# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# Add submodule libairspy (airspyone_host) as a subdirectory to our project.
# The EXCLUDE_FROM_ALL option allows us to cherry pick the airspy-static
# target later on since we are only interested in libairspy and not the host
# tools. Setting airspy_all_SOURCE_DIR is to prevent libairspy to add uninstall
# target
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE) # Needed to silence < 3.5 version warning
set(airspy_all_SOURCE_DIR "dummy")
add_subdirectory(libairspy/libairspy EXCLUDE_FROM_ALL)

# Add submodule librtlsdr as a subdirectory to our project.
# The EXCLUDE_FROM_ALL option will allow us to cherry pick the rtlsdr_static
# target later on since we are only interested in librtlsdr and not the other
# tools. We also enable the DETACH_KERNEL_DRIVER option.
option(DETACH_KERNEL_DRIVER "" ON)
add_subdirectory(librtlsdr EXCLUDE_FROM_ALL)

# Our compiler flags. We run with optimization even in debug mode to be able
# to run in "real time".
# Extra flags for reporting around vectorization are:
#   -ftree-vectorize -fopt-info-vec-missed -ftree-vectorizer-verbose=7
#   -fopt-info-vec -fopt-info-vec-note
set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -mtune=native")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O2")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_C_FLAGS "-Wall -Wextra -march=native -mtune=native")
set(CMAKE_C_FLAGS_DEBUG "-g -O2")
set(CMAKE_C_FLAGS_RELEASE "-O3")

add_library(r820dev src/r820_dev.cpp src/rtl_dev.cpp src/airspy_dev.cpp)
add_executable(sdrx src/sdrx.cpp)
add_executable(dts EXCLUDE_FROM_ALL src/dts.cpp)


# We take care of building uSockets ourselvs
FILE(GLOB USOCKET_SRCS "uSockets/src/*.c"
                       "uSockets/src/eventing/*.c"
                       "uSockets/src/crypto/*.c"
                       "uSockets/src/io_uring/*.c"
                       "uSockets/src/crypto/*.cpp"
)
add_library(usockets ${USOCKET_SRCS})
target_include_directories(usockets PRIVATE ${PROJECT_SOURCE_DIR}/uSockets/src)
# Silence some annoying build warnings
target_compile_options(usockets PRIVATE "-Wno-unused-parameter")
target_compile_options(usockets PRIVATE "-Wno-sign-compare")
target_compile_options(usockets PRIVATE "-Wno-use-after-free")
target_compile_options(usockets PRIVATE "-Wno-deprecated-declarations")
# Use openssl and libuv from system
target_compile_options(usockets PRIVATE "-DLIBUS_USE_OPENSSL")
target_compile_options(usockets PRIVATE "-DLIBUS_USE_LIBUV")
# Uggly hack to get rid of some annoying printf in crypto/openssl.c
#target_compile_options(usockets PRIVATE "-include${PROJECT_SOURCE_DIR}/src/silence_printf.h")

# Uggly hack to get rid of some std::cout in App.h
add_custom_command(OUTPUT App-fixed.h
    COMMAND sed -e "s/std::cout/\\/\\/std::cout/g" ${PROJECT_SOURCE_DIR}/uWebSockets/src/App.h > App-fixed.h
    DEPENDS ${PROJECT_SOURCE_DIR}/uWebSockets/src/App.h
    VERBATIM)
target_sources(sdrx PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/App-fixed.h")
target_include_directories(sdrx PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add our project root and the linked libairspy/librtlsdr as include directories
# TODO: Look over this and make it possible to build with both in-tree or
# system libs. Need to adjust the includes in rtl_dev.cpp and airspy_dev.cpp
# as well
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/uSockets/src)
target_include_directories(sdrx PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/src)

target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR})
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)

target_link_libraries(sdrx r820dev)
target_link_libraries(sdrx m)
target_link_libraries(sdrx airspy-static)
target_link_libraries(sdrx rtlsdr_static)
target_link_libraries(sdrx usockets)

target_link_libraries(dts r820dev)
target_link_libraries(dts m)
target_link_libraries(dts airspy-static)
target_link_libraries(dts rtlsdr_static)

# Add support for installing our program
install(TARGETS sdrx RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

# Create empty install_manifest.txt so that it does not get owned by root on sudo make install
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

# Same but to manage unwanted uninstall target inside librtlsdr
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/librtlsdr/install_manifest.txt")

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

find_package(ZLIB REQUIRED)
target_link_libraries(usockets ZLIB::ZLIB)
find_package(OpenSSL REQUIRED)
target_link_libraries(usockets OpenSSL::SSL)

pkg_check_modules(LIBUV REQUIRED libuv)
include_directories(${LIBUV_INCLUDE_DIRS})
target_link_libraries(usockets ${LIBUV_LIBRARIES})

#find_package(Volk REQUIRED)
#if (VOLK_FOUND)
#    include_directories(${VOLK_INCLUDE_DIRS})
#    target_link_libraries (sdrx ${VOLK_LIBRARIES})
#endif(VOLK_FOUND)
