// Capicola for disting NT: independently maintained wrapper integration, 2026.
// SPDX-License-Identifier: AGPL-3.0-only

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
constexpr uint32_t kMaxLoadedSampleFrames = 48000U * 32U;
constexpr float kSamplePlaybackGain = 8.0f;
using SourceProcessor = capicola_nt::CapicolaStereoLivePath<kRingFrames>;

struct SampleLoadSpec {
    uint32_t folder;
    uint32_t sample;
    uint32_t frames;
    uint32_t sampleRate;
    uint32_t generation;
};

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
    kParamEnvelopeSmoothing,
    kParamFade,
    kParamDrive,
    kParamDriveCharacter,
    kParamMix,
    kParamFeedbackTone,
    kParamInputTransientOutput,
    kParamOutputTransientOutput,
    kParamInputEnvelopeOutput,
    kParamOutputEnvelopeOutput,
    kNumParameters,
};

enum SourceMode {
    kSourceLive,
    kSourceSample,
};

enum UiView {
    kUiPerformance,
    kUiFolderSelection,
    kUiSampleSelection,
};

static const char* const kSourceNames[] = {"Live", "Sample"};

static const _NT_parameter kParameterTemplate[] = {
    NT_PARAMETER_AUDIO_INPUT("Left input", 1, 1)
    NT_PARAMETER_AUDIO_INPUT("Right input", 0, 2)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Left output", 1, 13)
    NT_PARAMETER_AUDIO_OUTPUT_WITH_MODE("Right output", 1, 14)
    {.name = "Source", .min = kSourceLive, .max = kSourceSample, .def = kSourceLive,
     .unit = kNT_unitEnum, .scaling = 0, .enumStrings = kSourceNames},
    // These units expose the NT/host folder and sample pickers. Names are
    // supplied by parameterString() from the firmware sample catalogue.
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
    // Secondary controls use a 0.00-100.00% normalized sweep. The conversion
    // below applies Capicola's exact exponential or linear panel taper.
    {.name = "Envelope Smoothing", .min = 0, .max = 10000, .def = 4259,
     .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = nullptr},
    {.name = "Fade", .min = 0, .max = 10000, .def = 2153,
     .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = nullptr},
    {.name = "Drive", .min = 0, .max = 10000, .def = 1429,
     .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = nullptr},
    {.name = "Drive Character", .min = 0, .max = 10000, .def = 10000,
     .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = nullptr},
    {.name = "Mix", .min = 0, .max = 100, .def = 100,
     .unit = kNT_unitPercent, .scaling = 0, .enumStrings = nullptr},
    {.name = "Feedback Tone", .min = 0, .max = 10000, .def = 3769,
     .unit = kNT_unitPercent, .scaling = kNT_scaling100, .enumStrings = nullptr},
    NT_PARAMETER_CV_OUTPUT("Input Transient output", 0, 0)
    NT_PARAMETER_CV_OUTPUT("Output Transient output", 0, 0)
    NT_PARAMETER_CV_OUTPUT("Input Envelope output", 0, 0)
    NT_PARAMETER_CV_OUTPUT("Output Envelope output", 0, 0)
};

static const uint8_t kPerformanceParameters[] = {
    kParamPitch,
    kParamStretch,
    kParamThreshold,
    kParamGrainSize,
    kParamQuality,
    kParamFeedback,
    kParamEnvelopeSmoothing,
    kParamFade,
    kParamDrive,
    kParamDriveCharacter,
    kParamMix,
    kParamFeedbackTone,
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
    kParamInputTransientOutput,
    kParamOutputTransientOutput,
    kParamInputEnvelopeOutput,
    kParamOutputEnvelopeOutput,
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
    _NT_frame* loadedSample;
    float* scratchLeft;
    float* scratchRight;
    float* sampleLeft;
    float* sampleRight;
    _NT_wavRequest sampleRequest;
    SampleLoadSpec loadingSample;
    SampleLoadSpec queuedSample;
    uint32_t selectionGeneration;
    uint32_t loadedSampleFrames;
    uint32_t loadedSampleIndex;
    float loadedSampleFraction;
    float sampleSpeed;
    bool cardMounted;
    bool sampleReady;
    bool sampleLoading;
    bool samplePlaying;
    bool queuedSampleLoad;
    SourceMode activeSource;
    UiView uiView;
    bool alternateControls;
    int16_t appliedControls[ARRAY_SIZE(kPerformanceParameters)];
    bool processingControlsApplied;
    bool processorFaulted;
    float lastOutputLeft;
    float lastOutputRight;
};

void calculateRequirements(_NT_algorithmRequirements& requirements,
                           const int32_t*) {
    requirements.numParameters = kNumParameters;
    requirements.sram = sizeof(Algorithm) + alignof(Algorithm) - 1U;
    requirements.dram = alignof(SourceProcessor) - 1U +
                        sizeof(SourceProcessor) +
                        alignof(float) - 1U +
                        kMaxLoadedSampleFrames * sizeof(_NT_frame) +
                        4U * NT_globals.maxFramesPerStep * sizeof(float);
    requirements.dtc = 0;
    requirements.itc = 0;
}

uint8_t* alignPointer(uint8_t* pointer, std::size_t alignment) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1U);
    return reinterpret_cast<uint8_t*>((address + mask) & ~mask);
}

void resetProcessor(Algorithm* algorithm) {
    algorithm->processor->init();
    algorithm->processingControlsApplied = false;
    algorithm->processorFaulted = false;
    algorithm->lastOutputLeft = 0.0f;
    algorithm->lastOutputRight = 0.0f;
}

_NT_algorithm* construct(const _NT_algorithmMemoryPtrs& memory,
                         const _NT_algorithmRequirements&,
                         const int32_t*) {
    static_assert(ARRAY_SIZE(kParameterTemplate) == kNumParameters);

    Algorithm* algorithm = new (alignPointer(memory.sram, alignof(Algorithm)))
        Algorithm();
    std::memcpy(algorithm->params, kParameterTemplate, sizeof(kParameterTemplate));
    algorithm->parameters = algorithm->params;
    algorithm->parameterPages = &kParameterPages;

    uint8_t* cursor = alignPointer(memory.dram, alignof(SourceProcessor));
    algorithm->processor = new (cursor) SourceProcessor();
    resetProcessor(algorithm);
    cursor += sizeof(SourceProcessor);
    cursor = alignPointer(cursor, alignof(float));
    algorithm->loadedSample = reinterpret_cast<_NT_frame*>(cursor);
    cursor += kMaxLoadedSampleFrames * sizeof(_NT_frame);
    algorithm->scratchLeft = reinterpret_cast<float*>(cursor);
    algorithm->scratchRight = algorithm->scratchLeft + NT_globals.maxFramesPerStep;
    algorithm->sampleLeft = algorithm->scratchRight + NT_globals.maxFramesPerStep;
    algorithm->sampleRight = algorithm->sampleLeft + NT_globals.maxFramesPerStep;

    algorithm->sampleRequest = {};
    algorithm->loadingSample = {};
    algorithm->queuedSample = {};
    algorithm->selectionGeneration = 0;
    algorithm->loadedSampleFrames = 0;
    algorithm->loadedSampleIndex = 0;
    algorithm->loadedSampleFraction = 0.0f;
    algorithm->sampleSpeed = 1.0f;
    algorithm->cardMounted = NT_isSdCardMounted();
    algorithm->sampleReady = false;
    algorithm->sampleLoading = false;
    algorithm->samplePlaying = false;
    algorithm->queuedSampleLoad = false;
    algorithm->activeSource = kSourceLive;
    algorithm->uiView = kUiPerformance;
    algorithm->alternateControls = false;

    const uint32_t folderCount = algorithm->cardMounted
        ? NT_getNumSampleFolders() : 0;
    algorithm->params[kParamFolder].max = folderCount == 0
        ? 0
        : static_cast<int16_t>(folderCount - 1U > 32767U
            ? 32767U : folderCount - 1U);
    _NT_wavFolderInfo folderInfo{};
    if (folderCount != 0) {
        NT_getSampleFolderInfo(0, folderInfo);
    }
    algorithm->params[kParamSample].max = folderInfo.numSampleFiles == 0
        ? 0
        : static_cast<int16_t>(folderInfo.numSampleFiles - 1U > 32767U
            ? 32767U : folderInfo.numSampleFiles - 1U);
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

float secondaryNorm(int32_t value) {
    return static_cast<float>(value) * 0.0001f;
}

float exponentialSweep(float minimum, float maximum, int32_t value) {
    return minimum * std::pow(maximum / minimum, secondaryNorm(value));
}

void applyProcessingControls(Algorithm* algorithm) {
    for (std::size_t i = 0; i < ARRAY_SIZE(kPerformanceParameters); ++i) {
        const int parameter = kPerformanceParameters[i];
        if (algorithm->processingControlsApplied &&
            algorithm->appliedControls[i] == algorithm->v[parameter]) {
            continue;
        }
        switch (parameter) {
            case kParamPitch:
                algorithm->processor->setPitchSemitones(
                    static_cast<float>(algorithm->v[parameter]) * 0.1f);
                break;
            case kParamStretch:
                algorithm->processor->setStretch(
                    stretchValue(algorithm->v[parameter]));
                break;
            case kParamThreshold:
                algorithm->processor->setTransientThreshold(
                    thresholdValue(algorithm->v[parameter]));
                break;
            case kParamGrainSize:
                algorithm->processor->setGrainSize(algorithm->v[parameter]);
                break;
            case kParamQuality:
                algorithm->processor->setQuality(
                    0.1f - static_cast<float>(algorithm->v[parameter]) * 0.00099f);
                break;
            case kParamFeedback:
                algorithm->processor->setFeedback(
                    static_cast<float>(algorithm->v[parameter]) * 0.01f);
                break;
            case kParamEnvelopeSmoothing:
                algorithm->processor->setEnvelopeSmoothing(
                    exponentialSweep(5.0e-5f, 0.125f,
                                     algorithm->v[parameter]));
                break;
            case kParamFade:
                algorithm->processor->setFade(
                    exponentialSweep(480.0f, 12000.0f,
                                     algorithm->v[parameter]));
                break;
            case kParamDrive:
                algorithm->processor->setDrive(
                    0.5f + 3.5f * secondaryNorm(algorithm->v[parameter]));
                break;
            case kParamDriveCharacter:
                algorithm->processor->setDriveCharacter(
                    secondaryNorm(algorithm->v[parameter]));
                break;
            case kParamMix:
                algorithm->processor->setMix(
                    static_cast<float>(algorithm->v[parameter]) * 0.01f);
                break;
            case kParamFeedbackTone:
                algorithm->processor->setFeedbackTone(
                    exponentialSweep(2.0e-3f, 0.9f,
                                     algorithm->v[parameter]));
                break;
            default:
                break;
        }
        algorithm->appliedControls[i] =
            algorithm->v[parameter];
    }
    algorithm->processingControlsApplied = true;
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

void invalidateSamplePlayback(Algorithm* algorithm) {
    ++algorithm->selectionGeneration;
    algorithm->loadedSampleFrames = 0;
    algorithm->loadedSampleIndex = 0;
    algorithm->loadedSampleFraction = 0.0f;
    algorithm->sampleReady = false;
    algorithm->samplePlaying = false;
    algorithm->queuedSampleLoad = false;
}

bool startSampleLoad(Algorithm* algorithm, const SampleLoadSpec& spec);

void sampleLoadCallback(void* callbackData, bool success) {
    Algorithm* algorithm = static_cast<Algorithm*>(callbackData);
    algorithm->sampleLoading = false;

    if (algorithm->queuedSampleLoad) {
        const SampleLoadSpec queued = algorithm->queuedSample;
        algorithm->queuedSampleLoad = false;
        if (queued.generation == algorithm->selectionGeneration &&
            algorithm->activeSource == kSourceSample &&
            algorithm->cardMounted) {
            startSampleLoad(algorithm, queued);
        }
        return;
    }

    const SampleLoadSpec loaded = algorithm->loadingSample;
    if (!success || loaded.generation != algorithm->selectionGeneration ||
        algorithm->activeSource != kSourceSample || !algorithm->cardMounted ||
        loaded.frames == 0 || loaded.sampleRate == 0 ||
        NT_globals.sampleRate == 0) {
        return;
    }

    algorithm->loadedSampleFrames = loaded.frames;
    algorithm->loadedSampleIndex = 0;
    algorithm->loadedSampleFraction = 0.0f;
    algorithm->sampleSpeed = static_cast<float>(loaded.sampleRate) /
                             static_cast<float>(NT_globals.sampleRate);
    algorithm->sampleReady = std::isfinite(algorithm->sampleSpeed) &&
                             algorithm->sampleSpeed > 0.0f;
    algorithm->samplePlaying = algorithm->sampleReady;
}

bool startSampleLoad(Algorithm* algorithm, const SampleLoadSpec& spec) {
    algorithm->loadingSample = spec;
    algorithm->sampleRequest.folder = spec.folder;
    algorithm->sampleRequest.sample = spec.sample;
    algorithm->sampleRequest.dst = algorithm->loadedSample;
    algorithm->sampleRequest.numFrames = spec.frames;
    algorithm->sampleRequest.startOffset = 0;
    algorithm->sampleRequest.channels = kNT_WavStereo;
    algorithm->sampleRequest.bits = kNT_WavBits32;
    algorithm->sampleRequest.progress = kNT_WavProgress;
    algorithm->sampleRequest.callback = sampleLoadCallback;
    algorithm->sampleRequest.callbackData = algorithm;

    // Set this before entering the host so even a synchronous test callback
    // cannot leave the state stuck in "loading" after it returns.
    algorithm->sampleLoading = true;
    if (!NT_readSampleFrames(algorithm->sampleRequest)) {
        algorithm->sampleLoading = false;
        return false;
    }
    return true;
}

// Catalogue discovery and parameter-definition updates may touch the SD card
// and are not audio-rate work. Call this only from host/UI parameter callbacks,
// never from step().
void refreshCatalog(Algorithm* algorithm) {
    const bool mounted = NT_isSdCardMounted();
    if (mounted != algorithm->cardMounted) {
        algorithm->cardMounted = mounted;
        invalidateSamplePlayback(algorithm);
        resetProcessor(algorithm);
    }

    const uint32_t folderCount = mounted ? NT_getNumSampleFolders() : 0;
    algorithm->params[kParamFolder].max = folderCount == 0
        ? 0
        : static_cast<int16_t>(folderCount - 1U > 32767U
            ? 32767U : folderCount - 1U);
    NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamFolder);
    if (mounted) {
        updateSampleRange(algorithm);
    } else {
        algorithm->params[kParamSample].max = 0;
        NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamSample);
    }
}

void loadSelectedSample(Algorithm* algorithm) {
    if (!algorithm->cardMounted || algorithm->activeSource != kSourceSample) {
        return;
    }

    // A new or failed selection replaces the previous sample immediately;
    // never render history retained from another file while the asynchronous
    // read is in flight.
    invalidateSamplePlayback(algorithm);
    resetProcessor(algorithm);

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
    if (info.numFrames == 0 || info.sampleRate == 0 ||
        NT_globals.sampleRate == 0) {
        return;
    }

    const SampleLoadSpec spec = {
        .folder = folder,
        .sample = sample,
        .frames = info.numFrames > kMaxLoadedSampleFrames
            ? kMaxLoadedSampleFrames : info.numFrames,
        .sampleRate = info.sampleRate,
        .generation = algorithm->selectionGeneration,
    };
    if (algorithm->sampleLoading) {
        algorithm->queuedSample = spec;
        algorithm->queuedSampleLoad = true;
        return;
    }
    startSampleLoad(algorithm, spec);
}

void selectSource(Algorithm* algorithm, SourceMode source) {
    if (algorithm->activeSource == source) {
        return;
    }
    // Reset Capicola at the source boundary so no history from the replaced
    // source is rendered after the switch.
    algorithm->activeSource = source;
    resetProcessor(algorithm);
    invalidateSamplePlayback(algorithm);
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
            refreshCatalog(algorithm);
            invalidateSamplePlayback(algorithm);
            if (algorithm->activeSource == kSourceSample) {
                resetProcessor(algorithm);
            }
            break;
        case kParamSample:
            refreshCatalog(algorithm);
            loadSelectedSample(algorithm);
            break;
        case kParamPitch:
        case kParamStretch:
        case kParamThreshold:
        case kParamGrainSize:
        case kParamQuality:
        case kParamFeedback:
        case kParamEnvelopeSmoothing:
        case kParamFade:
        case kParamDrive:
        case kParamDriveCharacter:
        case kParamMix:
        case kParamFeedbackTone:
            applyProcessingControls(algorithm);
            break;
        default:
            break;
    }
}

int parameterString(_NT_algorithm* base, int parameter, int value, char* buffer) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    const char* name = nullptr;
    if (!NT_isSdCardMounted()) {
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

void fadeToSilence(float* left, float* right, int frames,
                   float startLeft, float startRight) {
    const float scale = frames > 0 ? 1.0f / static_cast<float>(frames) : 0.0f;
    for (int i = 0; i < frames; ++i) {
        const float gain = 1.0f - static_cast<float>(i + 1) * scale;
        left[i] = startLeft * gain;
        right[i] = startRight * gain;
    }
}

bool outputsAreFinite(const float* left, const float* right, int frames) {
    for (int i = 0; i < frames; ++i) {
        if (!std::isfinite(left[i]) || !std::isfinite(right[i])) {
            return false;
        }
    }
    return true;
}

void writeAnalysisCv(float* busFrames,
                     int frames,
                     int selectedBus,
                     float voltage) {
    // Zero is the explicit disconnected value. Validate it before converting
    // the host's one-based bus number to a buffer index.
    if (selectedBus < 1 || selectedBus > kNT_lastBus) {
        return;
    }
    float* destination = busFrames + (selectedBus - 1) * frames;
    for (int i = 0; i < frames; ++i) {
        destination[i] = voltage;
    }
}

uint32_t renderLoadedSample(Algorithm* algorithm, int frames) {
    std::memset(algorithm->sampleLeft, 0,
                static_cast<std::size_t>(frames) * sizeof(float));
    std::memset(algorithm->sampleRight, 0,
                static_cast<std::size_t>(frames) * sizeof(float));

    if (!algorithm->sampleReady || !algorithm->samplePlaying ||
        algorithm->loadedSampleFrames == 0 ||
        algorithm->loadedSampleIndex >= algorithm->loadedSampleFrames ||
        !std::isfinite(algorithm->loadedSampleFraction) ||
        !std::isfinite(algorithm->sampleSpeed) ||
        algorithm->loadedSampleFraction < 0.0f ||
        algorithm->loadedSampleFraction >= 1.0f ||
        algorithm->sampleSpeed <= 0.0f) {
        algorithm->samplePlaying = false;
        return 0;
    }

    uint32_t index = algorithm->loadedSampleIndex;
    float fraction = algorithm->loadedSampleFraction;
    uint32_t rendered = 0;
    for (int i = 0;
         i < frames && index < algorithm->loadedSampleFrames;
         ++i) {
        const uint32_t next = index + 1U < algorithm->loadedSampleFrames
            ? index + 1U : index;
        const float left = algorithm->loadedSample[index][0] + fraction *
            (algorithm->loadedSample[next][0] -
             algorithm->loadedSample[index][0]);
        const float right = algorithm->loadedSample[index][1] + fraction *
            (algorithm->loadedSample[next][1] -
             algorithm->loadedSample[index][1]);
        algorithm->sampleLeft[i] = std::isfinite(left)
            ? left * kSamplePlaybackGain : 0.0f;
        algorithm->sampleRight[i] = std::isfinite(right)
            ? right * kSamplePlaybackGain : 0.0f;
        ++rendered;

        const float advance = fraction + algorithm->sampleSpeed;
        const uint32_t remaining = algorithm->loadedSampleFrames - index;
        if (!std::isfinite(advance) ||
            advance >= static_cast<float>(remaining)) {
            index = algorithm->loadedSampleFrames;
            fraction = 0.0f;
            break;
        }
        const uint32_t wholeFrames = static_cast<uint32_t>(advance);
        index += wholeFrames;
        fraction = advance - static_cast<float>(wholeFrames);
    }

    algorithm->loadedSampleIndex = index;
    algorithm->loadedSampleFraction = fraction;
    if (index >= algorithm->loadedSampleFrames) {
        algorithm->samplePlaying = false;
    }
    return rendered;
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
    // Host parameter mapping can update v[] between callbacks; applying the
    // linked controls here keeps UI, mapped CV, live, and sample processing on
    // the same confirmed Capicola control surface.
    applyProcessingControls(algorithm);

    const float* left = nullptr;
    const float* right = nullptr;
    bool sourceAvailable = true;
    if (algorithm->activeSource == kSourceLive) {
        const int leftInputBus = algorithm->v[kParamLeftInput];
        const int rightInputBus = algorithm->v[kParamRightInput];
        const float* rawLeft = busFrames + (leftInputBus - 1) * frames;
        const float* rawRight = rightInputBus == 0
            ? rawLeft
            : busFrames + (rightInputBus - 1) * frames;
        for (int i = 0; i < frames; ++i) {
            algorithm->sampleLeft[i] = std::isfinite(rawLeft[i])
                ? rawLeft[i] : 0.0f;
            algorithm->sampleRight[i] = std::isfinite(rawRight[i])
                ? rawRight[i] : 0.0f;
        }
        left = algorithm->sampleLeft;
        right = algorithm->sampleRight;
    } else {
        const uint32_t count = renderLoadedSample(algorithm, frames);
        sourceAvailable = count != 0;
        left = algorithm->sampleLeft;
        right = algorithm->sampleRight;
    }

    bool rendered = sourceAvailable && !algorithm->processorFaulted;
    if (rendered) {
        // Render both channels before touching an output bus. This preserves
        // correct routing when outputs alias inputs.
        algorithm->processor->process(left,
                                      right,
                                      algorithm->scratchLeft,
                                      algorithm->scratchRight,
                                      static_cast<std::size_t>(frames));
        rendered = outputsAreFinite(algorithm->scratchLeft,
                                    algorithm->scratchRight, frames);
        if (!rendered) {
            algorithm->processorFaulted = true;
        }
    }
    if (!rendered) {
        fadeToSilence(algorithm->scratchLeft, algorithm->scratchRight, frames,
                      algorithm->lastOutputLeft, algorithm->lastOutputRight);
    }
    algorithm->lastOutputLeft = algorithm->scratchLeft[frames - 1];
    algorithm->lastOutputRight = algorithm->scratchRight[frames - 1];

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

    const bool analysisValid = rendered &&
        std::isfinite(algorithm->processor->inputEnvelope()) &&
        std::isfinite(algorithm->processor->outputEnvelope());
    writeAnalysisCv(busFrames, frames,
                    algorithm->v[kParamInputTransientOutput],
                    analysisValid && algorithm->processor->inputTransient()
                        ? 5.0f : 0.0f);
    writeAnalysisCv(busFrames, frames,
                    algorithm->v[kParamOutputTransientOutput],
                    analysisValid && algorithm->processor->outputTransient()
                        ? 5.0f : 0.0f);
    writeAnalysisCv(busFrames, frames,
                    algorithm->v[kParamInputEnvelopeOutput],
                    analysisValid
                        ? 5.0f * algorithm->processor->inputEnvelope() : 0.0f);
    writeAnalysisCv(busFrames, frames,
                    algorithm->v[kParamOutputEnvelopeOutput],
                    analysisValid
                        ? 5.0f * algorithm->processor->outputEnvelope() : 0.0f);
}

void drawSelection(Algorithm* algorithm) {
    char name[kNT_parameterStringSize] = {};
    char text[64];
    const bool folderView = algorithm->uiView == kUiFolderSelection;
    const int parameter = folderView ? kParamFolder : kParamSample;
    if (parameterString(algorithm, parameter, algorithm->v[parameter], name) == 0) {
        std::strncpy(name, NT_isSdCardMounted() ? "Unavailable" : "No SD card",
                     sizeof(name) - 1);
    }
    std::snprintf(text, sizeof(text), "SELECT %s",
                  folderView ? "FOLDER" : "SAMPLE");
    NT_drawText(0, 11, text, 8);
    NT_drawText(0, 34, name, 15, kNT_textLeft, kNT_textLarge);
    NT_drawText(0, 60,
                folderView ? "TURN: CHOOSE   PRESS: NEXT"
                           : "TURN: CHOOSE   PRESS: LOAD",
                10);
}

void formatControlValue(const Algorithm* algorithm, int parameter,
                        char* text, std::size_t size) {
    const long value = static_cast<long>(algorithm->v[parameter]);
    if (parameter == kParamPitch) {
        const long magnitude = value < 0 ? -value : value;
        std::snprintf(text, size, "%c%ld.%ld st",
                      value < 0 ? '-' : '+', magnitude / 10, magnitude % 10);
    } else if (parameter == kParamGrainSize) {
        std::snprintf(text, size, "%ld", value);
    } else {
        std::snprintf(text, size, "%ld%%", value);
    }
}

bool draw(_NT_algorithm* base) {
    Algorithm* algorithm = static_cast<Algorithm*>(base);
    if (algorithm->uiView != kUiPerformance) {
        drawSelection(algorithm);
        return true;
    }

    char text[64];
    const bool sampleMode = algorithm->v[kParamSource] == kSourceSample;
    const char* sourceState = sampleMode
        ? (algorithm->sampleLoading ? "SAMPLE LOAD"
           : algorithm->samplePlaying ? "SAMPLE PLAY" : "SAMPLE WAIT")
        : "LIVE";
    std::snprintf(text, sizeof(text), "CAPICOLA   %s", sourceState);
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
        const int x = i * 85;
        NT_drawText(x, 27, labels[row][i], 8);
        formatControlValue(algorithm, params[row][i], text, sizeof(text));
        NT_drawText(x, 40, text);
    }

    const float rawInputEnvelope = algorithm->processor->inputEnvelope();
    const float rawOutputEnvelope = algorithm->processor->outputEnvelope();
    const float safeInputEnvelope = std::isfinite(rawInputEnvelope)
        ? std::fmin(1.0f, std::fmax(0.0f, rawInputEnvelope)) : 0.0f;
    const float safeOutputEnvelope = std::isfinite(rawOutputEnvelope)
        ? std::fmin(1.0f, std::fmax(0.0f, rawOutputEnvelope)) : 0.0f;
    const long inputEnvelope = static_cast<long>(safeInputEnvelope * 99.0f + 0.5f);
    const long outputEnvelope = static_cast<long>(safeOutputEnvelope * 99.0f + 0.5f);
    std::snprintf(text, sizeof(text), "%s  IN %02ld%c OUT %02ld%c  MIX %ld%%",
                  algorithm->alternateControls ? "ALT" : "MAIN",
                  inputEnvelope,
                  algorithm->processor->inputTransient() ? '!' : ' ',
                  outputEnvelope,
                  algorithm->processor->outputTransient() ? '!' : ' ',
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

    if (algorithm->uiView != kUiPerformance) {
        const int parameter = algorithm->uiView == kUiFolderSelection
            ? kParamFolder : kParamSample;
        if (data.encoders[0] != 0) {
            setFromUi(algorithm, parameter,
                      algorithm->v[parameter] + data.encoders[0]);
        }
        if (pressed(data, kNT_encoderButtonL)) {
            if (algorithm->uiView == kUiFolderSelection) {
                algorithm->uiView = kUiSampleSelection;
            } else {
                loadSelectedSample(algorithm);
                algorithm->uiView = kUiPerformance;
            }
        }
        return;
    }

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
        algorithm->uiView = kUiFolderSelection;
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
    .guid = NT_MULTICHAR('T', 'h', 'C', 'a'),
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
