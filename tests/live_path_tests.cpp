#include <cmath>
#include <cstdio>

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

} // namespace

int main() {
    testStereoCorrespondence();
    testLeftNormalizationAndRightRestore();
    testCapicolaProcessesNormalizedStereo();
    std::printf("live path: %s\n", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}
