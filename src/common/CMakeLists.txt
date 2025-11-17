# Common library shared by all applications

add_library(common STATIC
    message.cpp
    ipc.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(common
    ${ZMQ_LIBRARIES}
    Threads::Threads
)
