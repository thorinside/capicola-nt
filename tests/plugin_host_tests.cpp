#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <distingnt/api.h>
#include <distingnt/wav.h>

namespace {

constexpr int kFrames = 64;
constexpr float kPi = 3.14159265358979323846f;
float gWorkBuffer[kFrames * 2] = {};
bool gCardMounted = true;
uint32_t gStreamRenderCalls = 0;
uint32_t gStreamOpenCalls = 0;
uint32_t gStreamClock = 0;
uint32_t gOpenedFolder = 0;
uint32_t gOpenedSample = 0;
float gOpenedSpeed = 0.0f;

int fail(const char* message) {
    std::printf("FAIL: %s\n", message);
    return 1;
}

int findParameter(const _NT_algorithm* algorithm, int count, const char* name) {
    for (int i = 0; i < count; ++i) {
        if (algorithm->parameters[i].name != nullptr &&
            std::strcmp(algorithm->parameters[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

} // namespace

extern "C" const _NT_globals NT_globals = {
    48000,
    kFrames,
    gWorkBuffer,
    sizeof(gWorkBuffer),
    64,
    256,
};

extern "C" bool NT_isSdCardMounted() {
    return gCardMounted;
}

extern "C" uint32_t NT_getNumSampleFolders() {
    return 2;
}

extern "C" void NT_getSampleFolderInfo(uint32_t folder, _NT_wavFolderInfo& info) {
    static const char* names[] = {"Drums", "Textures"};
    info.name = names[folder < 2 ? folder : 0];
    info.numSampleFiles = folder == 0 ? 2 : 1;
}

extern "C" void NT_getSampleFileInfo(uint32_t folder,
                                       uint32_t sample,
                                       _NT_wavInfo& info) {
    static const char* names[] = {"Mono.wav", "Stereo.wav", "Cloud.wav"};
    info.name = folder == 0 ? names[sample < 2 ? sample : 0] : names[2];
    info.numFrames = 48000;
    info.sampleRate = folder == 1 ? 24000 : 48000;
    info.channels = sample == 0 ? kNT_WavMono : kNT_WavStereo;
    info.bits = kNT_WavBits16;
}

extern "C" bool NT_streamOpen(_NT_stream, const _NT_streamOpenData& data) {
    ++gStreamOpenCalls;
    gOpenedFolder = data.folder;
    gOpenedSample = data.sample;
    gStreamClock = 0;
    return true;
}

extern "C" uint32_t NT_streamRender(_NT_stream,
                                      _NT_frame* renderBuffer,
                                      uint32_t numFrames,
                                      float speed) {
    ++gStreamRenderCalls;
    gOpenedSpeed = speed;
    for (uint32_t i = 0; i < numFrames; ++i, ++gStreamClock) {
        renderBuffer[i][0] = std::sin(2.0f * kPi * 330.0f * gStreamClock / 48000.0f);
        renderBuffer[i][1] = gOpenedSample == 0
            ? renderBuffer[i][0]
            : 0.35f * std::sin(2.0f * kPi * 710.0f * gStreamClock / 48000.0f);
    }
    return numFrames;
}

extern "C" int32_t NT_algorithmIndex(const _NT_algorithm*) {
    return 0;
}

extern "C" void NT_updateParameterDefinition(uint32_t, uint32_t) {}

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
    std::vector<std::max_align_t> dtc(
        (requirements.dtc + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
    _NT_algorithmMemoryPtrs memory{
        reinterpret_cast<uint8_t*>(sram.data()),
        reinterpret_cast<uint8_t*>(dram.data()),
        reinterpret_cast<uint8_t*>(dtc.data()),
        nullptr,
    };
    _NT_algorithm* algorithm = factory->construct(memory, requirements, nullptr);

    const int count = static_cast<int>(requirements.numParameters);
    const int leftInput = findParameter(algorithm, count, "Left input");
    const int rightInput = findParameter(algorithm, count, "Right input");
    const int leftOutput = findParameter(algorithm, count, "Left output");
    const int leftMode = findParameter(algorithm, count, "Left output mode");
    const int rightOutput = findParameter(algorithm, count, "Right output");
    const int rightMode = findParameter(algorithm, count, "Right output mode");
    const int source = findParameter(algorithm, count, "Source");
    const int folder = findParameter(algorithm, count, "Folder");
    const int sample = findParameter(algorithm, count, "Sample");
    if (leftInput < 0 || rightInput < 0 || leftOutput < 0 || leftMode < 0 ||
        rightOutput < 0 || rightMode < 0 || source < 0 || folder < 0 || sample < 0) {
        return fail("expected source and routing parameters are unavailable");
    }

    std::vector<int16_t> values(requirements.numParameters, 0);
    values[leftInput] = 1;
    values[rightInput] = 0;
    values[leftOutput] = 13;
    values[leftMode] = 1;
    values[rightOutput] = 14;
    values[rightMode] = 1;
    values[source] = 0;
    algorithm->v = values.data();

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
                return fail("left-only live input was not normalized identically");
            }
        }
    }

    // Select the host-catalogued stereo sample. NaN on both live buses proves
    // that sample mode does not read or mix either live source.
    values[source] = 1;
    values[folder] = 0;
    values[sample] = 1;
    factory->parameterChanged(algorithm, source);
    factory->parameterChanged(algorithm, sample);
    double sampleEnergy = 0.0;
    double stereoDifference = 0.0;
    for (int block = 0; block < 320; ++block) {
        std::fill_n(buses.data(), 2 * kFrames,
                    std::numeric_limits<float>::quiet_NaN());
        factory->step(algorithm, buses.data(), kFrames / 4);
        const float* outLeft = buses.data() + 12 * kFrames;
        const float* outRight = buses.data() + 13 * kFrames;
        if (block >= 270) {
            for (int i = 0; i < kFrames; ++i) {
                if (!std::isfinite(outLeft[i]) || !std::isfinite(outRight[i])) {
                    return fail("sample mode mixed or read live input");
                }
                sampleEnergy += outLeft[i] * outLeft[i] + outRight[i] * outRight[i];
                stereoDifference += std::fabs(outLeft[i] - outRight[i]);
            }
        }
    }
    if (gStreamOpenCalls == 0 || gStreamRenderCalls == 0 ||
        gOpenedFolder != 0 || gOpenedSample != 1 || gOpenedSpeed != 1.0f) {
        return fail("selected sample was not opened and rendered at host-rate speed");
    }
    if (sampleEnergy < 1.0 || stereoDifference < 1.0) {
        return fail("stereo sample was not processed through both Capicola channels");
    }

    // Folder changes update selection and sample-rate conversion before opening.
    values[folder] = 1;
    values[sample] = 0;
    factory->parameterChanged(algorithm, folder);
    if (gOpenedFolder != 1 || gOpenedSample != 0) {
        return fail("folder change did not open its selected sample");
    }
    for (int block = 0; block < 300; ++block) {
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    if (gOpenedSpeed != 0.5f) {
        return fail("sample-rate conversion speed did not follow file metadata");
    }
    const float* monoLeft = buses.data() + 12 * kFrames;
    const float* monoRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        if (monoLeft[i] != monoRight[i]) {
            return fail("host-delivered mono sample was not duplicated to both channels");
        }
    }

    // Switching back to live resets Capicola and must stop all stream renders.
    const uint32_t rendersBeforeLive = gStreamRenderCalls;
    values[source] = 0;
    factory->parameterChanged(algorithm, source);
    values[rightInput] = 2;
    std::fill(buses.begin(), buses.end(), 0.0f);
    for (int block = 0; block < 300; ++block) {
        float* left = buses.data();
        float* right = buses.data() + kFrames;
        for (int i = 0; i < kFrames; ++i, ++clock) {
            left[i] = std::sin(2.0f * kPi * 220.0f * clock / 48000.0f);
            right[i] = 0.4f * std::sin(2.0f * kPi * 733.0f * clock / 48000.0f);
        }
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    if (gStreamRenderCalls != rendersBeforeLive) {
        return fail("live mode continued rendering the sample stream");
    }
    double liveDifference = 0.0;
    const float* outLeft = buses.data() + 12 * kFrames;
    const float* outRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        liveDifference += std::fabs(outLeft[i] - outRight[i]);
    }
    if (liveDifference < 0.1) {
        return fail("live stereo did not resume independently after source switch");
    }

    char text[kNT_parameterStringSize] = {};
    if (factory->parameterString(algorithm, folder, 1, text) == 0 ||
        std::strcmp(text, "Textures") != 0) {
        return fail("folder selection is not named through the NT interface");
    }

    std::printf("plugin host live/sample path: ok\n");
    return 0;
}
