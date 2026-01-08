# Benchmark tests

# Only build benchmark if source files exist and benchmark library is available
# Note: benchmark library needs to be added via FetchContent or vcpkg

set(BENCHMARK_FILES
    fft_cache_benchmark.cpp
    playback_latency_benchmark.cpp
    memory_usage_benchmark.cpp
    throughput_benchmark.cpp
)

# Check if all benchmark files exist
set(ALL_BENCHMARK_FILES_EXIST TRUE)
foreach(file ${BENCHMARK_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
        set(ALL_BENCHMARK_FILES_EXIST FALSE)
        break()
    endif()
endforeach()

if(ALL_BENCHMARK_FILES_EXIST AND TARGET benchmark::benchmark)
    add_executable(performance_benchmarks ${BENCHMARK_FILES})
    target_link_libraries(performance_benchmarks
        xpu
        GTest::gtest
        GTest::gtest_main
        benchmark::benchmark
    )
    target_include_directories(performance_benchmarks PRIVATE ${CMAKE_SOURCE_DIR}/src/lib)
    add_test(NAME performance_benchmark_test COMMAND performance_benchmarks LABELS benchmark)
elseif(ALL_BENCHMARK_FILES_EXIST)
    message(WARNING "Benchmark library not found. Skipping benchmark tests.")
    message(WARNING "To enable benchmarks, install Google Benchmark via vcpkg or add to FetchContent.")
endif()
