# xpuDaemon module - Background daemon with orchestration
add_executable(xpuDaemon
    xpuDaemon.cpp
    DaemonController.cpp
    ProcessManager.cpp
    OrchestrationManager.cpp
    ConfigWatcher.cpp
    StatePersistence.cpp
    MCPServer.cpp
)

# Link libraries
target_link_libraries(xpuDaemon PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
)

# Platform-specific linking
if(PLATFORM_LINUX)
    target_link_libraries(xpuDaemon PRIVATE
        pthread
    )
endif()

# Include directories
target_include_directories(xpuDaemon PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuDaemon
    RUNTIME DESTINATION bin
)

# Install systemd service file (Linux only)
if(PLATFORM_LINUX)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/xpu.service.in
        ${CMAKE_CURRENT_BINARY_DIR}/xpu.service
        @ONLY
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/xpu.service
        DESTINATION lib/systemd/system
    )
endif()

# Install launchd plist (macOS only)
if(PLATFORM_MACOS)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/com.xpu.daemon.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/com.xpu.daemon.plist
        @ONLY
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/com.xpu.daemon.plist
        DESTINATION ~/Library/LaunchAgents
    )
endif()
