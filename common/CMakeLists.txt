# Common library - shared code between all applications
add_library(common STATIC
    src/serialization.cpp
    src/ipc_manager.cpp
)

target_include_directories(common
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries(common
    PUBLIC
        ${ZMQ_LIBRARIES}
        Threads::Threads
)

# Set compile options
target_compile_options(common PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
