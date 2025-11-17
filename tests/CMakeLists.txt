# Find Google Test
find_package(GTest REQUIRED)
include(GoogleTest)

# Test for serialization
add_executable(test_serialization
    unit/test_serialization.cpp
)

target_link_libraries(test_serialization
    PRIVATE
        common
        GTest::GTest
        GTest::Main
)

gtest_discover_tests(test_serialization)

# Test for IPC
add_executable(test_ipc
    unit/test_ipc.cpp
)

target_link_libraries(test_ipc
    PRIVATE
        common
        GTest::GTest
        GTest::Main
)

gtest_discover_tests(test_ipc)

# Integration test
add_executable(test_integration
    integration/test_integration.cpp
)

target_link_libraries(test_integration
    PRIVATE
        common
        GTest::GTest
        GTest::Main
)

gtest_discover_tests(test_integration)
