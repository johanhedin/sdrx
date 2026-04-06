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

# Default build type is Release. Run cmake with -DCMAKE_BUILD_TYPE=Debug to
# build in debug mode
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build type not specified, defaulting to Release. Change to debug with -DCMAKE_BUILD_TYPE=Debug")
endif()

# Create a uninstall target if it does not already exist. We need this before
# any add_subdirectory so that uninstall targets from them do not propagate here.
# https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
    configure_file("${PROJECT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" @ONLY)
    add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

# We need the PkgConfig cmake module to be able to find libraries we depend on
find_package(PkgConfig REQUIRED)

find_package(Threads REQUIRED)

# Use GNUInstallDirs to get standard install directories for include, lib, bin and so on
include(GNUInstallDirs)

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
option(DETACH_KERNEL_DRIVER "Detach kernel driver on RTL-SDR device open" ON)
add_subdirectory(librtlsdr EXCLUDE_FROM_ALL)

# Find all pkg-config dependencies up front so imported targets are available
# to all targets below
pkg_check_modules(POPT REQUIRED IMPORTED_TARGET popt)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)
pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
pkg_check_modules(SIGCPP2 REQUIRED IMPORTED_TARGET sigc++-2.0)
pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
pkg_check_modules(LIBUV REQUIRED IMPORTED_TARGET libuv)
pkg_check_modules(ZLIB REQUIRED IMPORTED_TARGET zlib)
pkg_check_modules(OPENSSL REQUIRED IMPORTED_TARGET openssl>=3)

# Our device/dongle module
file(GLOB R820DEV_SRCS CONFIGURE_DEPENDS
    "src/r820_dev.cpp"
    "src/rtl_dev.cpp"
    "src/airspy_dev.cpp"
)
add_library(r820dev ${R820DEV_SRCS})
target_compile_options(r820dev PRIVATE -Wall -Wextra -pedantic)
target_include_directories(r820dev PRIVATE ${PROJECT_SOURCE_DIR}/libairspy/libairspy/src)
target_include_directories(r820dev PRIVATE ${PROJECT_SOURCE_DIR}/librtlsdr/include)
target_link_libraries(r820dev PkgConfig::SIGCPP2)

# We build the bundled uSockets ourselves
file(GLOB USOCKET_SRCS CONFIGURE_DEPENDS
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
target_compile_definitions(usockets PRIVATE LIBUS_USE_OPENSSL LIBUS_USE_LIBUV)
target_include_directories(usockets PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/uSockets/src)

# Main program
add_executable(${CMAKE_PROJECT_NAME} "src/sdrx.cpp")
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic -march=native -mtune=native)
# Extra flags for reporting around vectorization are:
#   -ftree-vectorize -fopt-info-vec-missed -ftree-vectorizer-verbose=7
#   -fopt-info-vec -fopt-info-vec-note

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/uSockets/src)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/uWebSockets/src)

target_link_libraries(${CMAKE_PROJECT_NAME} r820dev)
target_link_libraries(${CMAKE_PROJECT_NAME} m)
target_link_libraries(${CMAKE_PROJECT_NAME} airspy-static)
target_link_libraries(${CMAKE_PROJECT_NAME} rtlsdr_static)
target_link_libraries(${CMAKE_PROJECT_NAME} usockets)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::POPT)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::FFTW3F)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::LIBUSB)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::SIGCPP2)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::ALSA)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::LIBUV)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::ZLIB)
target_link_libraries(${CMAKE_PROJECT_NAME} PkgConfig::OPENSSL)

# We don't use the dts tool for the time being
#add_executable(dts EXCLUDE_FROM_ALL src/dts.cpp)
#target_include_directories(dts PRIVATE ${PROJECT_SOURCE_DIR})
#target_link_libraries(dts r820dev)
#target_link_libraries(dts m)
#target_link_libraries(dts airspy-static)
#target_link_libraries(dts rtlsdr_static)

# CRB lock-free SPSC queue test (build with: cmake --build build --target crb_test)
add_executable(crb_test EXCLUDE_FROM_ALL src/crb_test.cpp)
target_compile_options(crb_test PRIVATE -Wall -Wextra -pedantic -O2)
target_include_directories(crb_test PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(crb_test Threads::Threads)

# Add support for installing our program
install(TARGETS ${CMAKE_PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Create empty install_manifest.txt so that it does not get owned by root on sudo make install
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

# Same but to manage unwanted uninstall target inside librtlsdr
execute_process(COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/librtlsdr/install_manifest.txt")
