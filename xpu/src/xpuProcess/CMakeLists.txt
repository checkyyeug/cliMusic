# xpuProcess module - Basic DSP processing
add_executable(xpuProcess
    xpuProcess.cpp
    VolumeControl.cpp
    FadeEffects.cpp
    Equalizer.cpp
)

# Link libraries
target_link_libraries(xpuProcess PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
)

# Include directories
target_include_directories(xpuProcess PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuProcess
    RUNTIME DESTINATION bin
)
