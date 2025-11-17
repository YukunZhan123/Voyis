# App 2: Feature Extractor
add_executable(feature_extractor
    main.cpp
    feature_extractor.cpp
)

target_link_libraries(feature_extractor
    PRIVATE
        common
        ${OpenCV_LIBS}
)

# Set compile options
target_compile_options(feature_extractor PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install target
install(TARGETS feature_extractor
    RUNTIME DESTINATION bin
)
