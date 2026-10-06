// Capicola for disting NT: independently maintained wrapper integration, 2026.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cmath>
#include <cstdint>

namespace capicola_nt {

constexpr int kPresetFormatVersion = 2;
constexpr int kBipolarLimit = 10000;

inline int clampControl(int value, int minimum, int maximum) {
    return value < minimum ? minimum : value > maximum ? maximum : value;
}

// One-time affine range conversion, rounded to the nearest new raw value.
// These deliberately do not preserve the old sound or modulation tapers.
inline int pitchValueFromLegacy(int value) {
    const int numerator = clampControl(value, -120, 120) * 10000;
    return numerator < 0 ? -((-numerator + 60) / 120) : (numerator + 60) / 120;
}

inline int stretchValueFromLegacy(int value) {
    return clampControl(value, 0, 100) * 200 - 10000;
}

inline float bipolarPitchRate(int value) {
    return static_cast<float>(clampControl(value, -kBipolarLimit, kBipolarLimit)) * 0.0002f;
}

inline float bipolarStretchRate(int value) {
    const float magnitude = static_cast<float>(
        clampControl(value < 0 ? -value : value, 0, kBipolarLimit)) * 0.0001f;
    const float rate = std::pow(magnitude, 2.5f);
    return value < 0 ? -rate : rate;
}

} // namespace capicola_nt
