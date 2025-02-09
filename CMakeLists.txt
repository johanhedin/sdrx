cmake_minimum_required(VERSION 3.10...3.27)
project(sdrx C CXX)

# We use C17 and C++20
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
#set(CMAKE_CXX_FLAGS "-Wall -Wextra -march=native -mtune=native")
#set(CMAKE_CXX_FLAGS_DEBUG "-g -O2")
#set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#set(CMAKE_C_FLAGS "-Wall -Wextra -march=native -mtune=native")
#set(CMAKE_C_FLAGS_DEBUG "-g -O2")
#set(CMAKE_C_FLAGS_RELEASE "-O3")

FILE(GLOB R820DEV_SRCS
    "src/r820_dev.cpp"
    "src/rtl_dev.cpp"
    "src/airspy_dev.cpp"
)
add_library(r820dev ${R820DEV_SRCS})
target_compile_options(r820dev PRIVATE -Wall)
target_compile_options(r820dev PRIVATE -Wextra)
target_compile_options(r820dev PRIVATE -pedantic)

target_include_directories(r820dev PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(r820dev PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)

#add_executable(sdrx src/sdrx.cpp)
#add_executable(dts EXCLUDE_FROM_ALL src/dts.cpp)


# We build the bundled uSockets ourselves
FILE(GLOB USOCKET_SRCS
    "uWebSockets/uSockets/src/bsd.c"
    "uWebSockets/uSockets/src/context.c"
    "uWebSockets/uSockets/src/loop.c"
    "uWebSockets/uSockets/src/socket.c"
    "uWebSockets/uSockets/src/eventing/libuv.c"
    "uWebSockets/uSockets/src/crypto/openssl.c"
    "uWebSockets/uSockets/src/crypto/sni_tree.cpp"
)
add_library(usockets ${USOCKET_SRCS})
target_compile_options(usockets PRIVATE -Wno-deprecated-declarations)
target_compile_options(usockets PRIVATE -DLIBUS_USE_OPENSSL)
target_compile_options(usockets PRIVATE -DLIBUS_USE_LIBUV)

target_include_directories(usockets PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/uSockets/src)

# Main program
FILE(GLOB SRCS
    "src/sdrx.cpp"
)
add_executable(${CMAKE_PROJECT_NAME} ${SRCS})
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wextra)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -pedantic)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -march=native)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -mtune=native)


# Add our project root and the linked libairspy/librtlsdr as include directories
# TODO: Look over this and make it possible to build with both in-tree or
# system libs. Need to adjust the includes in rtl_dev.cpp and airspy_dev.cpp
# as well
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/uSockets/src)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/src)

#target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR})
#target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
#target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)

target_link_libraries(${CMAKE_PROJECT_NAME} r820dev)
target_link_libraries(${CMAKE_PROJECT_NAME} m)
target_link_libraries(${CMAKE_PROJECT_NAME} airspy-static)
target_link_libraries(${CMAKE_PROJECT_NAME} rtlsdr_static)
target_link_libraries(${CMAKE_PROJECT_NAME} usockets)

#target_link_libraries(dts r820dev)
#target_link_libraries(dts m)
#target_link_libraries(dts airspy-static)
#target_link_libraries(dts rtlsdr_static)

# Add support for installing our program
install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})

# Create empty install_manifest.txt so that it does not get owned by root on sudo make install
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

# Same but to manage unwanted uninstall target inside librtlsdr
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/librtlsdr/install_manifest.txt")

pkg_check_modules(POPT REQUIRED popt)
include_directories(${POPT_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${POPT_LIBRARIES})

pkg_check_modules(FFTW3F REQUIRED fftw3f)
include_directories(${FFTW3F_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${FFTW3F_LIBRARIES})

pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
include_directories(${LIBUSB_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBUSB_LIBRARIES})

pkg_check_modules(SIGCPP2 REQUIRED sigc++-2.0)
include_directories(${SIGCPP2_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${SIGCPP2_LIBRARIES})

pkg_check_modules(ALSA REQUIRED alsa)
include_directories(${ALSA_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${ALSA_LIBRARIES})

pkg_check_modules(LIBUV REQUIRED libuv)
include_directories(${LIBUV_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LIBUV_LIBRARIES})

pkg_check_modules(ZLIB REQUIRED zlib)
include_directories(${ZLIB_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${ZLIB_LIBRARIES})

pkg_check_modules(OPENSSL REQUIRED openssl>=3)
include_directories(${OPENSSL_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} ${OPENSSL_LIBRARIES})


# Look for all required packages
#find_package(POPT REQUIRED)
#if (POPT_FOUND)
#    include_directories(${POPT_INCLUDE_DIRS})
#    target_link_libraries (${CMAKE_PROJECT_NAME} ${POPT_LIBRARIES})
#    target_link_libraries (dts ${POPT_LIBRARIES})
#endif(POPT_FOUND)

#find_package(Threads REQUIRED)
#if (Threads_FOUND)
#    include_directories(${THREADS_PTHREADS_INCLUDE_DIR})
#    target_link_libraries (${CMAKE_PROJECT_NAME} Threads::Threads)
#    target_link_libraries (dts Threads::Threads)
#endif(Threads_FOUND)

#find_package(FFTW REQUIRED)
#if (FFTW_FLOAT_LIB_FOUND)
#    include_directories(${FFTW_INCLUDE_DIRS})
#    target_link_libraries (${CMAKE_PROJECT_NAME} ${FFTW_FLOAT_LIB})
#endif(FFTW_FLOAT_LIB_FOUND)

#find_package(LIBUSB REQUIRED)
#if (LIBUSB_FOUND)
#    include_directories(${LIBUSB_INCLUDE_DIRS})
#    target_link_libraries (${CMAKE_PROJECT_NAME} ${LIBUSB_LIBRARIES})
#    target_link_libraries (dts ${LIBUSB_LIBRARIES})
#endif(LIBUSB_FOUND)

#find_package(ALSA REQUIRED)
#if (ALSA_FOUND)
#    include_directories(${ALSA_INCLUDE_DIRS})
#    target_link_libraries (${CMAKE_PROJECT_NAME} ${ALSA_LIBRARIES})
#endif(ALSA_FOUND)

#find_package(PkgConfig REQUIRED)
#if (PkgConfig_FOUND)
#    pkg_check_modules(SIGC2 REQUIRED sigc++-2.0)
#    include_directories(${SIGC2_INCLUDE_DIRS})
#    target_link_libraries(${CMAKE_PROJECT_NAME} ${SIGC2_LIBRARIES})
#    target_link_libraries(dts ${SIGC2_LIBRARIES})
#endif(PkgConfig_FOUND)

#find_package(ZLIB REQUIRED)
#target_link_libraries(usockets ZLIB::ZLIB)
#find_package(OpenSSL REQUIRED)
#target_link_libraries(usockets OpenSSL::SSL)

#pkg_check_modules(LIBUV REQUIRED libuv)
#include_directories(${LIBUV_INCLUDE_DIRS})
#target_link_libraries(usockets ${LIBUV_LIBRARIES})

#find_package(Volk REQUIRED)
#if (VOLK_FOUND)
#    include_directories(${VOLK_INCLUDE_DIRS})
#    target_link_libraries (${CMAKE_PROJECT_NAME} ${VOLK_LIBRARIES})
#endif(VOLK_FOUND)
