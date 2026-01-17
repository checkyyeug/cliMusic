# xpuApi - REST API Server for XPU
# Phase 2: SSE-based streaming architecture

# cpp-httplib is a header-only library
# We'll use it directly from the include directory
# Download it to a known location if not already present

set(HTTPLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-httplib")

if(NOT EXISTS "${HTTPLIB_DIR}")
    message(STATUS "Downloading cpp-httplib...")
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch v0.15.3 https://github.com/yhirose/cpp-httplib.git ${HTTPLIB_DIR}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party
            RESULT_VARIABLE HTTPLIB_CLONE_RESULT
        )
        if(NOT HTTPLIB_CLONE_RESULT EQUAL "0")
            message(WARNING "Failed to clone cpp-httplib. SSE streaming may not work.")
        endif()
    else()
        message(WARNING "Git not found. Cannot download cpp-httplib.")
    endif()
endif()

# Create executable
add_executable(xpuApi
    xpuApi.cpp
    APIServer.cpp
)

# Link libraries
target_link_libraries(xpuApi
    PRIVATE
        xpu
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        Threads::Threads
)

# Include directories
if(EXISTS "${HTTPLIB_DIR}")
    target_include_directories(xpuApi PRIVATE ${HTTPLIB_DIR})
    target_compile_definitions(xpuApi PRIVATE CPP_HTTPLIB_ENABLED)
    message(STATUS "cpp-httplib found, SSE streaming enabled")
else()
    message(WARNING "cpp-httplib not found. Building with limited functionality.")
endif()

# Platform-specific libraries
if(UNIX AND NOT APPLE)
    target_link_libraries(xpuApi PRIVATE pthread)
endif()

# Windows-specific libraries
if(WIN32)
    target_link_libraries(xpuApi PRIVATE ws2_32 crypt32)
endif()

# Installation
install(TARGETS xpuApi
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

message(STATUS "xpuApi module configured")
