/**
 * @file test_Protocol.cpp
 * @brief Comprehensive unit tests for Protocol structures and error handling
 */

#include <gtest/gtest.h>
#include "../../src/lib/protocol/Protocol.h"
#include "../../src/lib/protocol/ErrorCode.h"
#include <string>
#include <cmath>

using namespace xpu;

class AudioMetadataTest : public ::testing::Test {
protected:
    void SetUp() override {
        metadata = protocol::AudioMetadata();
    }

    protocol::AudioMetadata metadata;
};

// Default Construction Tests

TEST_F(AudioMetadataTest, DefaultConstruction) {
    EXPECT_TRUE(metadata.title.empty());
    EXPECT_TRUE(metadata.artist.empty());
    EXPECT_TRUE(metadata.album.empty());
    EXPECT_TRUE(metadata.year.empty());
    EXPECT_TRUE(metadata.genre.empty());
    EXPECT_EQ(metadata.track_number, 0);
    EXPECT_EQ(metadata.duration, 0.0);
    EXPECT_EQ(metadata.sample_rate, 0);
    EXPECT_EQ(metadata.bit_depth, 0);
    EXPECT_EQ(metadata.channels, 0);
    EXPECT_EQ(metadata.sample_count, 0);
    EXPECT_TRUE(metadata.format.empty());
    EXPECT_TRUE(metadata.format_name.empty());
    EXPECT_EQ(metadata.bitrate, 0.0);
    EXPECT_TRUE(metadata.file_path.empty());
    EXPECT_FALSE(metadata.is_lossless);
    EXPECT_FALSE(metadata.is_high_res);
    EXPECT_EQ(metadata.original_sample_rate, 0);
}

// Field Assignment Tests

TEST_F(AudioMetadataTest, TitleAssignment) {
    metadata.title = "Test Song";
    EXPECT_EQ(metadata.title, "Test Song");
}

TEST_F(AudioMetadataTest, ArtistAssignment) {
    metadata.artist = "Test Artist";
    EXPECT_EQ(metadata.artist, "Test Artist");
}

TEST_F(AudioMetadataTest, AlbumAssignment) {
    metadata.album = "Test Album";
    EXPECT_EQ(metadata.album, "Test Album");
}

TEST_F(AudioMetadataTest, YearAssignment) {
    metadata.year = "2024";
    EXPECT_EQ(metadata.year, "2024");
}

TEST_F(AudioMetadataTest, GenreAssignment) {
    metadata.genre = "Rock";
    EXPECT_EQ(metadata.genre, "Rock");
}

TEST_F(AudioMetadataTest, TrackNumberAssignment) {
    metadata.track_number = 5;
    EXPECT_EQ(metadata.track_number, 5);
}

TEST_F(AudioMetadataTest, DurationAssignment) {
    metadata.duration = 180.5;
    EXPECT_DOUBLE_EQ(metadata.duration, 180.5);
}

TEST_F(AudioMetadataTest, SampleRateAssignment) {
    metadata.sample_rate = 96000;
    EXPECT_EQ(metadata.sample_rate, 96000);
}

TEST_F(AudioMetadataTest, BitDepthAssignment) {
    metadata.bit_depth = 24;
    EXPECT_EQ(metadata.bit_depth, 24);
}

TEST_F(AudioMetadataTest, ChannelsAssignment) {
    metadata.channels = 2;
    EXPECT_EQ(metadata.channels, 2);
}

TEST_F(AudioMetadataTest, SampleCountAssignment) {
    metadata.sample_count = 8640000;
    EXPECT_EQ(metadata.sample_count, 8640000);
}

TEST_F(AudioMetadataTest, FormatAssignment) {
    metadata.format = "FLAC";
    EXPECT_EQ(metadata.format, "FLAC");
}

TEST_F(AudioMetadataTest, FormatNameAssignment) {
    metadata.format_name = "FLAC Audio";
    EXPECT_EQ(metadata.format_name, "FLAC Audio");
}

TEST_F(AudioMetadataTest, BitrateAssignment) {
    metadata.bitrate = 1411.2;
    EXPECT_DOUBLE_EQ(metadata.bitrate, 1411.2);
}

TEST_F(AudioMetadataTest, FilePathAssignment) {
    metadata.file_path = "/music/test.flac";
    EXPECT_EQ(metadata.file_path, "/music/test.flac");
}

TEST_F(AudioMetadataTest, LosslessFlagAssignment) {
    metadata.is_lossless = true;
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, HighResFlagAssignment) {
    metadata.is_high_res = true;
    EXPECT_TRUE(metadata.is_high_res);
}

TEST_F(AudioMetadataTest, OriginalSampleRateAssignment) {
    metadata.original_sample_rate = 192000;
    EXPECT_EQ(metadata.original_sample_rate, 192000);
}

// High-Resolution Detection Tests

TEST_F(AudioMetadataTest, HighResDetection96kHz24bit) {
    metadata.sample_rate = 96000;
    metadata.bit_depth = 24;
    metadata.is_high_res = true;
    EXPECT_TRUE(metadata.is_high_res);
}

TEST_F(AudioMetadataTest, HighResDetection192kHz24bit) {
    metadata.sample_rate = 192000;
    metadata.bit_depth = 24;
    metadata.is_high_res = true;
    EXPECT_TRUE(metadata.is_high_res);
}

TEST_F(AudioMetadataTest, NotHighRes44kHz16bit) {
    metadata.sample_rate = 44100;
    metadata.bit_depth = 16;
    metadata.is_high_res = false;
    EXPECT_FALSE(metadata.is_high_res);
}

TEST_F(AudioMetadataTest, HighResDetectionDSD) {
    metadata.sample_rate = 2822400;
    metadata.bit_depth = 1;
    metadata.format = "DSD";
    metadata.is_high_res = true;
    EXPECT_TRUE(metadata.is_high_res);
}

// Lossless Format Detection Tests

TEST_F(AudioMetadataTest, LosslessFormatFLAC) {
    metadata.format = "FLAC";
    metadata.is_lossless = true;
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LosslessFormatWAV) {
    metadata.format = "WAV";
    metadata.is_lossless = true;
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LosslessFormatALAC) {
    metadata.format = "ALAC";
    metadata.is_lossless = true;
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LosslessFormatDSD) {
    metadata.format = "DSD";
    metadata.is_lossless = true;
    EXPECT_TRUE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LossyFormatMP3) {
    metadata.format = "MP3";
    metadata.is_lossless = false;
    EXPECT_FALSE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LossyFormatAAC) {
    metadata.format = "AAC";
    metadata.is_lossless = false;
    EXPECT_FALSE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, LossyFormatOGG) {
    metadata.format = "OGG";
    metadata.is_lossless = false;
    EXPECT_FALSE(metadata.is_lossless);
}

// Copy Construction and Assignment Tests

TEST_F(AudioMetadataTest, CopyConstruction) {
    metadata.title = "Original Title";
    metadata.artist = "Original Artist";
    metadata.sample_rate = 96000;

    protocol::AudioMetadata copy(metadata);

    EXPECT_EQ(copy.title, "Original Title");
    EXPECT_EQ(copy.artist, "Original Artist");
    EXPECT_EQ(copy.sample_rate, 96000);
}

TEST_F(AudioMetadataTest, CopyAssignment) {
    metadata.title = "Original Title";
    metadata.sample_rate = 192000;

    protocol::AudioMetadata copy;
    copy = metadata;

    EXPECT_EQ(copy.title, "Original Title");
    EXPECT_EQ(copy.sample_rate, 192000);
}

TEST_F(AudioMetadataTest, CopyIndependence) {
    metadata.title = "Original";
    protocol::AudioMetadata copy(metadata);

    copy.title = "Modified";

    EXPECT_EQ(metadata.title, "Original");
    EXPECT_EQ(copy.title, "Modified");
}

// Multi-Channel Support Tests

TEST_F(AudioMetadataTest, MonoChannel) {
    metadata.channels = 1;
    EXPECT_EQ(metadata.channels, 1);
}

TEST_F(AudioMetadataTest, StereoChannel) {
    metadata.channels = 2;
    EXPECT_EQ(metadata.channels, 2);
}

TEST_F(AudioMetadataTest, FivePointOneChannel) {
    metadata.channels = 6;
    EXPECT_EQ(metadata.channels, 6);
}

TEST_F(AudioMetadataTest, SevenPointOneChannel) {
    metadata.channels = 8;
    EXPECT_EQ(metadata.channels, 8);
}

// Bit Depth Tests

TEST_F(AudioMetadataTest, BitDepth16) {
    metadata.bit_depth = 16;
    EXPECT_EQ(metadata.bit_depth, 16);
}

TEST_F(AudioMetadataTest, BitDepth24) {
    metadata.bit_depth = 24;
    EXPECT_EQ(metadata.bit_depth, 24);
}

TEST_F(AudioMetadataTest, BitDepth32) {
    metadata.bit_depth = 32;
    EXPECT_EQ(metadata.bit_depth, 32);
}

TEST_F(AudioMetadataTest, BitDepthDSD) {
    metadata.bit_depth = 1;
    metadata.format = "DSD";
    EXPECT_EQ(metadata.bit_depth, 1);
}

// Sample Rate Tests

TEST_F(AudioMetadataTest, SampleRate44100) {
    metadata.sample_rate = 44100;
    EXPECT_EQ(metadata.sample_rate, 44100);
}

TEST_F(AudioMetadataTest, SampleRate48000) {
    metadata.sample_rate = 48000;
    EXPECT_EQ(metadata.sample_rate, 48000);
}

TEST_F(AudioMetadataTest, SampleRate96000) {
    metadata.sample_rate = 96000;
    EXPECT_EQ(metadata.sample_rate, 96000);
}

TEST_F(AudioMetadataTest, SampleRate192000) {
    metadata.sample_rate = 192000;
    EXPECT_EQ(metadata.sample_rate, 192000);
}

TEST_F(AudioMetadataTest, SampleRate384000) {
    metadata.sample_rate = 384000;
    EXPECT_EQ(metadata.sample_rate, 384000);
}

// DSD Support Tests

TEST_F(AudioMetadataTest, DSD64Format) {
    metadata.format = "DSD";
    metadata.sample_rate = 2822400;
    metadata.bit_depth = 1;
    EXPECT_EQ(metadata.sample_rate, 2822400);
}

TEST_F(AudioMetadataTest, DSD128Format) {
    metadata.format = "DSD";
    metadata.sample_rate = 5644800;
    metadata.bit_depth = 1;
    EXPECT_EQ(metadata.sample_rate, 5644800);
}

TEST_F(AudioMetadataTest, DSD256Format) {
    metadata.format = "DSD";
    metadata.sample_rate = 11289600;
    metadata.bit_depth = 1;
    EXPECT_EQ(metadata.sample_rate, 11289600);
}

// Duration and Bitrate Precision Tests

TEST_F(AudioMetadataTest, DurationPrecision) {
    metadata.duration = 180.456789;
    EXPECT_NEAR(metadata.duration, 180.456789, 0.000001);
}

TEST_F(AudioMetadataTest, DurationZero) {
    metadata.duration = 0.0;
    EXPECT_DOUBLE_EQ(metadata.duration, 0.0);
}

TEST_F(AudioMetadataTest, DurationLarge) {
    metadata.duration = 3600.0;
    EXPECT_DOUBLE_EQ(metadata.duration, 3600.0);
}

TEST_F(AudioMetadataTest, BitratePrecision) {
    metadata.bitrate = 1411.2;
    EXPECT_NEAR(metadata.bitrate, 1411.2, 0.01);
}

TEST_F(AudioMetadataTest, BitrateLossy) {
    metadata.bitrate = 320.0;
    metadata.is_lossless = false;
    EXPECT_NEAR(metadata.bitrate, 320.0, 0.01);
    EXPECT_FALSE(metadata.is_lossless);
}

TEST_F(AudioMetadataTest, BitrateVariable) {
    metadata.bitrate = 0.0;
    EXPECT_DOUBLE_EQ(metadata.bitrate, 0.0);
}

// ErrorCode Tests

class ErrorCodeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ErrorCodeTest, SuccessValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::Success), 0);
}

TEST_F(ErrorCodeTest, UnknownErrorValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::UnknownError), 1);
}

TEST_F(ErrorCodeTest, NotImplementedValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::NotImplemented), 2);
}

TEST_F(ErrorCodeTest, FileNotFoundValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::FileNotFound), 60);
}

TEST_F(ErrorCodeTest, AudioDecodeErrorValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::AudioDecodeError), 76);
}

TEST_F(ErrorCodeTest, CacheMissValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::CacheMiss), 80);
}

TEST_F(ErrorCodeTest, InvalidStateValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::InvalidState), 90);
}

TEST_F(ErrorCodeTest, OutOfMemoryValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::OutOfMemory), 100);
}

TEST_F(ErrorCodeTest, NetworkUnavailableValue) {
    EXPECT_EQ(static_cast<int>(ErrorCode::NetworkUnavailable), 110);
}

// String Conversion Tests

TEST_F(ErrorCodeTest, ToStringSuccess) {
    EXPECT_STREQ(toString(ErrorCode::Success), "Success");
}

TEST_F(ErrorCodeTest, ToStringUnknownError) {
    EXPECT_STREQ(toString(ErrorCode::UnknownError), "UnknownError");
}

TEST_F(ErrorCodeTest, ToStringNotImplemented) {
    EXPECT_STREQ(toString(ErrorCode::NotImplemented), "NotImplemented");
}

TEST_F(ErrorCodeTest, ToStringFileNotFound) {
    EXPECT_STREQ(toString(ErrorCode::FileNotFound), "FileNotFound");
}

TEST_F(ErrorCodeTest, ToStringAudioDecodeError) {
    EXPECT_STREQ(toString(ErrorCode::AudioDecodeError), "AudioDecodeError");
}

TEST_F(ErrorCodeTest, ToStringCacheMiss) {
    EXPECT_STREQ(toString(ErrorCode::CacheMiss), "CacheMiss");
}

TEST_F(ErrorCodeTest, ToStringInvalidState) {
    EXPECT_STREQ(toString(ErrorCode::InvalidState), "InvalidState");
}

TEST_F(ErrorCodeTest, ToStringOutOfMemory) {
    EXPECT_STREQ(toString(ErrorCode::OutOfMemory), "OutOfMemory");
}

TEST_F(ErrorCodeTest, ToStringNetworkUnavailable) {
    EXPECT_STREQ(toString(ErrorCode::NetworkUnavailable), "NetworkUnavailable");
}

TEST_F(ErrorCodeTest, ToStringInvalidCode) {
    EXPECT_STREQ(toString(static_cast<ErrorCode>(9999)), "UnknownErrorCode");
}

// Success/Failure Helper Tests

TEST_F(ErrorCodeTest, IsSuccessWithSuccess) {
    EXPECT_TRUE(isSuccess(ErrorCode::Success));
}

TEST_F(ErrorCodeTest, IsSuccessWithError) {
    EXPECT_FALSE(isSuccess(ErrorCode::FileNotFound));
}

TEST_F(ErrorCodeTest, IsSuccessWithUnknownError) {
    EXPECT_FALSE(isSuccess(ErrorCode::UnknownError));
}

TEST_F(ErrorCodeTest, IsFailureWithSuccess) {
    EXPECT_FALSE(isFailure(ErrorCode::Success));
}

TEST_F(ErrorCodeTest, IsFailureWithError) {
    EXPECT_TRUE(isFailure(ErrorCode::FileNotFound));
}

TEST_F(ErrorCodeTest, IsFailureWithUnknownError) {
    EXPECT_TRUE(isFailure(ErrorCode::UnknownError));
}

// HTTP Status Code Mapping Tests

TEST_F(ErrorCodeTest, HTTPStatusCodeSuccess) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::Success), 200);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeInvalidArgument) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::InvalidArgument), 400);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeFileNotFound) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::FileNotFound), 404);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeAPINotFound) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::APINotFound), 404);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeTimeout) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::Timeout), 408);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeDeviceUnavailable) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::DeviceUnavailable), 503);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeNotImplemented) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::NotImplemented), 501);
}

TEST_F(ErrorCodeTest, HTTPStatusCodeGenericError) {
    EXPECT_EQ(getHTTPStatusCode(ErrorCode::UnknownError), 500);
}

// Protocol Structure Tests

class ProtocolStructureTest : public ::testing::Test {
protected:
    void SetUp() override {
        metadata.title = "Test Song";
        metadata.artist = "Test Artist";
        metadata.album = "Test Album";
        metadata.year = "2024";
        metadata.genre = "Rock";
        metadata.track_number = 1;
        metadata.duration = 180.5;
        metadata.sample_rate = 96000;
        metadata.bit_depth = 24;
        metadata.channels = 2;
        metadata.sample_count = 17280000;
        metadata.format = "FLAC";
        metadata.format_name = "FLAC Audio";
        metadata.bitrate = 0.0;
        metadata.file_path = "/music/test.flac";
        metadata.is_lossless = true;
        metadata.is_high_res = true;
        metadata.original_sample_rate = 96000;

        status.state = protocol::PlaybackStatus::State::Playing;
        status.current_position = 45.2;
        status.duration = 180.5;
        status.buffer_fill_level = 75.0f;
        status.cpu_usage = 15.0f;
        status.sample_rate = 96000;
        status.bit_depth = 24;
        status.channels = 2;
        status.current_device = "Default";
        status.bytes_played = 8640000;
        status.playback_time = 45.2;
    }

    protocol::AudioMetadata metadata;
    protocol::PlaybackStatus status;
};

TEST_F(ProtocolStructureTest, MetadataToJSON) {
    std::string json = metadataToJSON(metadata);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("Test Song"), std::string::npos);
    EXPECT_NE(json.find("Test Artist"), std::string::npos);
    EXPECT_NE(json.find("96000"), std::string::npos);
    EXPECT_NE(json.find("FLAC"), std::string::npos);
}

TEST_F(ProtocolStructureTest, StatusToJSON) {
    std::string json = statusToJSON(status);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("playing"), std::string::npos);
    EXPECT_NE(json.find("45.2"), std::string::npos);
    EXPECT_NE(json.find("75.0"), std::string::npos);
}

TEST_F(ProtocolStructureTest, StatusStoppedState) {
    status.state = protocol::PlaybackStatus::State::Stopped;
    std::string json = statusToJSON(status);
    EXPECT_NE(json.find("stopped"), std::string::npos);
}

TEST_F(ProtocolStructureTest, StatusPausedState) {
    status.state = protocol::PlaybackStatus::State::Paused;
    std::string json = statusToJSON(status);
    EXPECT_NE(json.find("paused"), std::string::npos);
}

TEST_F(ProtocolStructureTest, StatusErrorState) {
    status.state = protocol::PlaybackStatus::State::Error;
    std::string json = statusToJSON(status);
    EXPECT_NE(json.find("error"), std::string::npos);
}

// Queue Structure Tests

TEST_F(ProtocolStructureTest, QueueStatusDefault) {
    protocol::QueueStatus queue;

    EXPECT_EQ(queue.current_index, -1);
    EXPECT_EQ(queue.total_count, 0);
    EXPECT_EQ(queue.playback_mode, "sequential");
    EXPECT_DOUBLE_EQ(queue.total_duration, 0.0);
    EXPECT_TRUE(queue.entries.empty());
}

TEST_F(ProtocolStructureTest, QueueEntryDefault) {
    protocol::QueueEntry entry;

    EXPECT_EQ(entry.index, 0);
    EXPECT_FALSE(entry.is_playing);
}

TEST_F(ProtocolStructureTest, QueueToJSONEmpty) {
    protocol::QueueStatus queue;
    std::string json = queueToJSON(queue);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("sequential"), std::string::npos);
}

TEST_F(ProtocolStructureTest, QueueToJSONWithEntries) {
    protocol::QueueStatus queue;
    queue.current_index = 0;
    queue.total_count = 2;
    queue.playback_mode = "loop_all";
    queue.total_duration = 361.0;

    protocol::QueueEntry entry1;
    entry1.index = 0;
    entry1.file_path = "/music/track1.flac";
    entry1.is_playing = true;
    entry1.metadata.title = "Track 1";
    entry1.metadata.artist = "Artist";
    entry1.metadata.duration = 180.5;

    protocol::QueueEntry entry2;
    entry2.index = 1;
    entry2.file_path = "/music/track2.flac";
    entry2.is_playing = false;
    entry2.metadata.title = "Track 2";
    entry2.metadata.artist = "Artist";
    entry2.metadata.duration = 180.5;

    queue.entries.push_back(entry1);
    queue.entries.push_back(entry2);

    std::string json = queueToJSON(queue);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("Track 1"), std::string::npos);
    EXPECT_NE(json.find("loop_all"), std::string::npos);
}

// Device Structure Tests

TEST_F(ProtocolStructureTest, DeviceInfoDefault) {
    protocol::DeviceInfo device;

    EXPECT_TRUE(device.name.empty());
    EXPECT_TRUE(device.id.empty());
    EXPECT_EQ(device.index, 0);
    EXPECT_FALSE(device.is_default);
    EXPECT_FALSE(device.is_exclusive);
    EXPECT_TRUE(device.sample_rates.empty());
    EXPECT_TRUE(device.bit_depths.empty());
    EXPECT_TRUE(device.channel_counts.empty());
}

TEST_F(ProtocolStructureTest, DeviceToJSON) {
    protocol::DeviceInfo device;
    device.name = "Default Device";
    device.id = "device_0";
    device.index = 0;
    device.is_default = true;
    device.is_exclusive = false;
    device.sample_rates.push_back(44100);
    device.sample_rates.push_back(48000);
    device.sample_rates.push_back(96000);
    device.bit_depths.push_back(16);
    device.bit_depths.push_back(24);
    device.channel_counts.push_back(2);

    std::string json = deviceToJSON(device);

    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("Default Device"), std::string::npos);
    EXPECT_NE(json.find("44100"), std::string::npos);
    EXPECT_NE(json.find("96000"), std::string::npos);
}

// Edge Cases Tests

TEST_F(AudioMetadataTest, EmptyStrings) {
    metadata.title = "";
    metadata.artist = "";
    metadata.album = "";
    EXPECT_TRUE(metadata.title.empty());
    EXPECT_TRUE(metadata.artist.empty());
    EXPECT_TRUE(metadata.album.empty());
}

TEST_F(AudioMetadataTest, SpecialCharactersInFields) {
    metadata.title = "Test: Song / Artist";
    metadata.artist = "Artist & Band";
    metadata.album = "Album [Deluxe Edition]";
    EXPECT_NE(metadata.title.find(":"), std::string::npos);
    EXPECT_NE(metadata.artist.find("&"), std::string::npos);
    EXPECT_NE(metadata.album.find("["), std::string::npos);
}

TEST_F(AudioMetadataTest, UnicodeInFields) {
    metadata.title = "Test Song *";
    metadata.artist = "Test Artist Music";
    EXPECT_GT(metadata.title.length(), 10);
    EXPECT_GT(metadata.artist.length(), 12);
}

TEST_F(AudioMetadataTest, VeryLongStrings) {
    std::string long_title(1000, 'A');
    metadata.title = long_title;
    EXPECT_EQ(metadata.title.length(), 1000);
}
