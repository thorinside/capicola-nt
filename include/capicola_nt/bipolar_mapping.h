// Capicola for disting NT: independently maintained wrapper integration, 2026.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

namespace capicola_nt {

// Center preserves the legacy control exactly. The negative half passes
// through an integer-valued stop at -50% before reaching full reverse.
inline float bipolarRate(float legacyRate, int position, float limit) {
    if (position <= -100) return -limit;
    if (position <= -50) {
        return limit * static_cast<float>(position + 50) * 0.02f;
    }
    if (position < 0) {
        return legacyRate * static_cast<float>(position + 50) * 0.02f;
    }
    if (position == 0) return legacyRate;
    if (position >= 100) return limit;
    return legacyRate + (limit - legacyRate) *
        (static_cast<float>(position) * 0.01f);
}

} // namespace capicola_nt
