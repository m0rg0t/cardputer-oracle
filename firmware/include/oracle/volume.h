#pragma once
#include <cstdint>

namespace oracle {
constexpr std::uint8_t kDefaultVolume = 255;
constexpr std::uint8_t kEffectVolume = 160;
constexpr int kVoiceCeiling = 13500;

// Raise quiet speech, progressively compress peaks. No hard clipping or state
// at buffer boundaries. The ceiling leaves room for a simultaneous sound effect
// with M5Unified's squared volume law and Cardputer's mono magnification of 16.
inline std::int16_t loudVoice(std::int16_t sample) {
    const int signedValue = int(sample) * 2;
    const int level = signedValue < 0 ? -signedValue : signedValue;
    const int delta = level - 6000;
    const int compressed = delta > 0 ? 6000 + delta * 7500 / (delta + 7500) : level;
    return std::int16_t(signedValue < 0 ? -compressed : compressed);
}
}
