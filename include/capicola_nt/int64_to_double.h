// Capicola for disting NT: independently maintained wrapper integration, 2026.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cstdint>

namespace capicola_nt {

// GCC lowers a direct int64_t-to-double conversion to __aeabi_l2d on ARM.
// The NT supports double arithmetic, but does not export that compiler-runtime
// helper to plug-ins. Compose the value from the M7's supported 32-bit
// conversions while retaining the analyzer's 64-bit timeline.
inline double int64ToDouble(std::int64_t value) {
    const bool negative = value < 0;
    const std::uint64_t magnitude = negative
        ? static_cast<std::uint64_t>(-(value + 1)) + 1
        : static_cast<std::uint64_t>(value);
    const std::uint32_t high = static_cast<std::uint32_t>(magnitude >> 32);
    const std::uint32_t low = static_cast<std::uint32_t>(magnitude);
    const double converted = static_cast<double>(high) * 4294967296.0 +
                             static_cast<double>(low);
    return negative ? -converted : converted;
}

} // namespace capicola_nt
