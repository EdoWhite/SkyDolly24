find_package(Qt6Test REQUIRED)

## HttpRequest Test ##
set(TEST_NAME "HttpRequestTest")

qt_add_executable(${TEST_NAME})

target_sources(${TEST_NAME}
    PRIVATE
        src/${TEST_NAME}.h src/${TEST_NAME}.cpp
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        Qt6::Test
        Sky::Remote
)
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
