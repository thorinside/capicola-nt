#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>

#include "capicola_nt/int64_to_double.h"
#include "capicola_nt/live_path.h"

namespace {

int failures = 0;

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            ++failures;                                                        \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        }                                                                      \
    } while (false)

class PassthroughChannel {
public:
    void init() {}
    void process(const float* input, float* output, std::size_t frames) {
        for (std::size_t i = 0; i < frames; ++i) {
            output[i] = input[i];
        }
    }
};

void testInt64ToDoubleConversion() {
    const std::int64_t values[] = {
        0,
        1,
        -1,
        std::numeric_limits<std::int32_t>::max(),
        std::numeric_limits<std::int32_t>::min(),
        (std::int64_t{1} << 32) - 1,
        std::int64_t{1} << 32,
        (std::int64_t{1} << 32) + 1,
        -(std::int64_t{1} << 32) - 1,
        -(std::int64_t{1} << 32),
        -(std::int64_t{1} << 32) + 1,
        std::numeric_limits<std::int64_t>::max(),
        std::numeric_limits<std::int64_t>::min(),
    };

    for (const std::int64_t value : values) {
        CHECK(capicola_nt::int64ToDouble(value) == static_cast<double>(value));
    }
}

void testStereoCorrespondence() {
    capicola_nt::StereoLivePath<PassthroughChannel> path;
    path.init();

    const float left[] = {1.0f, 2.0f, 3.0f, 4.0f};
    const float right[] = {-1.0f, -2.0f, -3.0f, -4.0f};
    float outLeft[4] = {};
    float outRight[4] = {};
    path.process(left, right, outLeft, outRight, 4);

    for (int i = 0; i < 4; ++i) {
        CHECK(outLeft[i] == left[i]);
        CHECK(outRight[i] == right[i]);
    }
}

void testLeftNormalizationAndRightRestore() {
    capicola_nt::StereoLivePath<PassthroughChannel> path;
    path.init();

    const float left[] = {0.1f, 0.2f, 0.3f, 0.4f};
    const float right[] = {0.9f, 0.8f, 0.7f, 0.6f};
    float outLeft[4] = {};
    float outRight[4] = {};

    path.process(left, nullptr, outLeft, outRight, 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(outLeft[i] == left[i]);
        CHECK(outRight[i] == left[i]);
    }

    path.process(left, right, outLeft, outRight, 4);
    for (int i = 0; i < 4; ++i) {
        CHECK(outLeft[i] == left[i]);
        CHECK(outRight[i] == right[i]);
    }
}

void testCapicolaProcessesNormalizedStereo() {
    static capicola_nt::CapicolaStereoLivePath<4096> path;
    path.init();

    constexpr int frames = 64;
    float input[frames];
    float outLeft[frames] = {};
    float outRight[frames] = {};
    int clock = 0;

    for (int block = 0; block < 300; ++block) {
        for (int i = 0; i < frames; ++i, ++clock) {
            input[i] = std::sin(2.0 * M_PI * 220.0 * clock / 48000.0);
        }
        path.process(input, nullptr, outLeft, outRight, frames);
        for (int i = 0; i < frames; ++i) {
            CHECK(std::isfinite(outLeft[i]));
            CHECK(outLeft[i] == outRight[i]);
        }
    }

    float energy = 0.0f;
    for (float sample : outLeft) {
        energy += sample * sample;
    }
    CHECK(energy > 0.01f);
}

void testLivePitchAndGrainChangesRemainFinite() {
    static capicola_nt::CapicolaStereoLivePath<16384> path;
    path.init();
    path.setDrive(1.0f);
    path.setDriveCharacter(1.0f);

    constexpr int frames = 64;
    constexpr float pitchValues[] = {-12.0f, 12.0f, -6.0f, 6.0f, 0.0f};
    constexpr int grainValues[] = {32, 4096, 64, 2048, 128, 1024};
    float input[frames] = {};
    float outLeft[frames] = {};
    float outRight[frames] = {};
    std::uint32_t noiseState = 0x9e3779b9U;

    for (int block = 0; block < 12000; ++block) {
        if (block % 5 == 0) {
            const int change = block / 5;
            path.setPitchSemitones(
                pitchValues[change % static_cast<int>(
                    sizeof(pitchValues) / sizeof(pitchValues[0]))]);
            path.setGrainSize(
                grainValues[change % static_cast<int>(
                    sizeof(grainValues) / sizeof(grainValues[0]))]);
        }
        for (int i = 0; i < frames; ++i) {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            input[i] = static_cast<float>(
                static_cast<std::int32_t>(noiseState)) / 2147483648.0f;
        }
        path.process(input, nullptr, outLeft, outRight, frames);
        for (int i = 0; i < frames; ++i) {
            CHECK(std::isfinite(outLeft[i]));
            CHECK(std::isfinite(outRight[i]));
            CHECK(outLeft[i] == outRight[i]);
        }
    }
}

void testFeedbackDriveToneAndMixChangesRemainFinite() {
    static capicola_nt::CapicolaStereoLivePath<16384> path;
    path.init();
    path.setDriveCharacter(1.0f);

    constexpr int frames = 64;
    constexpr float feedbackValues[] = {0.0f, 0.5f, 1.0f, 1.5f};
    constexpr float toneValues[] = {0.002f, 0.02f, 0.2f, 0.9f};
    constexpr float driveValues[] = {0.5f, 1.0f, 2.5f, 4.0f};
    constexpr float mixValues[] = {0.0f, 0.25f, 0.75f, 1.0f};
    float input[frames] = {};
    float outLeft[frames] = {};
    float outRight[frames] = {};
    std::uint32_t noiseState = 0x243f6a88U;

    for (int block = 0; block < 20000; ++block) {
        if (block % 3 == 0) {
            const int change = block / 3;
            const int index = change % 4;
            path.setFeedback(feedbackValues[index]);
            path.setFeedbackTone(toneValues[(index + 1) % 4]);
            path.setDrive(driveValues[(index + 2) % 4]);
            path.setMix(mixValues[(index + 3) % 4]);
        }
        for (int i = 0; i < frames; ++i) {
            noiseState ^= noiseState << 13;
            noiseState ^= noiseState >> 17;
            noiseState ^= noiseState << 5;
            input[i] = 5.0f * static_cast<float>(
                static_cast<std::int32_t>(noiseState)) / 2147483648.0f;
        }
        path.process(input, nullptr, outLeft, outRight, frames);
        for (int i = 0; i < frames; ++i) {
            CHECK(std::isfinite(outLeft[i]));
            CHECK(std::isfinite(outRight[i]));
            CHECK(outLeft[i] == outRight[i]);
        }
    }
}

void testFarSparseWindowSeekUsesBoundedFallback() {
    capicola::SparseLine<16384> line;
    line.Init();
    for (int i = 0; i < 16384; ++i) {
        line.Write({static_cast<float>(i), static_cast<double>(i)});
    }

    capicola::Window window = line.WindowAtLatest();
    line.StepWindow(window, 0.0);
    CHECK(window.index == 1);
    CHECK(window.p2->time == 0.0);
    CHECK(window.p1->time == 1.0);

    line.StepWindow(window, 16383.0);
    CHECK(window.index == 16383);
    CHECK(window.p2->time == 16382.0);
    CHECK(window.p1->time == 16383.0);

    line.Clear();
    CHECK(line.GetLatest()->value == 0.0f);
    CHECK(line.GetLatest()->time == 1.0);
}

} // namespace

int main() {
    testInt64ToDoubleConversion();
    testStereoCorrespondence();
    testLeftNormalizationAndRightRestore();
    testCapicolaProcessesNormalizedStereo();
    testLivePitchAndGrainChangesRemainFinite();
    testFeedbackDriveToneAndMixChangesRemainFinite();
    testFarSparseWindowSeekUsesBoundedFallback();
    std::printf("live path: %s\n", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
