/**
 * @file Equalizer.cpp
 * @brief 3-band equalizer implementation
 */

#include "Equalizer.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace xpu;

namespace xpu {
namespace process {

Equalizer::Equalizer()
    : bass_gain_db_(0.0f)
    , mid_gain_db_(0.0f)
    , treble_gain_db_(0.0f) {}

ErrorCode Equalizer::setBandGain(int band, float gain_db) {
    // Clamp gain to valid range
    gain_db = std::max(-20.0f, std::min(20.0f, gain_db));

    switch (band) {
        case 0:  // Bass
            bass_gain_db_ = gain_db;
            break;
        case 1:  // Mid
            mid_gain_db_ = gain_db;
            break;
        case 2:  // Treble
            treble_gain_db_ = gain_db;
            break;
        default:
            return ErrorCode::InvalidOperation;
    }

    return ErrorCode::Success;
}

float Equalizer::getBandGain(int band) const {
    switch (band) {
        case 0: return bass_gain_db_;
        case 1: return mid_gain_db_;
        case 2: return treble_gain_db_;
        default: return 0.0f;
    }
}

ErrorCode Equalizer::loadPreset(EQPreset preset) {
    switch (preset) {
        case EQPreset::Flat:
            bass_gain_db_ = 0.0f;
            mid_gain_db_ = 0.0f;
            treble_gain_db_ = 0.0f;
            break;

        case EQPreset::Rock:
            bass_gain_db_ = 5.0f;
            mid_gain_db_ = -2.0f;
            treble_gain_db_ = 4.0f;
            break;

        case EQPreset::Pop:
            bass_gain_db_ = 3.0f;
            mid_gain_db_ = 0.0f;
            treble_gain_db_ = 2.0f;
            break;

        case EQPreset::Classical:
            bass_gain_db_ = 3.0f;
            mid_gain_db_ = 2.0f;
            treble_gain_db_ = 0.0f;
            break;

        case EQPreset::Jazz:
            bass_gain_db_ = 2.0f;
            mid_gain_db_ = 3.0f;
            treble_gain_db_ = 1.0f;
            break;

        case EQPreset::Electronic:
            bass_gain_db_ = 6.0f;
            mid_gain_db_ = -3.0f;
            treble_gain_db_ = 3.0f;
            break;
    }

    return ErrorCode::Success;
}

void Equalizer::process(float* data, int frames, int channels, int sample_rate) {
    if (bass_gain_db_ == 0.0f && mid_gain_db_ == 0.0f && treble_gain_db_ == 0.0f) {
        return;  // No EQ needed
    }

    // Process each channel separately
    for (int ch = 0; ch < channels; ++ch) {
        // Apply bass filter (low shelf)
        if (bass_gain_db_ != 0.0f) {
            applyShelvingFilter(data, frames, bass_gain_db_, 200.0f,
                              sample_rate, &bass_filter_[ch]);
        }

        // Apply mid filter (peaking)
        if (mid_gain_db_ != 0.0f) {
            applyPeakingFilter(data, frames, mid_gain_db_, 1000.0f,
                             1.0f, sample_rate, &mid_filter_[ch]);
        }

        // Apply treble filter (high shelf)
        if (treble_gain_db_ != 0.0f) {
            applyShelvingFilter(data, frames, treble_gain_db_, 3000.0f,
                              sample_rate, &treble_filter_[ch]);
        }
    }
}

void Equalizer::reset() {
    bass_gain_db_ = 0.0f;
    mid_gain_db_ = 0.0f;
    treble_gain_db_ = 0.0f;

    for (int i = 0; i < 2; ++i) {
        bass_filter_[i] = FilterState();
        mid_filter_[i] = FilterState();
        treble_filter_[i] = FilterState();
    }
}

void Equalizer::applyShelvingFilter(float* data, int frames,
                                   float gain_db, float frequency,
                                   float sample_rate, FilterState* state) {
    // Convert gain to linear
    float A = std::pow(10.0f, gain_db / 40.0f);

    // Calculate filter coefficients (2nd order shelf)
    float w0 = 2.0f * M_PI * frequency / sample_rate;
    float s = std::sin(w0);
    float c = std::cos(w0);
    float alpha = s / 2.0f * std::sqrt((A + 1.0f / A) * (1.0f / 0.707f - 1.0f) + 2.0f);

    float b0 = A * ((A + 1.0f) - (A - 1.0f) * c + alpha * 2.0f * std::sqrt(A));
    float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * c);
    float b2 = A * ((A + 1.0f) - (A - 1.0f) * c - alpha * 2.0f * std::sqrt(A));
    float a0 = (A + 1.0f) + (A - 1.0f) * c + alpha * 2.0f * std::sqrt(A);
    float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * c);
    float a2 = (A + 1.0f) + (A - 1.0f) * c - alpha * 2.0f * std::sqrt(A);

    // Normalize coefficients
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;

    // Apply filter (biquad)
    for (int i = 0; i < frames; ++i) {
        float x0 = data[i];
        float y0 = b0 * x0 + b1 * state->x1 + b2 * state->x2
                  - a1 * state->y1 - a2 * state->y2;

        state->x2 = state->x1;
        state->x1 = x0;
        state->y2 = state->y1;
        state->y1 = y0;

        data[i] = y0;
    }
}

void Equalizer::applyPeakingFilter(float* data, int frames,
                                  float gain_db, float frequency,
                                  float Q, float sample_rate, FilterState* state) {
    // Convert gain to linear
    float A = std::pow(10.0f, gain_db / 40.0f);

    // Calculate filter coefficients
    float w0 = 2.0f * M_PI * frequency / sample_rate;
    float s = std::sin(w0);
    float c = std::cos(w0);
    float alpha = s / (2.0f * Q);

    float b0 = 1.0f + alpha * A;
    float b1 = -2.0f * c;
    float b2 = 1.0f - alpha * A;
    float a0 = 1.0f + alpha / A;
    float a1 = -2.0f * c;
    float a2 = 1.0f - alpha / A;

    // Normalize coefficients
    b0 /= a0;
    b1 /= a0;
    b2 /= a0;
    a1 /= a0;
    a2 /= a0;

    // Apply filter (biquad)
    for (int i = 0; i < frames; ++i) {
        float x0 = data[i];
        float y0 = b0 * x0 + b1 * state->x1 + b2 * state->x2
                  - a1 * state->y1 - a2 * state->y2;

        state->x2 = state->x1;
        state->x1 = x0;
        state->y2 = state->y1;
        state->y1 = y0;

        data[i] = y0;
    }
}

} // namespace process
} // namespace xpu
