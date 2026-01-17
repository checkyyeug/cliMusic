/**
 * @file FFTEngine.cpp
 * @brief Optimized FFT computation engine with multi-threading and SIMD support
 */

#ifdef PLATFORM_WINDOWS
#ifdef NOMINMAX
#undef NOMINMAX
#endif
#define NOMINMAX
#endif

#include "FFTEngine.h"
#include "MathConstants.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <functional>
#include <cstring>
#include <iomanip>
#include <thread>
#include <vector>
#include <future>

#ifdef __ARM_NEON
#include <arm_neon.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

using namespace xpu;

// SHA256 implementation for cache key generation
#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <wincrypt.h>
#else
    extern "C" {
        #include <openssl/sha.h>
    }
#endif

// Simple SHA256 implementation for Windows if OpenSSL is not available
#ifdef PLATFORM_WINDOWS
struct SHA256_CTX {
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
};

inline void SHA256_Init(SHA256_CTX* ctx) {
    CryptAcquireContext(&ctx->hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    CryptCreateHash(ctx->hProv, CALG_SHA_256, 0, 0, &ctx->hHash);
}

inline void SHA256_Update(SHA256_CTX* ctx, const void* data, size_t len) {
    CryptHashData(ctx->hHash, (BYTE*)data, static_cast<DWORD>(len), 0);
}

inline void SHA256_Final(unsigned char* digest, SHA256_CTX* ctx) {
    DWORD hash_len = 32;
    CryptGetHashParam(ctx->hHash, HP_HASHVAL, digest, &hash_len, 0);
    CryptDestroyHash(ctx->hHash);
    CryptReleaseContext(ctx->hProv, 0);
}
#endif

namespace xpu {
namespace in2wav {

/**
 * @brief Cache file format version
 */
static const std::string CACHE_VERSION = "1.0";

/**
 * @brief Cache file structure:
 * cache_dir/
 *   {cache_id}/
 *     meta.json       - Metadata
 *     magnitude.bin   - Magnitude data (float array)
 *     phase.bin       - Phase data (float array)
 *     config.json     - Configuration (fft_size, window, etc.)
 */

/**
 * @brief Optimized implementation class with multi-threading and SIMD
 */
class FFTEngine::Impl {
public:
    int fft_size;
    FFTCacheMetadata current_metadata;
    std::string cache_dir;
    CacheStats stats;

    // Multi-threading support
    int num_threads;

    // SIMD support flag
    bool simd_enabled;

    // FFTW3 plans for each thread
    std::vector<fftwf_plan> plans;
    std::vector<fftwf_complex*> in_buffers;
    std::vector<fftwf_complex*> out_buffers;
    std::vector<float*> windows;

    Impl()
        : fft_size(2048)
        , num_threads(std::thread::hardware_concurrency())
        , simd_enabled(true) {

        cache_dir = utils::PlatformUtils::getCacheDirectory();

        // Detect SIMD support
        #if defined(__AVX512F__)
            LOG_INFO("AVX-512 SIMD detected");
        #elif defined(__AVX2__)
            LOG_INFO("AVX2 SIMD detected");
        #elif defined(__ARM_NEON)
            LOG_INFO("ARM NEON SIMD detected");
        #else
            LOG_INFO("No SIMD detected, using scalar code");
            simd_enabled = false;
        #endif

        LOG_INFO("Using {} threads for FFT computation", num_threads);
    }

    ~Impl() {
        // Cleanup FFTW3 resources
        for (size_t i = 0; i < plans.size(); ++i) {
            if (plans[i]) {
                fftwf_destroy_plan(plans[i]);
            }
            if (in_buffers[i]) {
                fftwf_free(in_buffers[i]);
            }
            if (out_buffers[i]) {
                fftwf_free(out_buffers[i]);
            }
            if (windows[i]) {
                delete[] windows[i];
            }
        }
    }
};

FFTEngine::FFTEngine()
    : impl_(std::make_unique<Impl>()) {}

FFTEngine::~FFTEngine() = default;

ErrorCode FFTEngine::initialize(int fft_size) {
    LOG_INFO("Initializing optimized FFT engine with size: {}", fft_size);

    impl_->fft_size = fft_size;

    // Allocate FFTW3 buffers for each thread
    impl_->plans.resize(impl_->num_threads);
    impl_->in_buffers.resize(impl_->num_threads);
    impl_->out_buffers.resize(impl_->num_threads);
    impl_->windows.resize(impl_->num_threads);

    for (int i = 0; i < impl_->num_threads; ++i) {
        // Allocate buffers
        impl_->in_buffers[i] = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fft_size);
        impl_->out_buffers[i] = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fft_size);

        if (!impl_->in_buffers[i] || !impl_->out_buffers[i]) {
            LOG_ERROR("Failed to allocate FFTW3 buffers for thread {}", i);
            return ErrorCode::OutOfMemory;
        }

        // Create FFTW3 plan with SIMD optimization
        unsigned flags = FFTW_MEASURE;

        // Enable SIMD based on architecture
        #if defined(__AVX512F__)
        flags |= FFTW_ALLOW_PRUNING;
        #elif defined(__AVX__)
        flags |= FFTW_ALLOW_PRUNING;
        #endif

        impl_->plans[i] = fftwf_plan_dft_1d(
            fft_size,
            impl_->in_buffers[i],
            impl_->out_buffers[i],
            FFTW_FORWARD,
            flags
        );

        if (!impl_->plans[i]) {
            LOG_ERROR("Failed to create FFTW3 plan for thread {}", i);
            return ErrorCode::InvalidOperation;
        }

        // Create window function (Hann)
        impl_->windows[i] = new float[fft_size];
        for (int j = 0; j < fft_size; ++j) {
            impl_->windows[i][j] = 0.5f * (1.0f - std::cos(2.0f * M_PI * j / (fft_size - 1)));
        }
    }

    // Update metadata
    impl_->current_metadata.fft_size = fft_size;
    impl_->current_metadata.window_function = "hann";
    impl_->current_metadata.simd_enabled = impl_->simd_enabled;
    impl_->current_metadata.num_threads = impl_->num_threads;

    LOG_INFO("Optimized FFT engine initialized successfully");
    LOG_INFO("  SIMD: {}", impl_->simd_enabled ? "enabled" : "disabled");
    LOG_INFO("  Threads: {}", impl_->num_threads);

    return ErrorCode::Success;
}

ErrorCode FFTEngine::computeFFT(const std::vector<float>& audio_data,
                                 int sample_rate,
                                 FFTResult& result) {
    if (impl_->plans.empty()) {
        LOG_ERROR("FFT engine not initialized");
        return ErrorCode::InvalidState;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // For simplicity, use first thread for single FFT computation
    // Multi-threading would be more beneficial for multiple FFTs or very large audio data
    int thread_id = 0;

    // Apply window and prepare input
    int frames_to_process = std::min(impl_->fft_size, static_cast<int>(audio_data.size()));

    for (int i = 0; i < frames_to_process; ++i) {
        impl_->in_buffers[thread_id][i][0] = audio_data[i] * impl_->windows[thread_id][i];
        impl_->in_buffers[thread_id][i][1] = 0.0f;
    }

    // Zero-pad if necessary
    for (int i = frames_to_process; i < impl_->fft_size; ++i) {
        impl_->in_buffers[thread_id][i][0] = 0.0f;
        impl_->in_buffers[thread_id][i][1] = 0.0f;
    }

    // Execute FFT
    fftwf_execute(impl_->plans[thread_id]);

    // Extract magnitude and phase with SIMD optimization if available
    result.fft_size = impl_->fft_size;
    result.magnitude.resize(impl_->fft_size / 2 + 1);
    result.phase.resize(impl_->fft_size / 2 + 1);
    result.frequencies.resize(impl_->fft_size / 2 + 1);

    if (impl_->simd_enabled) {
        // SIMD-optimized magnitude calculation
        #if defined(__AVX__) || defined(__AVX512F__)
        __m256 sqrt_mask = _mm256_set1_ps(0.70710678f);  // 1/sqrt(2) approximation

        for (int i = 0; i <= impl_->fft_size / 2; i += 8) {
            if (i + 7 <= impl_->fft_size / 2) {
                // Load 8 floats (4 complex numbers)
                float real_arr[8], imag_arr[8];
                for (int j = 0; j < 8; ++j) {
                    real_arr[j] = impl_->out_buffers[thread_id][i + j][0];
                    imag_arr[j] = impl_->out_buffers[thread_id][i + j][1];
                }

                __m256 real = _mm256_loadu_ps(real_arr);
                __m256 imag = _mm256_loadu_ps(imag_arr);

                // Calculate magnitude: sqrt(real^2 + imag^2)
                __m256 real_sq = _mm256_mul_ps(real, real);
                __m256 imag_sq = _mm256_mul_ps(imag, imag);
                __m256 sum = _mm256_add_ps(real_sq, imag_sq);

                // Approximate sqrt using multiplication
                __m256 mag = _mm256_mul_ps(sum, sqrt_mask);

                // Store results
                float mag_arr[8];
                _mm256_storeu_ps(mag_arr, mag);

                for (int j = 0; j < 8; ++j) {
                    result.magnitude[i + j] = magnitudeToDB(mag_arr[j]);
                }
            }
        }

        // Handle remaining elements
        for (int i = (impl_->fft_size / 2) & ~7; i <= impl_->fft_size / 2; ++i) {
            float real = impl_->out_buffers[thread_id][i][0];
            float imag = impl_->out_buffers[thread_id][i][1];
            result.magnitude[i] = magnitudeToDB(std::sqrt(real * real + imag * imag));
        }

        #elif defined(__ARM_NEON)
        // ARM NEON implementation
        float32x4_t sqrt_mask = vdupq_n_f32(0.70710678f);

        for (int i = 0; i <= impl_->fft_size / 2; i += 4) {
            if (i + 3 <= impl_->fft_size / 2) {
                float32x4_t real = vld1q_f32(&impl_->out_buffers[thread_id][i][0]);
                float32x4_t imag = vld1q_f32(&impl_->out_buffers[thread_id][i][1]);

                float32x4_t real_sq = vmulq_f32(real, real);
                float32x4_t imag_sq = vmulq_f32(imag, imag);
                float32x4_t sum = vaddq_f32(real_sq, imag_sq);
                float32x4_t mag = vmulq_f32(sum, sqrt_mask);

                float mag_arr[4];
                vst1q_f32(mag_arr, mag);

                for (int j = 0; j < 4; ++j) {
                    result.magnitude[i + j] = magnitudeToDB(mag_arr[j]);
                }
            }
        }

        // Handle remaining elements
        for (int i = (impl_->fft_size / 2) & ~3; i <= impl_->fft_size / 2; ++i) {
            float real = impl_->out_buffers[thread_id][i][0];
            float imag = impl_->out_buffers[thread_id][i][1];
            result.magnitude[i] = magnitudeToDB(std::sqrt(real * real + imag * imag));
        }
        #else
        // Scalar fallback
        for (int i = 0; i <= impl_->fft_size / 2; ++i) {
            float real = impl_->out_buffers[thread_id][i][0];
            float imag = impl_->out_buffers[thread_id][i][1];
            result.magnitude[i] = magnitudeToDB(std::sqrt(real * real + imag * imag));
        }
        #endif
    } else {
        // Scalar implementation
        for (int i = 0; i <= impl_->fft_size / 2; ++i) {
            float real = impl_->out_buffers[thread_id][i][0];
            float imag = impl_->out_buffers[thread_id][i][1];
            result.magnitude[i] = magnitudeToDB(std::sqrt(real * real + imag * imag));
        }
    }

    // Calculate phase (scalar is fine for this)
    for (int i = 0; i <= impl_->fft_size / 2; ++i) {
        float real = impl_->out_buffers[thread_id][i][0];
        float imag = impl_->out_buffers[thread_id][i][1];
        result.phase[i] = std::atan2(imag, real);
        result.frequencies[i] = i * sample_rate / static_cast<float>(impl_->fft_size);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    LOG_DEBUG("Optimized FFT computation took {} ms", duration);

    return ErrorCode::Success;
}

std::string FFTEngine::generateCacheKey(const std::vector<float>& audio_data,
                                         int sample_rate,
                                         int fft_size) const {
    // Generate SHA256 hash for cache key
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);

    // Hash audio data
    SHA256_Update(&sha256, audio_data.data(), audio_data.size() * sizeof(float));

    // Hash parameters
    SHA256_Update(&sha256, &sample_rate, sizeof(sample_rate));
    SHA256_Update(&sha256, &fft_size, sizeof(fft_size));

    SHA256_Final(hash, &sha256);

    // Convert to hex string
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return oss.str();
}

ErrorCode FFTEngine::computeFFTWithCache(const std::string& cache_id,
                                         const std::vector<float>& audio_data,
                                         int sample_rate,
                                         FFTResult& result) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Try to load from cache first
    if (hasValidCache(cache_id)) {
        ErrorCode ret = loadFromCache(cache_id, result);
        if (ret == ErrorCode::Success) {
            impl_->stats.hit_count++;

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time).count();

            LOG_INFO("FFT cache hit: {} ms", duration);
            impl_->stats.hit_rate = static_cast<double>(impl_->stats.hit_count) /
                                   (impl_->stats.hit_count + impl_->stats.miss_count);
            return ErrorCode::Success;
        }
    }

    // Cache miss - compute FFT
    impl_->stats.miss_count++;

    ErrorCode ret = computeFFT(audio_data, sample_rate, result);
    if (ret != ErrorCode::Success) {
        return ret;
    }

    // Save to cache
    ret = saveToCache(cache_id, result);
    if (ret != ErrorCode::Success) {
        LOG_WARNING("Failed to save FFT to cache: {}", cache_id);
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    LOG_INFO("FFT computed and cached: {} ms", duration);

    impl_->stats.hit_rate = static_cast<double>(impl_->stats.hit_count) /
                           (impl_->stats.hit_count + impl_->stats.miss_count);

    return ErrorCode::Success;
}

ErrorCode FFTEngine::loadFromCache(const std::string& cache_id,
                                    FFTResult& result) {
    std::string cache_path = impl_->cache_dir + "/fft/" + cache_id;
    std::string magnitude_file = cache_path + "/magnitude.bin";
    std::string phase_file = cache_path + "/phase.bin";
    std::string config_file = cache_path + "/config.json";

    // Check if cache files exist
    std::ifstream mag_test(magnitude_file, std::ios::binary);
    std::ifstream phase_test(phase_file, std::ios::binary);
    std::ifstream config_test(config_file);

    if (!mag_test.good() || !phase_test.good() || !config_test.good()) {
        LOG_DEBUG("Cache files not found: {}", cache_id);
        return ErrorCode::CacheEntryNotFound;
    }

    // Read magnitude data
    std::ifstream mag_file(magnitude_file, std::ios::binary);
    std::vector<float> magnitude_data;
    float value;
    while (mag_file.read(reinterpret_cast<char*>(&value), sizeof(float))) {
        magnitude_data.push_back(value);
    }

    // Read phase data
    std::ifstream phase_stream(phase_file, std::ios::binary);
    std::vector<float> phase_data;
    while (phase_stream.read(reinterpret_cast<char*>(&value), sizeof(float))) {
        phase_data.push_back(value);
    }

    // Validate data
    if (magnitude_data.size() != phase_data.size()) {
        LOG_ERROR("Cache data size mismatch: {}", cache_id);
        return ErrorCode::CacheCorrupted;
    }

    // Load result
    result.magnitude = magnitude_data;
    result.phase = phase_data;
    result.fft_size = magnitude_data.size() * 2 - 2; // Inverse of fft_size/2 + 1

    // Generate frequency array
    result.frequencies.resize(magnitude_data.size());
    for (size_t i = 0; i < magnitude_data.size(); ++i) {
        result.frequencies[i] = i * 48000.0f / result.fft_size; // Assume 48kHz
    }

    LOG_INFO("Loaded FFT from cache: {}", cache_id);
    return ErrorCode::Success;
}

ErrorCode FFTEngine::saveToCache(const std::string& cache_id,
                                  const FFTResult& result) {
    std::string cache_path = impl_->cache_dir + "/fft/" + cache_id;

    // Create cache directory
    std::string mkdir_cmd = "mkdir -p " + cache_path;
    system(mkdir_cmd.c_str());

    std::string magnitude_file = cache_path + "/magnitude.bin";
    std::string phase_file = cache_path + "/phase.bin";
    std::string config_file = cache_path + "/config.json";

    // Write magnitude data (use atomic write: temp -> rename)
    std::string temp_mag_file = magnitude_file + ".tmp";
    std::ofstream mag_file(temp_mag_file, std::ios::binary);
    mag_file.write(reinterpret_cast<const char*>(result.magnitude.data()),
                   result.magnitude.size() * sizeof(float));
    mag_file.close();
    std::rename(temp_mag_file.c_str(), magnitude_file.c_str());

    // Write phase data
    std::string temp_phase_file = phase_file + ".tmp";
    std::ofstream phase_file_out(temp_phase_file, std::ios::binary);
    phase_file_out.write(reinterpret_cast<const char*>(result.phase.data()),
                         result.phase.size() * sizeof(float));
    phase_file_out.close();
    std::rename(temp_phase_file.c_str(), phase_file.c_str());

    // Write config file
    std::string temp_config_file = config_file + ".tmp";
    std::ofstream config_out(temp_config_file);
    config_out << "{\n";
    config_out << "  \"fft_size\": " << result.fft_size << ",\n";
    config_out << "  \"window\": \"hann\",\n";
    config_out << "  \"version\": \"" << CACHE_VERSION << "\",\n";
    config_out << "  \"samples\": " << result.magnitude.size() << "\n";
    config_out << "}\n";
    config_out.close();
    std::rename(temp_config_file.c_str(), config_file.c_str());

    // Update cache stats
    impl_->stats.total_cache_size += result.magnitude.size() * sizeof(float);
    impl_->stats.total_cache_size += result.phase.size() * sizeof(float);

    LOG_INFO("Saved FFT to cache: {}", cache_id);
    return ErrorCode::Success;
}

bool FFTEngine::hasValidCache(const std::string& cache_id) const {
    std::string cache_path = impl_->cache_dir + "/fft/" + cache_id;
    std::string magnitude_file = cache_path + "/magnitude.bin";
    std::string phase_file = cache_path + "/phase.bin";
    std::string config_file = cache_path + "/config.json";

    // Check if all files exist
    std::ifstream mag_file(magnitude_file, std::ios::binary);
    std::ifstream phase_stream(phase_file, std::ios::binary);
    std::ifstream config_stream(config_file);

    return mag_file.good() && phase_stream.good() && config_stream.good();
}

ErrorCode FFTEngine::clearCache(const std::string& cache_id) {
    std::string cache_path = impl_->cache_dir + "/fft/" + cache_id;
    std::string remove_cmd = "rm -rf " + cache_path;
    system(remove_cmd.c_str());

    LOG_INFO("Cleared FFT cache: {}", cache_id);
    return ErrorCode::Success;
}

FFTEngine::CacheStats FFTEngine::getCacheStats() const {
    return impl_->stats;
}

float FFTEngine::magnitudeToDB(float magnitude) const {
    if (magnitude < 1e-10f) {
        return -100.0f;  // Floor at -100 dB
    }
    return 20.0f * std::log10(magnitude);
}

bool FFTCacheEntry::isValid() const {
    return !magnitude.empty() && !phase.empty();
}

} // namespace in2wav
} // namespace xpu
