# Image Generator Application

add_executable(image_generator
    main.cpp
)

target_link_libraries(image_generator
    common
    ${ZMQ_LIBRARIES}
    Threads::Threads
    stdc++fs  # For filesystem support
)
