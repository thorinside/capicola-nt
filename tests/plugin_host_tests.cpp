#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

#include <distingnt/api.h>

extern "C" const _NT_globals NT_globals = {
    48000,
    64,
    nullptr,
    0,
    0,
    0,
};

namespace {

constexpr float kPi = 3.14159265358979323846f;

int fail(const char* message) {
    std::printf("FAIL: %s\n", message);
    return 1;
}

} // namespace

int main() {
    const auto* factory = reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0));
    if (factory == nullptr || pluginEntry(kNT_selector_version, 0) != kNT_apiVersion13) {
        return fail("API v13 factory is unavailable");
    }

    _NT_algorithmRequirements requirements{};
    factory->calculateRequirements(requirements, nullptr);
    std::vector<std::max_align_t> sram(
        (requirements.sram + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    std::vector<std::max_align_t> dram(
        (requirements.dram + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    _NT_algorithmMemoryPtrs memory{
        reinterpret_cast<uint8_t*>(sram.data()),
        reinterpret_cast<uint8_t*>(dram.data()),
        nullptr,
        nullptr,
    };
    _NT_algorithm* algorithm = factory->construct(memory, requirements, nullptr);

    // Left input 1, right disconnected, outputs 13/14 in Replace mode.
    int16_t values[] = {1, 0, 13, 1, 14, 1};
    algorithm->v = values;

    constexpr int kFrames = 64;
    std::vector<float> buses(kNT_lastBus * kFrames, 0.0f);
    int clock = 0;
    for (int block = 0; block < 300; ++block) {
        float* left = buses.data();
        for (int i = 0; i < kFrames; ++i, ++clock) {
            left[i] = std::sin(2.0f * kPi * 220.0f * clock / 48000.0f);
        }
        factory->step(algorithm, buses.data(), kFrames / 4);
        const float* outLeft = buses.data() + 12 * kFrames;
        const float* outRight = buses.data() + 13 * kFrames;
        for (int i = 0; i < kFrames; ++i) {
            if (!std::isfinite(outLeft[i]) || outLeft[i] != outRight[i]) {
                return fail("left-only input was not normalized identically");
            }
        }
    }

    // Connect right input 2 with distinct audio. The channels must become
    // independent rather than summing, swapping, or continuing normalization.
    values[1] = 2;
    double difference = 0.0;
    for (int block = 0; block < 300; ++block) {
        float* left = buses.data();
        float* right = buses.data() + kFrames;
        for (int i = 0; i < kFrames; ++i, ++clock) {
            left[i] = std::sin(2.0f * kPi * 220.0f * clock / 48000.0f);
            right[i] = 0.4f * std::sin(2.0f * kPi * 733.0f * clock / 48000.0f);
        }
        factory->step(algorithm, buses.data(), kFrames / 4);
        if (block >= 250) {
            const float* outLeft = buses.data() + 12 * kFrames;
            const float* outRight = buses.data() + 13 * kFrames;
            for (int i = 0; i < kFrames; ++i) {
                difference += std::fabs(outLeft[i] - outRight[i]);
            }
        }
    }
    if (difference < 1.0) {
        return fail("connecting right did not restore independent stereo");
    }

    std::printf("plugin host path: ok\n");
    return 0;
}
