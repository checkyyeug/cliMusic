# XPU Test Suite

# Enable testing
enable_testing()

# GoogleTest setup - use FetchContent version if available
if(BUILD_TESTING)
    # The GTest::gtest target should be available from FetchContent in parent CMakeLists.txt
    if(TARGET GTest::gtest OR TARGET gtest)
        # Use the FetchContent version
        message(STATUS "Using FetchContent GoogleTest")
    elseif(TARGET GTest::GTest OR TARGET GTest::gtest_main)
        # Alternative target names from newer GoogleTest versions
        message(STATUS "Using GoogleTest (alternative target)")
    else()
        # Try to find system GoogleTest as fallback
        find_package(GTest QUIET)
        if(NOT GTest_FOUND)
            message(FATAL_ERROR "GoogleTest not found. Please either install GoogleTest or ensure FetchContent is working.\n"
                "If using FetchContent, check that GIT is available and network connectivity is working.")
        endif()
    endif()

    # Unit tests
    add_subdirectory(unit)

    # Integration tests
    add_subdirectory(integration)

    # Performance tests
    add_subdirectory(performance)

    # Error handling tests
    add_subdirectory(error)

    # Cross-platform tests
    add_subdirectory(crossplatform)

    # Contract tests (legacy)
    add_subdirectory(contract)

    # Benchmark tests (legacy)
    add_subdirectory(benchmark)
endif()

# Custom test targets
add_custom_target(test_units
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L unit
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_integration
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L integration
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_performance
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L performance
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_error
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L error
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_crossplatform
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L crossplatform
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_contract
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L contract
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_benchmark
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -L benchmark
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(test_all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS xpuLoad xpuIn2Wav xpuPlay xpuQueue xpuProcess xpuDaemon
    USES_TERMINAL
)

add_custom_target(run_comprehensive_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS
        test_Protocol
        test_PlatformUtils
        test_AudioWrappers
        test_Daemon
        test_InterfaceCompatibility
        test_PipelineIntegration
        test_QueueIntegration
        test_DaemonIntegration
        test_Performance
        test_ErrorHandling
        test_CrossPlatform
    USES_TERMINAL
    COMMENT "Running comprehensive XPU test suite..."
)
