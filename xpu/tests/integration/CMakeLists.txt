# Integration tests

# Only build test targets if the source files exist

# PipelineIntegration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_PipelineIntegration.cpp")
    add_executable(test_PipelineIntegration test_PipelineIntegration.cpp)
    target_link_libraries(test_PipelineIntegration
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_PipelineIntegration PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_PipelineIntegration COMMAND test_PipelineIntegration LABELS integration)
endif()

# Pipeline tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_Pipeline.cpp")
    add_executable(test_Pipeline test_Pipeline.cpp)
    target_link_libraries(test_Pipeline
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_Pipeline PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_Pipeline COMMAND test_Pipeline LABELS integration)
endif()

# QueueIntegration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_QueueIntegration.cpp")
    add_executable(test_QueueIntegration test_QueueIntegration.cpp)
    target_link_libraries(test_QueueIntegration
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_QueueIntegration PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_QueueIntegration COMMAND test_QueueIntegration LABELS integration)
endif()

# DaemonIntegration tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_DaemonIntegration.cpp")
    add_executable(test_DaemonIntegration test_DaemonIntegration.cpp)
    target_link_libraries(test_DaemonIntegration
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_DaemonIntegration PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_DaemonIntegration COMMAND test_DaemonIntegration LABELS integration)
endif()
