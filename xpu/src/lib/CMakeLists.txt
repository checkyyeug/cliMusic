# Shared library (libxpu)
add_library(xpu STATIC
    protocol/ErrorCode.cpp
    protocol/ErrorResponse.cpp
    utils/ConfigLoader.cpp
    utils/ConfigValidator.cpp
    utils/Logger.cpp
    utils/PlatformUtils.cpp
    audio/AudioFormat.cpp
    audio/AudioMetadata.cpp
    audio/AudioProperties.cpp
    interfaces/IAudioFingerprint.cpp
    interfaces/IAudioClassifier.cpp
    interfaces/IAudioVisualizer.cpp
    interfaces/IAdvancedDSP.cpp
    interfaces/IMetadataProvider.cpp
    interfaces/IAudioStreamer.cpp
    interfaces/IDistributedCache.cpp
    interfaces/INetworkAudio.cpp
)

# Set output directories
set_target_properties(xpu PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE}"
)

# Link libraries
target_link_libraries(xpu PUBLIC
    Threads::Threads
    ${FFMPEG_LIBRARIES}
    ${PORTAUDIO_LIBRARIES}
    ${FFTW3_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(xpu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FFMPEG_INCLUDE_DIRS}
    ${PORTAUDIO_INCLUDE_DIRS}
    ${FFTW3_INCLUDE_DIRS}
    ${SAMPLERATE_INCLUDE_DIRS}
)

# Compiler definitions
target_compile_definitions(xpu PUBLIC
    XPU_VERSION_MAJOR=${xpu_VERSION_MAJOR}
    XPU_VERSION_MINOR=${xpu_VERSION_MINOR}
    XPU_VERSION_PATCH=${xpu_VERSION_PATCH}
)

# Platform-specific linking
if(PLATFORM_LINUX)
    target_link_libraries(xpu PUBLIC asound)
elseif(PLATFORM_MACOS)
    find_library(COREAUDIO_LIBRARY CoreAudio)
    find_library(AUDIOUNIT_LIBRARY AudioUnit)
    find_library(AUDIOTOOLBOX_LIBRARY AudioToolbox)
    target_link_libraries(xpu PUBLIC
        ${COREAUDIO_LIBRARY}
        ${AUDIOUNIT_LIBRARY}
        ${AUDIOTOOLBOX_LIBRARY}
    )
elseif(PLATFORM_WINDOWS)
    target_link_libraries(xpu PUBLIC
        ole32
        winmm
    )
endif()

# Installation
install(TARGETS xpu
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
)

install(FILES
    protocol/ErrorCode.h
    protocol/ErrorResponse.h
    protocol/Protocol.h
    utils/ConfigLoader.h
    utils/ConfigValidator.h
    utils/Logger.h
    utils/PlatformUtils.h
    audio/AudioFormat.h
    audio/AudioMetadata.h
    audio/AudioProperties.h
    interfaces/IAudioFingerprint.h
    interfaces/IAudioClassifier.h
    interfaces/IAudioVisualizer.h
    interfaces/IAdvancedDSP.h
    interfaces/IMetadataProvider.h
    interfaces/IAudioStreamer.h
    interfaces/IDistributedCache.h
    interfaces/INetworkAudio.h
    interfaces/FeatureStatus.h
    DESTINATION include/xpu
)
