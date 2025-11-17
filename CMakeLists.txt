cmake_minimum_required(VERSION 3.14)
project(DistributedImagingServices VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(PkgConfig REQUIRED)

# ZeroMQ
pkg_check_modules(ZMQ REQUIRED libzmq)
include_directories(${ZMQ_INCLUDE_DIRS})
link_directories(${ZMQ_LIBRARY_DIRS})

# OpenCV
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# SQLite3
pkg_check_modules(SQLITE3 REQUIRED sqlite3)
include_directories(${SQLITE3_INCLUDE_DIRS})
link_directories(${SQLITE3_LIBRARY_DIRS})

# Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

# Common library (shared code)
add_subdirectory(src/common)

# Applications
add_subdirectory(src/image_generator)
add_subdirectory(src/feature_extractor)
add_subdirectory(src/data_logger)

# Testing
enable_testing()
add_subdirectory(tests)

# Print configuration summary
message(STATUS "=== Configuration Summary ===")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ZeroMQ Found: ${ZMQ_FOUND}")
message(STATUS "OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "SQLite3 Found: ${SQLITE3_FOUND}")
message(STATUS "============================")
