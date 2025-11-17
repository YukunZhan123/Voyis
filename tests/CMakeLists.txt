# Unit Tests

# Download and configure Google Test
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Test executable
add_executable(unit_tests
    test_message.cpp
    test_ipc.cpp
)

target_link_libraries(unit_tests
    common
    ${ZMQ_LIBRARIES}
    ${OpenCV_LIBS}
    gtest_main
    gmock_main
    Threads::Threads
)

include(GoogleTest)
gtest_discover_tests(unit_tests)
