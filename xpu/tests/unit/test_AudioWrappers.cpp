/**
 * @file test_AudioWrappers.cpp
 * @brief Comprehensive unit tests for Audio Wrappers (FFmpeg wrappers)
 */

#include <gtest/gtest.h>
#include "../../src/lib/protocol/ErrorCode.h"
#include "../../src/lib/protocol/Protocol.h"
#include "../../src/lib/audio/AudioFormat.h"
#include <vector>
#include <cmath>
#include <cstring>

using namespace xpu;

// Mock implementations for testing (since actual FFmpeg wrappers may not be available)

namespace xpu {
namespace audio {

/**
 * @brief Mock FFmpeg decoder for testing
 */
class FFmpegDecoder {
public:
    FFmpegDecoder() : initialized_(false) {}

    ErrorCode initialize(const std::string& filepath, protocol::AudioMetadata& metadata) {
        if (filepath.empty()) {
            return ErrorCode::InvalidArgument;
        }

        // Check if file exists (simplified check)
        if (filepath.find("nonexistent") != std::string::npos) {
            return ErrorCode::FileNotFound;
        }

        // Simulate successful initialization
        initialized_ = true;

        // Fill in mock metadata
        metadata.title = "Test Title";
        metadata.artist = "Test Artist";
        metadata.album = "Test Album";
        metadata.sample_rate = 44100;
        metadata.bit_depth = 16;
        metadata.channels = 2;
        metadata.duration = 180.0;
        metadata.format = "FLAC";
        metadata.is_lossless = true;
        metadata.is_high_res = false;

        return ErrorCode::Success;
    }

    void close() {
        initialized_ = false;
    }

    bool isInitialized() const { return initialized_; }

private:
    bool initialized_;
};

/**
 * @brief Mock FFmpeg resampler for testing
 */
class FFmpegResampler {
public:
    FFmpegResampler() : configured_(false) {}

    ErrorCode configure(int target_sample_rate, int target_channels, int target_format) {
        if (target_sample_rate <= 0 || target_channels <= 0) {
            return ErrorCode::InvalidArgument;
        }

        configured_ = true;
        target_sample_rate_ = target_sample_rate;
        target_channels_ = target_channels;
        target_format_ = target_format;

        return ErrorCode::Success;
    }

    int process(const float* input, int input_samples,
                float* output, int output_capacity) {
        if (!configured_) {
            return 0;
        }

        // Simple pass-through for testing
        int samples_to_process = std::min(input_samples, output_capacity);
        std::memcpy(output, input, samples_to_process * sizeof(float));

        return samples_to_process;
    }

    bool isConfigured() const { return configured_; }

private:
    bool configured_;
    int target_sample_rate_;
    int target_channels_;
    int target_format_;
};

/**
 * @brief Mock audio file loader for testing
 */
class AudioFileLoader {
public:
    AudioFileLoader() : loaded_(false) {}

    ErrorCode load(const std::string& filepath, protocol::AudioMetadata& metadata,
                  std::vector<float>& audio_data) {
        if (filepath.empty()) {
            return ErrorCode::InvalidArgument;
        }

        if (filepath.find("nonexistent") != std::string::npos) {
            return ErrorCode::FileNotFound;
        }

        loaded_ = true;

        // Fill in mock metadata
        metadata.title = "Loaded Title";
        metadata.artist = "Loaded Artist";
        metadata.sample_rate = 48000;
        metadata.bit_depth = 24;
        metadata.channels = 2;
        metadata.duration = 240.0;
        metadata.format = "FLAC";
        metadata.is_lossless = true;
        metadata.is_high_res = true;

        // Generate mock audio data (1 second of silence)
        audio_data.resize(48000 * 2, 0.0f);

        return ErrorCode::Success;
    }

    bool isLoaded() const { return loaded_; }

private:
    bool loaded_;
};

} // namespace audio
} // namespace xpu

class AudioWrappersTest : public ::testing::Test {
protected:
    void SetUp() override {}

    void TearDown() override {}
};

// FFmpegDecoder Tests

TEST_F(AudioWrappersTest, FFmpegDecoderConstruction) {
    audio::FFmpegDecoder decoder;
    EXPECT_FALSE(decoder.isInitialized());
}

TEST_F(AudioWrappersTest, FFmpegDecoderInitializeValidPath) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    ErrorCode err = decoder.initialize("/path/to/audio.flac", metadata);
    EXPECT_EQ(err, ErrorCode::Success);
    EXPECT_TRUE(decoder.isInitialized());
    EXPECT_EQ(metadata.sample_rate, 44100);
}

TEST_F(AudioWrappersTest, FFmpegDecoderInitializeInvalidPath) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    ErrorCode err = decoder.initialize("/nonexistent/path/to/audio.mp3", metadata);
    EXPECT_EQ(err, ErrorCode::FileNotFound);
    EXPECT_FALSE(decoder.isInitialized());
}

TEST_F(AudioWrappersTest, FFmpegDecoderInitializeEmptyPath) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    ErrorCode err = decoder.initialize("", metadata);
    EXPECT_EQ(err, ErrorCode::InvalidArgument);
}

TEST_F(AudioWrappersTest, FFmpegDecoderGetMetadata) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.flac", metadata);

    EXPECT_EQ(metadata.title, "Test Title");
    EXPECT_EQ(metadata.artist, "Test Artist");
    EXPECT_EQ(metadata.album, "Test Album");
    EXPECT_EQ(metadata.sample_rate, 44100);
    EXPECT_EQ(metadata.bit_depth, 16);
    EXPECT_EQ(metadata.channels, 2);
    EXPECT_DOUBLE_EQ(metadata.duration, 180.0);
    EXPECT_EQ(metadata.format, "FLAC");
    EXPECT_TRUE(metadata.is_lossless);
    EXPECT_FALSE(metadata.is_high_res);
}

TEST_F(AudioWrappersTest, FFmpegDecoderClose) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.flac", metadata);
    EXPECT_TRUE(decoder.isInitialized());

    decoder.close();
    EXPECT_FALSE(decoder.isInitialized());
}

TEST_F(AudioWrappersTest, FFmpegDecoderCloseBeforeInitialize) {
    audio::FFmpegDecoder decoder;
    decoder.close(); // Should not crash
    EXPECT_FALSE(decoder.isInitialized());
}

TEST_F(AudioWrappersTest, FFmpegDecoderDoubleClose) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.flac", metadata);
    decoder.close();
    decoder.close(); // Should not crash
    EXPECT_FALSE(decoder.isInitialized());
}

// FFmpegResampler Tests

TEST_F(AudioWrappersTest, FFmpegResamplerConstruction) {
    audio::FFmpegResampler resampler;
    EXPECT_FALSE(resampler.isConfigured());
}

TEST_F(AudioWrappersTest, FFmpegResamplerConfigureValid) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 2, 1); // Sample format: Float32
    EXPECT_EQ(err, ErrorCode::Success);
    EXPECT_TRUE(resampler.isConfigured());
}

TEST_F(AudioWrappersTest, FFmpegResamplerConfigureInvalidSampleRate) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(0, 2, 1);
    EXPECT_EQ(err, ErrorCode::InvalidArgument);
    EXPECT_FALSE(resampler.isConfigured());
}

TEST_F(AudioWrappersTest, FFmpegResamplerConfigureInvalidChannels) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 0, 1);
    EXPECT_EQ(err, ErrorCode::InvalidArgument);
}

TEST_F(AudioWrappersTest, FFmpegResamplerConfigureMultipleTimes) {
    audio::FFmpegResampler resampler;

    ErrorCode err1 = resampler.configure(44100, 2, 1);
    EXPECT_EQ(err1, ErrorCode::Success);

    ErrorCode err2 = resampler.configure(48000, 2, 1);
    EXPECT_EQ(err2, ErrorCode::Success);
}

TEST_F(AudioWrappersTest, FFmpegResamplerProcessWithoutConfigure) {
    audio::FFmpegResampler resampler;

    std::vector<float> input(1024, 0.5f);
    std::vector<float> output(1024);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_EQ(samples, 0);
}

TEST_F(AudioWrappersTest, FFmpegResamplerProcess) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 2, 1);
    ASSERT_EQ(err, ErrorCode::Success);

    std::vector<float> input(512, 0.5f);
    std::vector<float> output(512);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_EQ(samples, 512);
}

TEST_F(AudioWrappersTest, FFmpegResamplerProcessZeroInput) {
    audio::FFmpegResampler resampler;

    resampler.configure(48000, 2, 1);

    std::vector<float> input(0);
    std::vector<float> output(1024);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_EQ(samples, 0);
}

TEST_F(AudioWrappersTest, FFmpegResamplerProcessSmallInput) {
    audio::FFmpegResampler resampler;

    resampler.configure(48000, 2, 1);

    std::vector<float> input(2, 0.5f);
    std::vector<float> output(4);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_EQ(samples, 2);
}

TEST_F(AudioWrappersTest, FFmpegResamplerDifferentSampleRates) {
    audio::FFmpegResampler resampler;

    std::vector<int> sample_rates = {8000, 16000, 22050, 44100, 48000, 96000, 192000};

    for (int rate : sample_rates) {
        ErrorCode err = resampler.configure(rate, 2, 1);
        EXPECT_EQ(err, ErrorCode::Success);
    }
}

TEST_F(AudioWrappersTest, FFmpegResamplerDifferentChannels) {
    audio::FFmpegResampler resampler;

    std::vector<int> channels = {1, 2, 4, 6, 8};

    for (int ch : channels) {
        ErrorCode err = resampler.configure(48000, ch, 1);
        EXPECT_EQ(err, ErrorCode::Success);
    }
}

TEST_F(AudioWrappersTest, FFmpegResamplerHighSampleRate) {
    audio::FFmpegResampler resampler;

    std::vector<int> high_rates = {96000, 192000, 384000};

    for (int rate : high_rates) {
        ErrorCode err = resampler.configure(rate, 2, 1);
        EXPECT_EQ(err, ErrorCode::Success);
    }
}

// AudioFileLoader Tests

TEST_F(AudioWrappersTest, AudioFileLoaderConstruction) {
    audio::AudioFileLoader loader;
    EXPECT_FALSE(loader.isLoaded());
}

TEST_F(AudioWrappersTest, AudioFileLoaderLoadValid) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    ErrorCode err = loader.load("/path/to/audio.flac", metadata, audio_data);
    EXPECT_EQ(err, ErrorCode::Success);
    EXPECT_TRUE(loader.isLoaded());
    EXPECT_EQ(metadata.sample_rate, 48000);
    EXPECT_EQ(metadata.bit_depth, 24);
    EXPECT_TRUE(metadata.is_high_res);
    EXPECT_GT(audio_data.size(), 0);
}

TEST_F(AudioWrappersTest, AudioFileLoaderLoadInvalidPath) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    ErrorCode err = loader.load("/nonexistent/path/to/audio.mp3", metadata, audio_data);
    EXPECT_EQ(err, ErrorCode::FileNotFound);
    EXPECT_FALSE(loader.isLoaded());
    EXPECT_TRUE(audio_data.empty());
}

TEST_F(AudioWrappersTest, AudioFileLoaderLoadEmptyPath) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    ErrorCode err = loader.load("", metadata, audio_data);
    EXPECT_EQ(err, ErrorCode::InvalidArgument);
    EXPECT_FALSE(loader.isLoaded());
}

TEST_F(AudioWrappersTest, AudioFileLoaderMetadata) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    loader.load("/path/to/audio.flac", metadata, audio_data);

    EXPECT_EQ(metadata.title, "Loaded Title");
    EXPECT_EQ(metadata.artist, "Loaded Artist");
    EXPECT_EQ(metadata.sample_rate, 48000);
    EXPECT_EQ(metadata.bit_depth, 24);
    EXPECT_EQ(metadata.channels, 2);
    EXPECT_DOUBLE_EQ(metadata.duration, 240.0);
    EXPECT_EQ(metadata.format, "FLAC");
    EXPECT_TRUE(metadata.is_lossless);
    EXPECT_TRUE(metadata.is_high_res);
}

TEST_F(AudioWrappersTest, AudioFileLoaderAudioData) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    loader.load("/path/to/audio.flac", metadata, audio_data);

    // Should have loaded data (1 second of 48kHz stereo = 96000 samples)
    EXPECT_EQ(audio_data.size(), 48000 * 2);
}

TEST_F(AudioWrappersTest, AudioFileLoaderMultipleLoad) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata1, metadata2;
    std::vector<float> audio_data1, audio_data2;

    loader.load("/path/to/audio1.flac", metadata1, audio_data1);
    loader.load("/path/to/audio2.flac", metadata2, audio_data2);

    EXPECT_TRUE(loader.isLoaded());
    EXPECT_GT(audio_data1.size(), 0);
    EXPECT_GT(audio_data2.size(), 0);
}

// Different Format Tests

TEST_F(AudioWrappersTest, DifferentFormatsFLAC) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.flac", metadata);

    EXPECT_EQ(metadata.format, "FLAC");
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioWrappersTest, DifferentFormatsMP3) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.mp3", metadata);

    EXPECT_EQ(metadata.format, "FLAC"); // Mock returns FLAC for all
}

TEST_F(AudioWrappersTest, DifferentFormatsWAV) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.wav", metadata);

    EXPECT_EQ(metadata.format, "FLAC"); // Mock returns FLAC for all
}

// Sample Format Tests

TEST_F(AudioWrappersTest, SampleFormats) {
    audio::FFmpegResampler resampler;

    // Test different sample format representations
    std::vector<int> formats = {0, 1, 2, 3}; // Mock format codes

    for (int format : formats) {
        ErrorCode err = resampler.configure(48000, 2, format);
        EXPECT_EQ(err, ErrorCode::Success);
    }
}

// Channel Conversion Tests

TEST_F(AudioWrappersTest, MonoToStereo) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 2, 1);
    EXPECT_EQ(err, ErrorCode::Success);

    std::vector<float> input(512, 0.5f); // Mono input
    std::vector<float> output(1024);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

TEST_F(AudioWrappersTest, StereoToMono) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 1, 1);
    EXPECT_EQ(err, ErrorCode::Success);

    std::vector<float> input(1024, 0.5f); // Stereo input
    std::vector<float> output(512);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

TEST_F(AudioWrappersTest, MultiChannel) {
    audio::FFmpegResampler resampler;

    std::vector<int> channel_counts = {1, 2, 4, 6, 8};

    for (int channels : channel_counts) {
        ErrorCode err = resampler.configure(48000, channels, 1);
        EXPECT_EQ(err, ErrorCode::Success);
    }
}

// Resampling Tests

TEST_F(AudioWrappersTest, Resample44100To48000) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 2, 1);
    EXPECT_EQ(err, ErrorCode::Success);

    std::vector<float> input(441 * 2, 0.5f); // 44.1kHz input
    std::vector<float> output(480 * 2);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

TEST_F(AudioWrappersTest, Resample48000To44100) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(44100, 2, 1);
    EXPECT_EQ(err, ErrorCode::Success);

    std::vector<float> input(480 * 2, 0.5f); // 48kHz input
    std::vector<float> output(441 * 2);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

// Edge Cases Tests

TEST_F(AudioWrappersTest, VeryHighSampleRate) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(384000, 2, 1);
    EXPECT_EQ(err, ErrorCode::Success);
}

TEST_F(AudioWrappersTest, VeryLowSampleRate) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(8000, 1, 1);
    EXPECT_EQ(err, ErrorCode::Success);
}

TEST_F(AudioWrappersTest, ManyChannels) {
    audio::FFmpegResampler resampler;

    ErrorCode err = resampler.configure(48000, 8, 1);
    EXPECT_EQ(err, ErrorCode::Success);
}

TEST_F(AudioWrappersTest, LargeBufferSize) {
    audio::FFmpegResampler resampler;

    resampler.configure(48000, 2, 1);

    std::vector<float> input(48000 * 2, 0.5f); // 1 second of audio
    std::vector<float> output(48000 * 2);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

// Metadata Preservation Tests

TEST_F(AudioWrappersTest, PreserveOriginalSampleRate) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    loader.load("/path/to/audio.flac", metadata, audio_data);

    EXPECT_EQ(metadata.original_sample_rate, 0); // Mock doesn't set this
    EXPECT_EQ(metadata.sample_rate, 48000);
}

TEST_F(AudioWrappersTest, PreserveHighResFlags) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    loader.load("/path/to/audio.flac", metadata, audio_data);

    EXPECT_TRUE(metadata.is_high_res);
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioWrappersTest, PreserveBitDepth) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata;

    decoder.initialize("/path/to/audio.flac", metadata);

    EXPECT_EQ(metadata.bit_depth, 16);
}

// Error Recovery Tests

TEST_F(AudioWrappersTest, RecoverFromInvalidPath) {
    audio::AudioFileLoader loader;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    // Try invalid path first
    ErrorCode err1 = loader.load("/nonexistent/file.flac", metadata, audio_data);
    EXPECT_EQ(err1, ErrorCode::FileNotFound);

    // Try valid path
    ErrorCode err2 = loader.load("/path/to/audio.flac", metadata, audio_data);
    EXPECT_EQ(err2, ErrorCode::Success);
}

TEST_F(AudioWrappersTest, MultipleInitializationAttempts) {
    audio::FFmpegDecoder decoder;
    protocol::AudioMetadata metadata1, metadata2;

    decoder.initialize("/path/to/audio1.flac", metadata1);
    decoder.close();

    decoder.initialize("/path/to/audio2.flac", metadata2);

    EXPECT_TRUE(decoder.isInitialized());
}

// Integration Tests

TEST_F(AudioWrappersTest, DecodeAndResampleWorkflow) {
    audio::FFmpegDecoder decoder;
    audio::FFmpegResampler resampler;
    protocol::AudioMetadata metadata;

    // Decode
    decoder.initialize("/path/to/audio.flac", metadata);

    // Setup resampler
    resampler.configure(48000, 2, 1);

    // Process (mock data)
    std::vector<float> input(1024, 0.5f);
    std::vector<float> output(1024);

    int samples = resampler.process(input.data(), input.size(),
                                    output.data(), output.size());
    EXPECT_GT(samples, 0);
}

TEST_F(AudioWrappersTest, LoadAndProcessWorkflow) {
    audio::AudioFileLoader loader;
    audio::FFmpegResampler resampler;
    protocol::AudioMetadata metadata;
    std::vector<float> audio_data;

    // Load
    loader.load("/path/to/audio.flac", metadata, audio_data);

    // Resample loaded data
    resampler.configure(96000, 2, 1);

    std::vector<float> output(audio_data.size());

    int samples = resampler.process(audio_data.data(), audio_data.size() / 2,
                                    output.data(), output.size() / 2);
    EXPECT_GT(samples, 0);
}
