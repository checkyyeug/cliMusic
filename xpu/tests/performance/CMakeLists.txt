# Performance Tests

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_Performance.cpp")
    add_executable(test_Performance test_Performance.cpp)

    target_link_libraries(test_Performance
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        xpu
    )

    target_include_directories(test_Performance
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME test_Performance COMMAND test_Performance)
    set_tests_properties(test_Performance PROPERTIES LABELS "performance")
endif()
