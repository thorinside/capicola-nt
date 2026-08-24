#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

#include <distingnt/api.h>
#include <distingnt/wav.h>

#include "capicola_nt/live_path.h"

namespace {

// This ring supports the audited maximum 4096-keyframe grain while retaining
// additional source history. The two rings live in host-provided DRAM.
constexpr int kRingFrames = 16384;
using SourceProcessor = capicola_nt::CapicolaStereoLivePath<kRingFrames>;

enum Parameter {
    kParamLeftInput,
    kParamRightInput,
    kParamLeftOutput,
    kParamLeftOutputMode,
    kParamRightOutput,
    kParamRightOutputMode,
    kParamSource,
    kParamFolder,
    kParamSample,
    kParamPitch,
    kParamStretch,
    kParamThreshold,
    kParamGrainSize,
    kParamQuality,
    kParamFeedback,
    kParamMix,
    kNumParameters,
};

enum SourceMode {
    kSourceLive,
    kSourceSample,
};

static const char* const kSourceNames[] = {"Live", "Sample"};

static const _NT_parameter kParameterTemplate[] = {
    NT_PARAMETER_AUDIO_INPUT("Left input", 1, 1)
    NT_PARAMETER_AUDIO_INPUT("Right input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Left output", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Right output", 1, 14)
    {.name = "Source", .min = kSourceLive, .max = kSourceSample, .def = kSourceLive,
     .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kSourceNames},
    {.name = "Folder", .min = 0, .max = 0, .def = 0,
     .unit = kNT_unitHasStrings, .scaling = 0, .enumStrings = nullptr},
    {.name = "Sample", .min = 0, .max = 0, .def = 0,
     .unit = kNT_unitConfirm, .scaling = 0, .enumStrings = nullptr},
    {.name = "Pitch", .min = -120, .max = 120, .def = 0,
     .unit = kNT_unitSemitones, .scaling = kNT_scaling10, .enumStrings = nullptr},
    {.name = "Stretch", .min = 0, .max = 100, .def = 0,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
    {.name = "Threshold", .min = 0, .max = 100, .def = 22,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
    {.name = "Grain Size", .min = 32, .max = 4096, .def = 128,
     .unit = kNT_unitNone, .scaling = 0, .enumStrings = nullptr},
    {.name = "Quality", .min = 0, .max = 100, .def = 100,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
    {.name = "Feedback", .min = 0, .max = 150, .def = 0,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
    {.name = "Mix", .min = 0, .max = 100, .def = 100,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
};

static const uint8_t kPerformanceParameters[] = {
    kParamPitch,
    kParamStretch,
    kParamThreshold,
    kParamGrainSize,
    kParamQuality,
    kParamFeedback,
    kParamMix,
};

static const uint8_t kSourceParameters[] = {
    kParamSource,
    kParamFolder,
    kParamSample,
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
    {"Performance", ARRAY_SIZE(kPerformanceParameters), 0, {0, 0}, kPerformanceParameters},
    {"Source", ARRAY_SIZE(kSourceParameters), 0, {0, 0}, kSourceParameters},
    {"Routing", ARRAY_SIZE(kRoutingParameters), 0, {0, 0}, kRoutingParameters},
};

static const _NT_parameterPages kParameterPages = {
    ARRAY_SIZE(kPages),
    kPages,
};

struct Algorithm : public _NT_algorithm {
    _NT_parameter params[kNumParameters];
    SourceProcessor* processor;
    void* streamBuffer;
    _NT_stream stream;
    float* scratchLeft;
    float* scratchRight;
    float* sampleLeft;
    float* sampleRight;
    float sampleSpeed;
    bool cardMounted;
    bool streamOpen;
    SourceMode activeSource;
    bool alternateControls;
};

void calculateRequirements(_NT_algorithmRequirements& requirements,
                           const int32_t*) {
    requirements.numParameters = kNumParameters;
    requirements.sram = sizeof(Algorithm);
    requirements.dram = sizeof(SourceProcessor) + NT_globals.streamBufferSizeBytes +
                        4U * NT_globals.maxFramesPerStep * sizeof(float);
    requirements.dtc = NT_globals.streamSizeBytes;
    requirements.itc = 0;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& memory,
                         const _NT_algorithmRequirements&,
                         const int32_t*) {
    static_assert(ARRAY_SIZE(kParameterTemplate) == kNumParameters);

    Algorithm* algorithm = new (memory.sram) Algorithm();
    std::memcpy(algorithm->params, kParameterTemplate, sizeof(kParameterTemplate));
    algorithm->parameters = algorithm->params;
    algorithm->parameterPages = &kParameterPages;

    algorithm->processor = new (memory.dram) SourceProcessor();
    algorithm->processor->init();
    uint8_t* cursor = memory.dram + sizeof(SourceProcessor);
    algorithm->streamBuffer = cursor;
    cursor += NT_globals.streamBufferSizeBytes;
    algorithm->scratchLeft = reinterpret_cast<float*>(cursor);
    algorithm->scratchRight = algorithm->scratchLeft + NT_globals.maxFramesPerStep;
    algorithm->sampleLeft = algorithm->scratchRight + NT_globals.maxFramesPerStep;
    algorithm->sampleRight = algorithm->sampleLeft + NT_globals.maxFramesPerStep;

    algorithm->stream = memory.dtc;
    algorithm->sampleSpeed = 1.0f;
    algorithm->cardMounted = false;
    algorithm->streamOpen = false;
    algorithm->activeSource = kSourceLive;
    algorithm->alternateControls = false;
    return algorithm;
}

float stretchValue(int32_t value) {
    const float normalized = static_cast<float>(value) * 0.01f;
    return std::pow(1.0f - normalized, 2.5f);
}

float thresholdValue(int32_t value) {
    const float normalized = static_cast<float>(value) * 0.01f;
    if (normalized > 0.99f) return 1.0e9f;
    return normalized <= 0.9f
        ? normalized * (4.0f / 0.9f)
        : 4.0f + (normalized - 0.9f) * 40.0f;
}

void applyProcessingControls(Algorithm* algorithm) {
    algorithm->processor->setPitchSemitones(
        static_cast<float>(algorithm->v[kParamPitch]) * 0.1f);
    algorithm->processor->setStretch(stretchValue(algorithm->v[kParamStretch]));
    algorithm->processor->setTransientThreshold(
        thresholdValue(algorithm->v[kParamThreshold]));
    algorithm->processor->setGrainSize(algorithm->v[kParamGrainSize]);
    algorithm->processor->setQuality(
        0.1f - static_cast<float>(algorithm->v[kParamQuality]) * 0.00099f);
    algorithm->processor->setFeedback(
        static_cast<float>(algorithm->v[kParamFeedback]) * 0.01f);
    algorithm->processor->setMix(
        static_cast<float>(algorithm->v[kParamMix]) * 0.01f);
}

bool catalogIndex(int32_t value, uint32_t count, uint32_t& index) {
    if (value < 0) {
        return false;
    }
    index = static_cast<uint32_t>(value);
    return index < count;
}

void updateSampleRange(Algorithm* algorithm) {
    const uint32_t folderCount = NT_getNumSampleFolders();
    uint32_t folder = 0;
    _NT_wavFolderInfo folderInfo{};
    if (catalogIndex(algorithm->v[kParamFolder], folderCount, folder)) {
        NT_getSampleFolderInfo(folder, folderInfo);
    }
    algorithm->params[kParamSample].max = folderInfo.numSampleFiles == 0
        ? 0
        : static_cast<int16_t>(folderInfo.numSampleFiles - 1U > 32767U
            ? 32767U : folderInfo.numSampleFiles - 1U);
    NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamSample);
}

void openSelectedSample(Algorithm* algorithm) {
    algorithm->streamOpen = false;
    if (algorithm->activeSource == kSourceSample) {
        // A new or failed selection replaces the previous sample immediately;
        // never render history retained from another file.
        algorithm->processor->init();
    }
    if (!algorithm->cardMounted || algorithm->activeSource != kSourceSample) {
        return;
    }

    const uint32_t folderCount = NT_getNumSampleFolders();
    uint32_t folder = 0;
    if (!catalogIndex(algorithm->v[kParamFolder], folderCount, folder)) {
        return;
    }
    _NT_wavFolderInfo folderInfo{};
    NT_getSampleFolderInfo(folder, folderInfo);
    uint32_t sample = 0;
    if (!catalogIndex(algorithm->v[kParamSample],
                      folderInfo.numSampleFiles,
                      sample)) {
        return;
    }
    _NT_wavInfo info{};
    NT_getSampleFileInfo(folder, sample, info);
    if (info.sampleRate == 0 || NT_globals.sampleRate == 0) {
        return;
    }

    const _NT_streamOpenData data = {
        .streamBuffer = algorithm->streamBuffer,
        .folder = folder,
        .sample = sample,
        .velocity = 1.0f,
        .startOffset = 0,
        .reverse = false,
        .rrMode = kNT_RRModeSequential,
    };
    algorithm->sampleSpeed = static_cast<float>(info.sampleRate) /
                             static_cast<float>(NT_globals.sampleRate);
    algorithm->streamOpen = NT_streamOpen(algorithm->stream, data);
}

void selectSource(Algorithm* algorithm, SourceMode source) {
    if (algorithm->activeSource == source) {
        return;
    }
    // Reset Capicola at the source boundary so no history from the replaced
    // source is rendered after the switch.
    algorithm->activeSource = source;
    algorithm->processor->init();
    algorithm->streamOpen = false;
}

void parameterChanged(_NT_algorithm* base, int parameter) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    switch (parameter) {
        case kParamSource:
            selectSource(algorithm,
                         algorithm->v[kParamSource] == kSourceSample
                             ? kSourceSample : kSourceLive);
            break;
        case kParamFolder:
            algorithm->streamOpen = false;
            if (algorithm->activeSource == kSourceSample) {
                algorithm->processor->init();
            }
            if (algorithm->cardMounted) {
                updateSampleRange(algorithm);
            }
            break;
        case kParamSample:
            openSelectedSample(algorithm);
            break;
        case kParamPitch:
        case kParamStretch:
        case kParamThreshold:
        case kParamGrainSize:
        case kParamQuality:
        case kParamFeedback:
        case kParamMix:
            applyProcessingControls(algorithm);
            break;
        default:
            break;
    }
}

int parameterString(_NT_algorithm* base, int parameter, int value, char* buffer) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    const char* name = nullptr;
    if (!algorithm->cardMounted) {
        return 0;
    }

    const uint32_t folderCount = NT_getNumSampleFolders();
    if (folderCount == 0) {
        return 0;
    }
    if (parameter == kParamFolder) {
        uint32_t folder = 0;
        if (!catalogIndex(value, folderCount, folder)) {
            return 0;
        }
        _NT_wavFolderInfo info{};
        NT_getSampleFolderInfo(folder, info);
        name = info.name;
    } else if (parameter == kParamSample) {
        uint32_t folder = 0;
        if (!catalogIndex(algorithm->v[kParamFolder], folderCount, folder)) {
            return 0;
        }
        _NT_wavFolderInfo folderInfo{};
        NT_getSampleFolderInfo(folder, folderInfo);
        uint32_t sample = 0;
        if (!catalogIndex(value, folderInfo.numSampleFiles, sample)) {
            return 0;
        }
        _NT_wavInfo info{};
        NT_getSampleFileInfo(folder, sample, info);
        name = info.name;
    }
    if (name == nullptr) {
        return 0;
    }
    std::strncpy(buffer, name, kNT_parameterStringSize - 1);
    buffer[kNT_parameterStringSize - 1] = '\0';
    return static_cast<int>(std::strlen(buffer));
}

void updateCardState(Algorithm* algorithm) {
    const bool mounted = NT_isSdCardMounted();
    if (mounted == algorithm->cardMounted) {
        return;
    }
    algorithm->cardMounted = mounted;
    algorithm->streamOpen = false;
    algorithm->processor->init();

    const uint32_t folderCount = mounted ? NT_getNumSampleFolders() : 0;
    algorithm->params[kParamFolder].max = folderCount == 0
        ? 0
        : static_cast<int16_t>(folderCount - 1U > 32767U
            ? 32767U : folderCount - 1U);
    NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamFolder);
    if (mounted) {
        updateSampleRange(algorithm);
    }
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
    selectSource(algorithm,
                 algorithm->v[kParamSource] == kSourceSample
                     ? kSourceSample : kSourceLive);
    updateCardState(algorithm);
    // Host parameter mapping can update v[] between callbacks; applying the
    // linked controls here keeps UI, mapped CV, live, and sample processing on
    // the same confirmed Capicola control surface.
    applyProcessingControls(algorithm);

    const float* left = nullptr;
    const float* right = nullptr;
    if (algorithm->activeSource == kSourceLive) {
        const int leftInputBus = algorithm->v[kParamLeftInput];
        const int rightInputBus = algorithm->v[kParamRightInput];
        left = busFrames + (leftInputBus - 1) * frames;
        right = rightInputBus == 0
            ? left
            : busFrames + (rightInputBus - 1) * frames;
    } else {
        std::memset(algorithm->sampleLeft, 0, static_cast<std::size_t>(frames) * sizeof(float));
        std::memset(algorithm->sampleRight, 0, static_cast<std::size_t>(frames) * sizeof(float));
        if (algorithm->streamOpen && NT_globals.workBuffer != nullptr &&
            NT_globals.workBufferSizeBytes >=
                static_cast<uint32_t>(frames) * sizeof(_NT_frame)) {
            _NT_frame* rendered = reinterpret_cast<_NT_frame*>(NT_globals.workBuffer);
            uint32_t count = NT_streamRender(algorithm->stream,
                                             rendered,
                                             static_cast<uint32_t>(frames),
                                             algorithm->sampleSpeed);
            if (count > static_cast<uint32_t>(frames)) {
                count = static_cast<uint32_t>(frames);
            }
            for (uint32_t i = 0; i < count; ++i) {
                algorithm->sampleLeft[i] = rendered[i][0];
                algorithm->sampleRight[i] = rendered[i][1];
            }
        }
        left = algorithm->sampleLeft;
        right = algorithm->sampleRight;
    }

    // Render both channels before touching an output bus. This preserves
    // correct routing when outputs alias inputs.
    algorithm->processor->process(left,
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

bool draw(_NT_algorithm* base) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    char text[48];
    const char* source = algorithm->v[kParamSource] == kSourceSample
        ? "SAMPLE" : "LIVE";
    std::snprintf(text, sizeof(text), "CAPICOLA  %s", source);
    NT_drawText(0, 10, text);

    const int params[2][3] = {
        {kParamStretch, kParamThreshold, kParamFeedback},
        {kParamPitch, kParamGrainSize, kParamQuality},
    };
    const char* labels[2][3] = {
        {"STRETCH", "THRESH", "FEEDBACK"},
        {"PITCH", "GRAIN", "QUALITY"},
    };
    const int row = algorithm->alternateControls ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        NT_drawText(i * 43, 29, labels[row][i], 8);
        std::snprintf(text, sizeof(text), "%ld",
                      static_cast<long>(algorithm->v[params[row][i]]));
        NT_drawText(i * 43, 42, text);
    }
    std::snprintf(text, sizeof(text), "%s  MIX %ld%%",
                  algorithm->alternateControls ? "ALT" : "MAIN",
                  static_cast<long>(algorithm->v[kParamMix]));
    NT_drawText(0, 60, text, 12);
    return true;
}

uint32_t hasCustomUi(_NT_algorithm*) {
    return kNT_potL | kNT_potC | kNT_potR |
           kNT_potButtonL | kNT_potButtonC | kNT_potButtonR |
           kNT_encoderL | kNT_encoderR |
           kNT_encoderButtonL | kNT_encoderButtonR;
}

bool pressed(const _NT_uiData& data, uint16_t control) {
    return (data.controls & control) != 0 && (data.lastButtons & control) == 0;
}

void setFromUi(Algorithm* algorithm, int parameter, int value) {
    const _NT_parameter& definition = algorithm->params[parameter];
    if (value < definition.min) value = definition.min;
    if (value > definition.max) value = definition.max;
    NT_setParameterFromUi(NT_algorithmIndex(algorithm),
                          parameter + NT_parameterOffset(), value);
}

void customUi(_NT_algorithm* base, const _NT_uiData& data) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    if (pressed(data, kNT_potButtonL) || pressed(data, kNT_potButtonC) ||
        pressed(data, kNT_potButtonR)) {
        algorithm->alternateControls = !algorithm->alternateControls;
    }

    const int params[2][3] = {
        {kParamStretch, kParamThreshold, kParamFeedback},
        {kParamPitch, kParamGrainSize, kParamQuality},
    };
    const uint16_t potControls[3] = {kNT_potL, kNT_potC, kNT_potR};
    const int row = algorithm->alternateControls ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        if ((data.controls & potControls[i]) != 0) {
            const _NT_parameter& definition = algorithm->params[params[row][i]];
            const int value = definition.min + static_cast<int>(
                data.pots[i] * static_cast<float>(definition.max - definition.min) + 0.5f);
            setFromUi(algorithm, params[row][i], value);
        }
    }

    if (data.encoders[0] != 0) {
        setFromUi(algorithm, kParamSource,
                  algorithm->v[kParamSource] + data.encoders[0]);
    }
    if (pressed(data, kNT_encoderButtonL) &&
        algorithm->v[kParamSource] == kSourceSample) {
        // Folder/sample browsing remains the host's temporary parameter view;
        // this confirms its displayed sample from the performance screen.
        openSelectedSample(algorithm);
    }
    if (data.encoders[1] != 0) {
        setFromUi(algorithm, kParamMix,
                  algorithm->v[kParamMix] + data.encoders[1]);
    }
    if (pressed(data, kNT_encoderButtonR)) {
        algorithm->processor->triggerSlice();
    }
}

void setupUi(_NT_algorithm* base, _NT_float3& pots) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    const int params[2][3] = {
        {kParamStretch, kParamThreshold, kParamFeedback},
        {kParamPitch, kParamGrainSize, kParamQuality},
    };
    const int row = algorithm->alternateControls ? 1 : 0;
    for (int i = 0; i < 3; ++i) {
        const _NT_parameter& definition = algorithm->params[params[row][i]];
        pots[i] = static_cast<float>(algorithm->v[params[row][i]] - definition.min) /
                  static_cast<float>(definition.max - definition.min);
    }
}

static const _NT_factory kFactory = {
    .guid = NT_MULTICHAR('C', 'a', 'N', 'T'),
    .name = "Capicola",
    .description = "Capicola live/sample stereo processor",
    .numSpecifications = 0,
    .specifications = nullptr,
    .calculateStaticRequirements = nullptr,
    .initialise = nullptr,
    .calculateRequirements = calculateRequirements,
    .construct = construct,
    .parameterChanged = parameterChanged,
    .step = step,
    .draw = draw,
    .midiRealtime = nullptr,
    .midiMessage = nullptr,
    .tags = kNT_tagEffect,
    .hasCustomUi = hasCustomUi,
    .customUi = customUi,
    .setupUi = setupUi,
    .serialise = nullptr,
    .deserialise = nullptr,
    .midiSysEx = nullptr,
    .parameterUiPrefix = nullptr,
    .parameterString = parameterString,
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
