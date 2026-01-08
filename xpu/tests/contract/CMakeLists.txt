# Contract tests

# Only build test targets if the source files exist
# These are legacy tests - may not have source files yet

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/protocol_test.cpp")
    add_executable(protocol_contract_tests protocol_test.cpp)
    target_link_libraries(protocol_contract_tests
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(protocol_contract_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME protocol_contract_test COMMAND protocol_contract_tests LABELS contract)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/interface_test.cpp")
    add_executable(interface_contract_tests interface_test.cpp)
    target_link_libraries(interface_contract_tests
        xpu
        GTest::gtest
        GTest::gtest_main
    )
    target_include_directories(interface_contract_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME interface_contract_test COMMAND interface_contract_tests LABELS contract)
endif()
