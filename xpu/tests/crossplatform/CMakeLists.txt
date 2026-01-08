# Cross-Platform Tests

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_CrossPlatform.cpp")
    add_executable(test_CrossPlatform test_CrossPlatform.cpp)

    target_link_libraries(test_CrossPlatform
        PRIVATE
        GTest::gtest
        GTest::gtest_main
        xpu
    )

    target_include_directories(test_CrossPlatform
        PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    add_test(NAME test_CrossPlatform COMMAND test_CrossPlatform)
    set_tests_properties(test_CrossPlatform PROPERTIES LABELS "crossplatform")
endif()
