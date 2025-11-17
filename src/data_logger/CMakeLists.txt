# Data Logger Application

add_executable(data_logger
    main.cpp
)

target_link_libraries(data_logger
    common
    ${ZMQ_LIBRARIES}
    ${SQLITE3_LIBRARIES}
    Threads::Threads
)
