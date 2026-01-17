# xpuLoad module - Audio file parser
add_executable(xpuLoad
    xpuLoad.cpp
    AudioFileLoader.cpp
    SACDDecoder.cpp
    DSDDecoder.cpp
    MetadataExtractor.cpp
    CLIInterface.cpp
)

# Link libraries
target_link_libraries(xpuLoad PRIVATE
    xpu
    Threads::Threads
    ${FFMPEG_LIBRARIES}
)

# Include directories
target_include_directories(xpuLoad PRIVATE
    ${CMAKE_SOURCE_DIR}/src/lib
    ${FFMPEG_INCLUDE_DIRS}
)

# Installation
install(TARGETS xpuLoad
    RUNTIME DESTINATION bin
)
