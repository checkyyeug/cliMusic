# xpuIn2Wav module - Format converter + FFT cache
add_executable(xpuIn2Wav
    xpuIn2Wav.cpp
    FormatConverter.cpp
    FFTEngine.cpp
    CacheManager.cpp
    CLIInterface.cpp
    # Add xpuLoad sources for AudioFileLoader and DSDDecoder
    ../xpuLoad/AudioFileLoader.cpp
    ../xpuLoad/DSDDecoder.cpp
)

# Link libraries
target_link_libraries(xpuIn2Wav PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
    ${FFTW3_LIBRARIES}
    fftw3f
    ${SAMPLERATE_LIBRARIES}
)

# Include directories
target_include_directories(xpuIn2Wav PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuIn2Wav
    RUNTIME DESTINATION bin
)
