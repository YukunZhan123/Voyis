# Feature Extractor Application

add_executable(feature_extractor
    main.cpp
)

target_link_libraries(feature_extractor
    common
    ${ZMQ_LIBRARIES}
    ${OpenCV_LIBS}
    Threads::Threads
)
