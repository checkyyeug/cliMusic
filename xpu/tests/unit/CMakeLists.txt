# Unit tests

# Only build test targets if the source files exist

# Protocol tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_Protocol.cpp")
    add_executable(test_Protocol test_Protocol.cpp)
    target_link_libraries(test_Protocol
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_Protocol PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_Protocol COMMAND test_Protocol LABELS unit)
endif()

# AudioWrappers tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_AudioWrappers.cpp")
    add_executable(test_AudioWrappers test_AudioWrappers.cpp)
    target_link_libraries(test_AudioWrappers
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_AudioWrappers PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_AudioWrappers COMMAND test_AudioWrappers LABELS unit)
endif()

# PlatformUtils tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_PlatformUtils.cpp")
    add_executable(test_PlatformUtils test_PlatformUtils.cpp)
    target_link_libraries(test_PlatformUtils
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_PlatformUtils PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_PlatformUtils COMMAND test_PlatformUtils LABELS unit)
endif()

# Daemon tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_Daemon.cpp")
    add_executable(test_Daemon test_Daemon.cpp)
    target_link_libraries(test_Daemon
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_Daemon PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_Daemon COMMAND test_Daemon LABELS unit)
endif()

# InterfaceCompatibility tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_InterfaceCompatibility.cpp")
    add_executable(test_InterfaceCompatibility test_InterfaceCompatibility.cpp)
    target_link_libraries(test_InterfaceCompatibility
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_InterfaceCompatibility PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_InterfaceCompatibility COMMAND test_InterfaceCompatibility LABELS unit)
endif()

# AudioFileLoader tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_AudioFileLoader.cpp")
    add_executable(test_AudioFileLoader test_AudioFileLoader.cpp)
    target_link_libraries(test_AudioFileLoader
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_AudioFileLoader PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_AudioFileLoader COMMAND test_AudioFileLoader LABELS unit)
endif()

# DSPEffects tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_DSPEffects.cpp")
    add_executable(test_DSPEffects test_DSPEffects.cpp)
    target_link_libraries(test_DSPEffects
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_DSPEffects PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib
        ${CMAKE_SOURCE_DIR}/src/xpuProcess
    )
    add_test(NAME test_DSPEffects COMMAND test_DSPEffects LABELS unit)
endif()

# FFTEngine tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_FFTEngine.cpp")
    add_executable(test_FFTEngine test_FFTEngine.cpp)
    target_link_libraries(test_FFTEngine
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_FFTEngine PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME test_FFTEngine COMMAND test_FFTEngine LABELS unit)
endif()

# QueueManager tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_QueueManager.cpp")
    add_executable(test_QueueManager test_QueueManager.cpp)
    target_link_libraries(test_QueueManager
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(test_QueueManager PRIVATE
        ${CMAKE_SOURCE_DIR}/src/lib
        ${CMAKE_SOURCE_DIR}/src/xpuQueue
        ${CMAKE_SOURCE_DIR}/src/xpuLoad
    )
    add_test(NAME test_QueueManager COMMAND test_QueueManager LABELS unit)
endif()
