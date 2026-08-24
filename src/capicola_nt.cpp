#include <cstddef>
#include <new>

#include <distingnt/api.h>

#include "capicola_nt/live_path.h"

namespace {

// This ring supports the audited maximum 4096-keyframe grain while retaining
// additional live history. The two rings live in host-provided DRAM.
constexpr int kRingFrames = 16384;
using LivePath = capicola_nt::CapicolaStereoLivePath<kRingFrames>;

enum Parameter {
    kParamLeftInput,
    kParamRightInput,
    kParamLeftOutput,
    kParamLeftOutputMode,
    kParamRightOutput,
    kParamRightOutputMode,
};

static const _NT_parameter kParameters[] = {
    NT_PARAMETER_AUDIO_INPUT("Left input", 1, 1)
    NT_PARAMETER_AUDIO_INPUT("Right input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Left output", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Right output", 1, 14)
};

static const uint8_t kRoutingParameters[] = {
    kParamLeftInput,
    kParamRightInput,
    kParamLeftOutput,
    kParamLeftOutputMode,
    kParamRightOutput,
    kParamRightOutputMode,
};

static const _NT_parameterPage kPages[] = {
    {"Routing", ARRAY_SIZE(kRoutingParameters), 0, {0, 0}, kRoutingParameters},
};

static const _NT_parameterPages kParameterPages = {
    ARRAY_SIZE(kPages),
    kPages,
};

struct Algorithm : public _NT_algorithm {
    LivePath* livePath;
    float* scratchLeft;
    float* scratchRight;
};

void calculateRequirements(_NT_algorithmRequirements& requirements,
                           const int32_t*) {
    requirements.numParameters = ARRAY_SIZE(kParameters);
    requirements.sram = sizeof(Algorithm);
    requirements.dram = sizeof(LivePath) +
                        2U * NT_globals.maxFramesPerStep * sizeof(float);
    requirements.dtc = 0;
    requirements.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& memory,
                         const _NT_algorithmRequirements&,
                         const int32_t*) {
    Algorithm* algorithm = new (memory.sram) Algorithm();
    algorithm->parameters = kParameters;
    algorithm->parameterPages = &kParameterPages;
    algorithm->livePath = new (memory.dram) LivePath();
    algorithm->livePath->init();

    algorithm->scratchLeft = reinterpret_cast<float*>(memory.dram + sizeof(LivePath));
    algorithm->scratchRight = algorithm->scratchLeft + NT_globals.maxFramesPerStep;
    return algorithm;
}

void writeOutput(float* destination,
                 const float* source,
                 int frames,
                 bool replace) {
    if (replace) {
        for (int i = 0; i < frames; ++i) {
            destination[i] = source[i];
        }
    } else {
        for (int i = 0; i < frames; ++i) {
            destination[i] += source[i];
        }
    }
}

void step(_NT_algorithm* base, float* busFrames, int numFramesBy4) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    const int frames = numFramesBy4 * 4;
    if (frames <= 0 || static_cast<uint32_t>(frames) > NT_globals.maxFramesPerStep) {
        return;
    }

    const int leftInputBus = algorithm->v[kParamLeftInput];
    const int rightInputBus = algorithm->v[kParamRightInput];
    const float* left = busFrames + (leftInputBus - 1) * frames;
    const float* right = rightInputBus == 0
        ? nullptr
        : busFrames + (rightInputBus - 1) * frames;

    // Render both channels before touching any output bus. This also preserves
    // correct routing when a user selects buses that alias an input bus.
    algorithm->livePath->process(left,
                                 right,
                                 algorithm->scratchLeft,
                                 algorithm->scratchRight,
                                 static_cast<std::size_t>(frames));

    float* leftOutput = busFrames +
        (algorithm->v[kParamLeftOutput] - 1) * frames;
    float* rightOutput = busFrames +
        (algorithm->v[kParamRightOutput] - 1) * frames;
    writeOutput(leftOutput,
                algorithm->scratchLeft,
                frames,
                algorithm->v[kParamLeftOutputMode] != 0);
    writeOutput(rightOutput,
                algorithm->scratchRight,
                frames,
                algorithm->v[kParamRightOutputMode] != 0);
}

static const _NT_factory kFactory = {
    .guid = NT_MULTICHAR('C', 'a', 'N', 'T'),
    .name = "Capicola",
    .description = "Capicola live stereo processor",
    .numSpecifications = 0,
    .specifications = nullptr,
    .calculateStaticRequirements = nullptr,
    .initialise = nullptr,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = nullptr,
    .step = step,
    .draw = nullptr,
    .midiRealtime = nullptr,
    .midiMessage = nullptr,
    .tags = kNT_tagEffect,
    .hasCustomUi = nullptr,
    .customUi = nullptr,
    .setupUi = nullptr,
    .serialise = nullptr,
    .deserialise = nullptr,
    .midiSysEx = nullptr,
    .parameterUiPrefix = nullptr,
    .parameterString = nullptr,
};

} // namespace

extern "C" uintptr_t pluginEntry(_NT_selector selector, uint32_t data) {
    switch (selector) {
        case kNT_selector_version:
            return kNT_apiVersion13;
        case kNT_selector_numFactories:
            return 1;
        case kNT_selector_factoryInfo:
            return data == 0 ? reinterpret_cast<uintptr_t>(&kFactory) : 0;
        default:
            return 0;
    }
}
