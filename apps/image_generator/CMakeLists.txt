# App 1: Image Generator
add_executable(image_generator
    main.cpp
    image_generator.cpp
)

target_link_libraries(image_generator
    PRIVATE
        common
)

# Set compile options
target_compile_options(image_generator PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install target
install(TARGETS image_generator
    RUNTIME DESTINATION bin
)
