/**
 * @file SACDDecoder.cpp
 * @brief DSD decoder using foo_input_sacd.dll implementation
 */

#ifdef PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <initguid.h>  // Must be included before GUID definitions
#endif

#include "SACDDecoder.h"
#include "foobar2000_wrapper.h"
#include "utils/Logger.h"
#include "utils/PlatformUtils.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <vector>
#include <memory>

using namespace xpu;

namespace xpu {
namespace load {

/**
 * @brief Minimal foobar2000 service manager implementation
 *
 * This class provides a minimal implementation of foobar2000's service
 * management system to allow foo_input_sacd.dll to work standalone.
 */
class foobar2000ServiceManager {
public:
    static foobar2000ServiceManager& getInstance() {
        static foobar2000ServiceManager instance;
        return instance;
    }

    // Register a service factory
    void registerFactory(service_factory* factory) {
        factories_.push_back(factory);
    }

    // Get service by GUID
    service_base* getService(const GUID& guid) {
        for (auto* factory : factories_) {
            if (factory && factory->get_guid() == guid) {
                return factory->instantiate();
            }
        }
        return nullptr;
    }

    // foobar2000_get_interface implementation
    static void* getInterface(GUID* guid) {
        if (!guid) return nullptr;
        return getInstance().getService(*guid);
    }

private:
    std::vector<service_factory*> factories_;
};

/**
 * @brief Input decoder wrapper for foo_input_sacd
 *
 * This class wraps the actual input_decoder from foo_input_sacd.dll
 * and provides a C++ interface we can use.
 */
class SACDInputDecoderWrapper : public input_decoder {
public:
    SACDInputDecoderWrapper(input_decoder* impl) : impl_(impl) {}

    bool initialize(const char* filepath, int flags) override {
        if (!impl_) return false;
        return impl_->initialize(filepath, flags);
    }

    bool run(const char* filepath) override {
        if (!impl_) return false;
        return impl_->run(filepath);
    }

    bool get_info(unsigned int what, void* data) override {
        if (!impl_) return false;
        return impl_->get_info(what, data);
    }

    void service_add_ref() override {
        if (impl_) impl_->service_add_ref();
    }

    void service_release() override {
        if (impl_) impl_->service_release();
    }

private:
    input_decoder* impl_ = nullptr;
};

/**
 * @brief Implementation class
 */
class SACDDecoder::Impl {
public:
    protocol::AudioMetadata metadata;
    bool loaded = false;
    int target_sample_rate = 0;
    std::string file_path;

#ifdef PLATFORM_WINDOWS
    HMODULE dll_handle = nullptr;
    foobar2000_get_interface_func get_interface = nullptr;
    input_decoder* decoder = nullptr;
#endif
};

SACDDecoder::SACDDecoder()
    : impl_(std::make_unique<Impl>()) {}

SACDDecoder::~SACDDecoder() {
#ifdef PLATFORM_WINDOWS
    if (impl_->decoder) {
        impl_->decoder->service_release();
    }
    if (impl_->dll_handle) {
        FreeLibrary(impl_->dll_handle);
    }
#endif
}

void SACDDecoder::setTargetSampleRate(int sample_rate) {
    impl_->target_sample_rate = sample_rate;
    LOG_INFO("Target sample rate set to: {}", sample_rate);
}

ErrorCode SACDDecoder::prepareStreaming(const std::string& filepath) {
    LOG_INFO("Preparing DSD streaming with foo_input_sacd.dll: {}", filepath);

    // Convert path to absolute path
    std::string absolute_path = filepath;
    if (filepath.size() > 0 && filepath[0] != '/' && filepath[0] != '\\' &&
        !(filepath.size() > 1 && filepath[1] == ':')) {
        char buffer[4096];
        #ifdef PLATFORM_WINDOWS
        _fullpath(buffer, filepath.c_str(), sizeof(buffer));
        #else
        realpath(filepath.c_str(), buffer);
        #endif
        absolute_path = buffer;
    }

    // Check if file exists
    std::ifstream test_file(absolute_path, std::ios::binary);
    if (!test_file) {
        LOG_ERROR("Failed to open DSD file: {}", absolute_path);
        return ErrorCode::FileReadError;
    }
    test_file.close();

    impl_->file_path = absolute_path;

#ifdef PLATFORM_WINDOWS
    // Load foo_input_sacd.dll
    std::string dll_path = "foo_input_sacd.dll";
    impl_->dll_handle = LoadLibraryA(dll_path.c_str());
    if (!impl_->dll_handle) {
        LOG_ERROR("Failed to load foo_input_sacd.dll from: {}", dll_path);
        LOG_ERROR("Error code: {}", GetLastError());
        return ErrorCode::FileReadError;
    }

    LOG_INFO("Loaded foo_input_sacd.dll from: {}", dll_path);

    // Get the foobar2000_get_interface function
    impl_->get_interface = reinterpret_cast<foobar2000_get_interface_func>(
        GetProcAddress(impl_->dll_handle, "foobar2000_get_interface")
    );

    if (!impl_->get_interface) {
        LOG_ERROR("Failed to get foobar2000_get_interface function");
        return ErrorCode::FileReadError;
    }

    LOG_INFO("Found foobar2000_get_interface function");

    // Initialize service manager with our get_interface function
    // The DLL will call our get_interface when it needs services

    // Try to get the input_decoder service
    // Note: The actual GUID for input_decoder needs to be discovered
    // For now, we'll use a placeholder implementation
    LOG_WARN("SACD decoder: foobar2000 interface detected but full integration incomplete");
    LOG_WARN("This requires reverse-engineering the exact GUID and interface used by foo_input_sacd.dll");

    // Set placeholder metadata
    impl_->metadata.file_path = absolute_path;
    impl_->metadata.format = "DSD (SACD decoder)";
    impl_->metadata.format_name = "DSD (SACD decoder - integration in progress)";
    impl_->metadata.channels = 2;
    impl_->metadata.bit_depth = 32;
    impl_->metadata.original_bit_depth = 1;

    // Calculate sample rate
    if (impl_->target_sample_rate > 0) {
        impl_->metadata.sample_rate = impl_->target_sample_rate;
    } else {
        impl_->metadata.sample_rate = 88200;  // DSD64/32
    }
    impl_->metadata.original_sample_rate = 2822400;  // DSD64
    impl_->metadata.is_lossless = true;
    impl_->metadata.is_high_res = true;

    LOG_INFO("SACD streaming prepared - DLL loaded but full integration requires foobar2000 SDK");
    return ErrorCode::Success;
#else
    LOG_ERROR("SACD decoder only supported on Windows");
    return ErrorCode::NotImplemented;
#endif
}

ErrorCode SACDDecoder::streamPCM(StreamingCallback callback, size_t chunk_size_bytes) {
    LOG_INFO("Streaming DSD to PCM in chunks: {} bytes", chunk_size_bytes);

#ifdef PLATFORM_WINDOWS
    if (!impl_->dll_handle) {
        LOG_ERROR("streamPCM() called without prepareStreaming()");
        return ErrorCode::InvalidOperation;
    }

    // TODO: Implement actual streaming using the foobar2000 input_decoder
    // This requires:
    // 1. Calling decoder->initialize(filepath, flags)
    // 2. Setting up a callback to receive PCM data
    // 3. Calling decoder->run(filepath)
    // 4. Processing the output in chunks

    // For now, return silent audio as placeholder
    LOG_WARN("SACD streaming: foobar2000 integration incomplete - outputting silence");

    const size_t samples_per_chunk = chunk_size_bytes / sizeof(float);
    std::vector<float> chunk_buffer(samples_per_chunk, 0.0f);

    callback(chunk_buffer.data(), chunk_buffer.size());

    LOG_INFO("SACD streaming complete (placeholder)");
    return ErrorCode::Success;
#else
    LOG_ERROR("SACD decoder only supported on Windows");
    return ErrorCode::NotImplemented;
#endif
}

const protocol::AudioMetadata& SACDDecoder::getMetadata() const {
    return impl_->metadata;
}

} // namespace load
} // namespace xpu
