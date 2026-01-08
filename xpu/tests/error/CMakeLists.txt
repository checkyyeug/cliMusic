# Error Handling Tests

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_ErrorHandling.cpp")
    add_executable(test_ErrorHandling test_ErrorHandling.cpp)

    target_link_libraries(test_ErrorHandling
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        xpu
    )

    target_include_directories(test_ErrorHandling
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME test_ErrorHandling COMMAND test_ErrorHandling)
    set_tests_properties(test_ErrorHandling PROPERTIES LABELS "error")
endif()
