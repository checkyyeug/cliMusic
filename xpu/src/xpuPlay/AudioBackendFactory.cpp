/**
 * @file AudioBackendFactory.cpp
 * @brief Factory for creating platform-specific audio backends
 */

#include "AudioBackend.h"
#include "utils/Logger.h"

// Platform-specific headers
#ifdef PLATFORM_WINDOWS
#include "AudioBackend_WASAPI.h"
#endif

#ifdef PLATFORM_MACOS
#include "AudioBackend_CoreAudio.h"
#endif

#ifdef PLATFORM_LINUX
#include "AudioBackend_ALSA.h"
#endif

using namespace xpu;

namespace xpu {
namespace playback {

std::unique_ptr<AudioBackend> AudioBackend::create() {
#ifdef PLATFORM_WINDOWS
    LOG_INFO("Creating WASAPI audio backend");
    return std::make_unique<AudioBackendWASAPI>();
#elif defined(PLATFORM_MACOS)
    LOG_INFO("Creating CoreAudio audio backend");
    return std::make_unique<AudioBackendCoreAudio>();
#elif defined(PLATFORM_LINUX)
    LOG_INFO("Creating ALSA audio backend");
    return std::make_unique<AudioBackendALSA>();
#else
    LOG_ERROR("No audio backend available for this platform");
    return nullptr;
#endif
}

} // namespace playback
} // namespace xpu
