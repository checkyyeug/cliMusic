# xpuPlay module - Low-latency audio output
add_executable(xpuPlay
    xpuPlay.cpp
    AudioBackend.cpp
    AudioBackendFactory.cpp
)

# Platform-specific sources
if(PLATFORM_WINDOWS)
    target_sources(xpuPlay PRIVATE
        AudioBackend_WASAPI.cpp
    )
elseif(PLATFORM_MACOS)
    target_sources(xpuPlay PRIVATE
        AudioBackend_CoreAudio.cpp
    )
elseif(PLATFORM_LINUX)
    target_sources(xpuPlay PRIVATE
        AudioBackend_ALSA.cpp
    )
endif()

# Link libraries
target_link_libraries(xpuPlay PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
)

# Platform-specific linking
if(PLATFORM_LINUX)
    target_link_libraries(xpuPlay PRIVATE asound)
elseif(PLATFORM_MACOS)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    target_link_libraries(xpuPlay PRIVATE
        ${COREAUDIO_LIBRARY}
        ${AUDIOUNIT_LIBRARY}
        ${AUDIOTOOLBOX_LIBRARY}
    )
elseif(PLATFORM_WINDOWS)
    target_link_libraries(xpuPlay PRIVATE
        ole32
        winmm
    )
endif()

# Include directories
target_include_directories(xpuPlay PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuPlay
    RUNTIME DESTINATION bin
)
