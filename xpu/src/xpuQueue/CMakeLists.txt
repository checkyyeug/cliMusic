# xpuQueue module - Queue management
add_executable(xpuQueue
    xpuQueue.cpp
    QueueManager.cpp
    # Add xpuLoad sources for AudioFileLoader and DSDDecoder
    ../xpuLoad/AudioFileLoader.cpp
    ../xpuLoad/DSDDecoder.cpp
)

# Link libraries
target_link_libraries(xpuQueue PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
)

# Include directories
target_include_directories(xpuQueue PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuQueue
    RUNTIME DESTINATION bin
)
