# App 3: Data Logger
add_executable(data_logger
    main.cpp
    data_logger.cpp
)

target_link_libraries(data_logger
    PRIVATE
        common
        SQLite::SQLite3
)

# Set compile options
target_compile_options(data_logger PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install target
install(TARGETS data_logger
    RUNTIME DESTINATION bin
)
