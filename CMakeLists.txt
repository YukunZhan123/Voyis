cmake_minimum_required(VERSION 3.16)
project(DistributedImagingSystem VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(ZMQ REQUIRED libzmq)
find_package(OpenCV REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/common/include)

# Add subdirectories
add_subdirectory(common)
add_subdirectory(apps/image_generator)
add_subdirectory(apps/feature_extractor)
add_subdirectory(apps/data_logger)

# Testing
enable_testing()
add_subdirectory(tests)

# Print configuration summary
message(STATUS "")
message(STATUS "Configuration Summary:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  OpenCV Version: ${OpenCV_VERSION}")
message(STATUS "  ZeroMQ Found: ${ZMQ_FOUND}")
message(STATUS "  SQLite3 Found: ${SQLite3_FOUND}")
message(STATUS "")
