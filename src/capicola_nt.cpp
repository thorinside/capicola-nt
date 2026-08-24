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
constexpr float kSamplePlaybackGain = 8.0f;
constexpr uint32_t kSampleTransitionRateDivisor = 20U; // 50 ms.
using SourceProcessor = capicola_nt::CapicolaStereoLivePath<kRingFrames>;

struct SampleStreamSpec {
    uint32_t folder;
    uint32_t sample;
    uint32_t frames;
    uint32_t sampleRate;
    char name[kNT_parameterStringSize];
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

enum SampleTransitionPhase {
    kSampleTransitionNone,
    kSampleTransitionFadeOut,
    kSampleTransitionFadeIn,
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
    _NT_stream stream;
    void* streamBuffer;
    _NT_stream pendingStream;
    void* pendingStreamBuffer;
    _NT_frame* streamFrames;
    _NT_frame* pendingStreamFrames;
    float* scratchLeft;
    float* scratchRight;
    float* sampleLeft;
    float* sampleRight;
    SampleStreamSpec streamedSample;
    SampleStreamSpec pendingSample;
    uint32_t streamSourceFrame;
    float streamSourceFraction;
    float sampleSpeed;
    uint32_t pendingSourceFrame;
    float pendingSourceFraction;
    float pendingSampleSpeed;
    uint32_t activePrefetchFrames;
    uint32_t activePrefetchOffset;
    uint32_t pendingPrefetchFrames;
    bool pendingSampleOpen;
    bool pendingSamplePrimed;
    char streamedSampleName[kNT_parameterStringSize];
    bool cardMounted;
    bool sampleReady;
    bool samplePlaying;
    bool synchronizingSampleParameter;
    SourceMode activeSource;
    UiView uiView;
    bool alternateControls;
    int16_t pendingUiValues[kNumParameters];
    uint32_t pendingUiValueMask;
    int16_t appliedControls[ARRAY_SIZE(kPerformanceParameters)];
    bool processingControlsApplied;
    bool processorFaulted;
    float lastOutputLeft;
    float lastOutputRight;
    SampleTransitionPhase sampleTransitionPhase;
    bool sampleTransitionFadeInPending;
    uint32_t sampleTransitionFrame;
    uint32_t sampleTransitionFrames;
    float sampleTransitionStartLeft;
    float sampleTransitionStartRight;
};

void calculateRequirements(_NT_algorithmRequirements& requirements,
                           const int32_t*) {
    requirements.numParameters = kNumParameters;
    requirements.sram = sizeof(Algorithm) + alignof(Algorithm) - 1U;
    requirements.dram = alignof(SourceProcessor) - 1U +
                        sizeof(SourceProcessor) +
                        3U * (alignof(float) - 1U) +
                        2U * NT_globals.streamBufferSizeBytes +
                        2U * NT_globals.maxFramesPerStep * sizeof(_NT_frame) +
                        4U * NT_globals.maxFramesPerStep * sizeof(float);
    requirements.dtc = 2U * (alignof(uint32_t) - 1U +
                             NT_globals.streamSizeBytes);
    requirements.itc = 0;
}

uint8_t* alignPointer(uint8_t* pointer, std::size_t alignment) {
    const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
    const uintptr_t mask = static_cast<uintptr_t>(alignment - 1U);
    return reinterpret_cast<uint8_t*>((address + mask) & ~mask);
}

void resetProcessor(Algorithm* algorithm, bool clearOutputState = true) {
    algorithm->processor->init();
    algorithm->processingControlsApplied = false;
    algorithm->processorFaulted = false;
    if (clearOutputState) {
        algorithm->lastOutputLeft = 0.0f;
        algorithm->lastOutputRight = 0.0f;
        algorithm->sampleTransitionPhase = kSampleTransitionNone;
        algorithm->sampleTransitionFadeInPending = false;
        algorithm->sampleTransitionFrame = 0U;
        algorithm->sampleTransitionStartLeft = 0.0f;
        algorithm->sampleTransitionStartRight = 0.0f;
    }
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
    algorithm->streamBuffer = cursor;
    cursor += NT_globals.streamBufferSizeBytes;
    cursor = alignPointer(cursor, alignof(float));
    algorithm->pendingStreamBuffer = cursor;
    cursor += NT_globals.streamBufferSizeBytes;
    cursor = alignPointer(cursor, alignof(float));
    algorithm->streamFrames = reinterpret_cast<_NT_frame*>(cursor);
    cursor += NT_globals.maxFramesPerStep * sizeof(_NT_frame);
    algorithm->pendingStreamFrames = reinterpret_cast<_NT_frame*>(cursor);
    cursor += NT_globals.maxFramesPerStep * sizeof(_NT_frame);
    algorithm->scratchLeft = reinterpret_cast<float*>(cursor);
    algorithm->scratchRight = algorithm->scratchLeft + NT_globals.maxFramesPerStep;
    algorithm->sampleLeft = algorithm->scratchRight + NT_globals.maxFramesPerStep;
    algorithm->sampleRight = algorithm->sampleLeft + NT_globals.maxFramesPerStep;
    uint8_t* dtcCursor = alignPointer(memory.dtc, alignof(uint32_t));
    algorithm->stream = dtcCursor;
    dtcCursor += NT_globals.streamSizeBytes;
    algorithm->pendingStream = alignPointer(dtcCursor, alignof(uint32_t));

    algorithm->streamedSample = {};
    algorithm->pendingSample = {};
    algorithm->streamSourceFrame = 0U;
    algorithm->streamSourceFraction = 0.0f;
    algorithm->sampleSpeed = 1.0f;
    algorithm->pendingSourceFrame = 0U;
    algorithm->pendingSourceFraction = 0.0f;
    algorithm->pendingSampleSpeed = 1.0f;
    algorithm->activePrefetchFrames = 0U;
    algorithm->activePrefetchOffset = 0U;
    algorithm->pendingPrefetchFrames = 0U;
    algorithm->pendingSampleOpen = false;
    algorithm->pendingSamplePrimed = false;
    algorithm->streamedSampleName[0] = '\0';
    algorithm->cardMounted = NT_isSdCardMounted();
    algorithm->sampleReady = false;
    algorithm->samplePlaying = false;
    algorithm->synchronizingSampleParameter = false;
    algorithm->activeSource = kSourceLive;
    algorithm->uiView = kUiPerformance;
    algorithm->alternateControls = false;
    algorithm->pendingUiValueMask = 0U;
    algorithm->sampleTransitionFrames =
        NT_globals.sampleRate / kSampleTransitionRateDivisor;
    if (algorithm->sampleTransitionFrames == 0U) {
        algorithm->sampleTransitionFrames = 1U;
    }

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
    if (value < 0) value = 0;
    if (value > 100) value = 100;
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

bool updateSampleRange(Algorithm* algorithm) {
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

    if (algorithm->synchronizingSampleParameter) {
        return false;
    }
    int16_t sample = algorithm->v[kParamSample];
    if (sample < algorithm->params[kParamSample].min) {
        sample = algorithm->params[kParamSample].min;
    }
    if (sample > algorithm->params[kParamSample].max) {
        sample = algorithm->params[kParamSample].max;
    }
    if (sample == algorithm->v[kParamSample]) {
        return false;
    }

    // Changing folders can change the legal Sample range. Keep the host value
    // inside the instance-owned definition and guard the synchronous callback
    // generated by the host setter.
    algorithm->synchronizingSampleParameter = true;
    NT_setParameterFromUi(NT_algorithmIndex(algorithm),
                          kParamSample + NT_parameterOffset(), sample);
    algorithm->synchronizingSampleParameter = false;
    return true;
}

void invalidateSamplePlayback(Algorithm* algorithm) {
    algorithm->streamSourceFrame = 0U;
    algorithm->streamSourceFraction = 0.0f;
    algorithm->activePrefetchFrames = 0U;
    algorithm->activePrefetchOffset = 0U;
    algorithm->streamedSampleName[0] = '\0';
    algorithm->sampleReady = false;
    algorithm->samplePlaying = false;
    algorithm->pendingSampleOpen = false;
    algorithm->pendingSamplePrimed = false;
    algorithm->pendingPrefetchFrames = 0U;
}

void beginSampleTransition(Algorithm* algorithm,
                           bool fadeOutFirst,
                           bool fadeInAfter,
                           float oldLeft,
                           float oldRight) {
    algorithm->sampleTransitionFrame = 0U;
    algorithm->sampleTransitionFadeInPending = fadeInAfter;
    algorithm->sampleTransitionStartLeft = std::isfinite(oldLeft)
        ? oldLeft : 0.0f;
    algorithm->sampleTransitionStartRight = std::isfinite(oldRight)
        ? oldRight : 0.0f;
    if (fadeOutFirst) {
        algorithm->sampleTransitionPhase = kSampleTransitionFadeOut;
    } else if (fadeInAfter) {
        algorithm->sampleTransitionPhase = kSampleTransitionFadeIn;
    } else {
        algorithm->sampleTransitionPhase = kSampleTransitionNone;
    }
}

void stopSamplePlayback(Algorithm* algorithm) {
    const bool fadeOutFirst = algorithm->samplePlaying ||
        algorithm->sampleTransitionPhase != kSampleTransitionNone;
    const float oldLeft = algorithm->lastOutputLeft;
    const float oldRight = algorithm->lastOutputRight;
    invalidateSamplePlayback(algorithm);
    resetProcessor(algorithm, false);
    beginSampleTransition(algorithm, fadeOutFirst, false, oldLeft, oldRight);
}

bool openStream(_NT_stream stream,
                void* streamBuffer,
                const SampleStreamSpec& spec) {
    const _NT_streamOpenData data = {
        .streamBuffer = streamBuffer,
        .folder = spec.folder,
        .sample = spec.sample,
        .velocity = 1.0f,
        .startOffset = 0,
        .reverse = false,
        .rrMode = kNT_RRModeSequential,
    };
    return NT_streamOpen(stream, data);
}

bool openSampleStream(Algorithm* algorithm, const SampleStreamSpec& spec) {
    if (!openStream(algorithm->stream, algorithm->streamBuffer, spec)) {
        algorithm->sampleReady = false;
        algorithm->samplePlaying = false;
        return false;
    }
    algorithm->streamedSample = spec;
    algorithm->streamSourceFrame = 0U;
    algorithm->streamSourceFraction = 0.0f;
    algorithm->activePrefetchFrames = 0U;
    algorithm->activePrefetchOffset = 0U;
    algorithm->sampleSpeed = static_cast<float>(spec.sampleRate) /
                             static_cast<float>(NT_globals.sampleRate);
    algorithm->sampleReady = std::isfinite(algorithm->sampleSpeed) &&
                             algorithm->sampleSpeed > 0.0f;
    algorithm->samplePlaying = algorithm->sampleReady;
    std::strncpy(algorithm->streamedSampleName, spec.name,
                 sizeof(algorithm->streamedSampleName) - 1U);
    algorithm->streamedSampleName[
        sizeof(algorithm->streamedSampleName) - 1U] = '\0';
    return algorithm->sampleReady;
}

bool openPendingSampleStream(Algorithm* algorithm,
                             const SampleStreamSpec& spec) {
    algorithm->pendingSampleOpen = false;
    algorithm->pendingSamplePrimed = false;
    algorithm->pendingPrefetchFrames = 0U;
    if (!openStream(algorithm->pendingStream,
                    algorithm->pendingStreamBuffer, spec)) {
        return false;
    }
    const float speed = static_cast<float>(spec.sampleRate) /
                        static_cast<float>(NT_globals.sampleRate);
    if (!std::isfinite(speed) || speed <= 0.0f) {
        return false;
    }
    algorithm->pendingSample = spec;
    algorithm->pendingSourceFrame = 0U;
    algorithm->pendingSourceFraction = 0.0f;
    algorithm->pendingSampleSpeed = speed;
    algorithm->pendingSampleOpen = true;
    return true;
}

bool advanceStreamPosition(uint32_t& sourceFrame,
                           float& sourceFraction,
                           uint32_t sampleFrames,
                           uint32_t rendered,
                           float speed) {
    const float advance = sourceFraction +
        static_cast<float>(rendered) * speed;
    if (!std::isfinite(advance) ||
        advance >= static_cast<float>(UINT32_MAX)) {
        return false;
    }
    const uint32_t wholeFrames = static_cast<uint32_t>(advance);
    sourceFraction = advance - static_cast<float>(wholeFrames);
    const uint32_t remaining = sampleFrames - sourceFrame;
    sourceFrame += wholeFrames < remaining ? wholeFrames : remaining;
    return true;
}

bool primePendingSampleStream(Algorithm* algorithm, uint32_t frames) {
    if (!algorithm->pendingSampleOpen || algorithm->pendingSamplePrimed ||
        frames == 0U || frames > NT_globals.maxFramesPerStep) {
        return algorithm->pendingSamplePrimed;
    }
    const uint32_t rendered = NT_streamRender(
        algorithm->pendingStream, algorithm->pendingStreamFrames,
        frames, algorithm->pendingSampleSpeed);
    const uint32_t bounded = rendered > frames ? frames : rendered;
    if (bounded == 0U ||
        !advanceStreamPosition(algorithm->pendingSourceFrame,
                               algorithm->pendingSourceFraction,
                               algorithm->pendingSample.frames,
                               bounded,
                               algorithm->pendingSampleSpeed)) {
        return false;
    }
    algorithm->pendingPrefetchFrames = bounded;
    algorithm->pendingSamplePrimed = true;
    return true;
}

void activatePendingSampleStream(Algorithm* algorithm) {
    if (!algorithm->pendingSamplePrimed) return;

    _NT_stream oldStream = algorithm->stream;
    algorithm->stream = algorithm->pendingStream;
    algorithm->pendingStream = oldStream;
    void* oldStreamBuffer = algorithm->streamBuffer;
    algorithm->streamBuffer = algorithm->pendingStreamBuffer;
    algorithm->pendingStreamBuffer = oldStreamBuffer;

    algorithm->streamedSample = algorithm->pendingSample;
    algorithm->streamSourceFrame = algorithm->pendingSourceFrame;
    algorithm->streamSourceFraction = algorithm->pendingSourceFraction;
    algorithm->sampleSpeed = algorithm->pendingSampleSpeed;
    algorithm->activePrefetchFrames = algorithm->pendingPrefetchFrames;
    algorithm->activePrefetchOffset = 0U;
    std::memcpy(algorithm->streamFrames, algorithm->pendingStreamFrames,
                static_cast<std::size_t>(algorithm->activePrefetchFrames) *
                    sizeof(_NT_frame));
    std::strncpy(algorithm->streamedSampleName,
                 algorithm->pendingSample.name,
                 sizeof(algorithm->streamedSampleName) - 1U);
    algorithm->streamedSampleName[
        sizeof(algorithm->streamedSampleName) - 1U] = '\0';
    algorithm->sampleReady = true;
    algorithm->samplePlaying = true;

    algorithm->pendingSampleOpen = false;
    algorithm->pendingSamplePrimed = false;
    algorithm->pendingPrefetchFrames = 0U;
    algorithm->sampleTransitionFrame = 0U;
    algorithm->sampleTransitionPhase = kSampleTransitionFadeIn;
    algorithm->sampleTransitionFadeInPending = false;
}

// Catalogue discovery and parameter-definition updates may touch the SD card
// and are not audio-rate work. Call this only from host/UI parameter callbacks,
// never from step().
bool refreshCatalog(Algorithm* algorithm) {
    const bool mounted = NT_isSdCardMounted();
    if (mounted != algorithm->cardMounted) {
        algorithm->cardMounted = mounted;
        stopSamplePlayback(algorithm);
    }

    const uint32_t folderCount = mounted ? NT_getNumSampleFolders() : 0;
    algorithm->params[kParamFolder].max = folderCount == 0
        ? 0
        : static_cast<int16_t>(folderCount - 1U > 32767U
            ? 32767U : folderCount - 1U);
    NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamFolder);
    if (mounted) {
        return updateSampleRange(algorithm);
    } else {
        algorithm->params[kParamSample].max = 0;
        NT_updateParameterDefinition(NT_algorithmIndex(algorithm), kParamSample);
    }
    return false;
}

void openSelectedSample(Algorithm* algorithm, bool forceReload = false) {
    if (!algorithm->cardMounted || algorithm->activeSource != kSourceSample) {
        return;
    }

    const uint32_t folderCount = NT_getNumSampleFolders();
    uint32_t folder = 0;
    if (!catalogIndex(algorithm->v[kParamFolder], folderCount, folder)) {
        stopSamplePlayback(algorithm);
        return;
    }
    _NT_wavFolderInfo folderInfo{};
    NT_getSampleFolderInfo(folder, folderInfo);
    uint32_t sample = 0;
    if (!catalogIndex(algorithm->v[kParamSample],
                      folderInfo.numSampleFiles,
                      sample)) {
        stopSamplePlayback(algorithm);
        return;
    }
    _NT_wavInfo info{};
    NT_getSampleFileInfo(folder, sample, info);
    if (info.numFrames == 0 || info.sampleRate == 0 ||
        NT_globals.sampleRate == 0) {
        stopSamplePlayback(algorithm);
        return;
    }

    const bool sameActiveStream =
        algorithm->streamedSample.folder == folder &&
        algorithm->streamedSample.sample == sample;
    const bool samePendingStream = algorithm->pendingSampleOpen &&
        algorithm->pendingSample.folder == folder &&
        algorithm->pendingSample.sample == sample;
    if (!forceReload && sameActiveStream && algorithm->sampleReady) {
        algorithm->pendingSampleOpen = false;
        algorithm->pendingSamplePrimed = false;
        algorithm->pendingPrefetchFrames = 0U;
        if (algorithm->sampleTransitionPhase == kSampleTransitionFadeOut &&
            algorithm->sampleTransitionFadeInPending) {
            const uint32_t fadeOutFrame = algorithm->sampleTransitionFrame;
            algorithm->sampleTransitionPhase = kSampleTransitionFadeIn;
            algorithm->sampleTransitionFadeInPending = false;
            algorithm->sampleTransitionFrame =
                algorithm->sampleTransitionFrames - fadeOutFrame - 1U;
        }
        return;
    }
    if (!forceReload && samePendingStream) return;

    SampleStreamSpec spec{};
    spec.folder = folder;
    spec.sample = sample;
    spec.frames = info.numFrames;
    spec.sampleRate = info.sampleRate;
    std::strncpy(spec.name, info.name == nullptr ? "Sample" : info.name,
                 sizeof(spec.name) - 1U);
    spec.name[sizeof(spec.name) - 1U] = '\0';

    // Keep the currently audible stream and warm processor intact. The audio
    // callback first asks the separately opened stream for real frames; only
    // after that succeeds does the old stream begin fading toward the exact
    // midpoint handoff.
    const bool hasActiveStream = algorithm->samplePlaying &&
        algorithm->sampleReady && !algorithm->processorFaulted;
    if (hasActiveStream) {
        if (!openPendingSampleStream(algorithm, spec)) {
            stopSamplePlayback(algorithm);
        }
        return;
    }

    invalidateSamplePlayback(algorithm);
    resetProcessor(algorithm, false);
    const bool opened = openSampleStream(algorithm, spec);
    if (!opened) {
        resetProcessor(algorithm, false);
    }
    beginSampleTransition(algorithm, false, opened, 0.0f, 0.0f);
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
    if (parameter >= 0 && parameter < kNumParameters) {
        algorithm->pendingUiValueMask &=
            ~(uint32_t{1} << static_cast<uint32_t>(parameter));
    }
    switch (parameter) {
        case kParamSource:
        {
            const SourceMode source =
                algorithm->v[kParamSource] == kSourceSample
                    ? kSourceSample : kSourceLive;
            selectSource(algorithm, source);
            if (source == kSourceSample) {
                const bool sampleValueSynchronized = refreshCatalog(algorithm);
                if (!sampleValueSynchronized) {
                    openSelectedSample(algorithm);
                }
            }
            break;
        }
        case kParamFolder:
        {
            const bool sampleValueSynchronized = refreshCatalog(algorithm);
            if (algorithm->activeSource == kSourceSample &&
                !sampleValueSynchronized) {
                openSelectedSample(algorithm);
            } else if (algorithm->activeSource != kSourceSample) {
                invalidateSamplePlayback(algorithm);
                resetProcessor(algorithm);
            }
            break;
        }
        case kParamSample:
            if (!refreshCatalog(algorithm)) {
                // Preset restoration can notify Source, Folder, and Sample in
                // sequence after all values are already present in v[]. The
                // Source callback may therefore have opened this exact file.
                // Deduplicate ordinary host notifications; the sample
                // selector's explicit LOAD action retains its forced-reload
                // path below.
                openSelectedSample(algorithm);
            }
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

uint32_t renderStreamedSample(Algorithm* algorithm,
                              int outputOffset,
                              int frames,
                              bool& restartedThisBlock) {
    std::memset(algorithm->sampleLeft + outputOffset, 0,
                static_cast<std::size_t>(frames) * sizeof(float));
    std::memset(algorithm->sampleRight + outputOffset, 0,
                static_cast<std::size_t>(frames) * sizeof(float));

    if (!algorithm->sampleReady || !algorithm->samplePlaying ||
        algorithm->streamedSample.frames == 0 ||
        algorithm->streamSourceFrame > algorithm->streamedSample.frames ||
        !std::isfinite(algorithm->streamSourceFraction) ||
        !std::isfinite(algorithm->sampleSpeed) ||
        algorithm->streamSourceFraction < 0.0f ||
        algorithm->streamSourceFraction >= 1.0f ||
        algorithm->sampleSpeed <= 0.0f) {
        algorithm->samplePlaying = false;
        return 0;
    }

    uint32_t total = 0U;
    if (algorithm->activePrefetchOffset < algorithm->activePrefetchFrames) {
        const uint32_t available = algorithm->activePrefetchFrames -
            algorithm->activePrefetchOffset;
        const uint32_t copied = available < static_cast<uint32_t>(frames)
            ? available : static_cast<uint32_t>(frames);
        for (uint32_t i = 0U; i < copied; ++i) {
            const _NT_frame& frame = algorithm->streamFrames[
                algorithm->activePrefetchOffset + i];
            algorithm->sampleLeft[outputOffset + total + i] =
                std::isfinite(frame[0]) ? frame[0] * kSamplePlaybackGain : 0.0f;
            algorithm->sampleRight[outputOffset + total + i] =
                std::isfinite(frame[1]) ? frame[1] * kSamplePlaybackGain : 0.0f;
        }
        algorithm->activePrefetchOffset += copied;
        total += copied;
        if (algorithm->activePrefetchOffset >=
            algorithm->activePrefetchFrames) {
            algorithm->activePrefetchFrames = 0U;
            algorithm->activePrefetchOffset = 0U;
        }
    }

    while (total < static_cast<uint32_t>(frames)) {
        if (algorithm->streamSourceFrame >= algorithm->streamedSample.frames) {
            if (restartedThisBlock) break;
            const SampleStreamSpec loop = algorithm->streamedSample;
            if (!openSampleStream(algorithm, loop)) break;
            restartedThisBlock = true;
        }

        const uint32_t requested = static_cast<uint32_t>(frames) - total;
        const uint32_t sourceFrameBefore = algorithm->streamSourceFrame;
        const float sourceFractionBefore = algorithm->streamSourceFraction;
        const uint32_t rendered = NT_streamRender(
            algorithm->stream, algorithm->streamFrames,
            requested, algorithm->sampleSpeed);
        const uint32_t bounded = rendered > requested ? requested : rendered;
        for (uint32_t i = 0U; i < bounded; ++i) {
            const float left = algorithm->streamFrames[i][0];
            const float right = algorithm->streamFrames[i][1];
            algorithm->sampleLeft[outputOffset + total + i] = std::isfinite(left)
                ? left * kSamplePlaybackGain : 0.0f;
            algorithm->sampleRight[outputOffset + total + i] = std::isfinite(right)
                ? right * kSamplePlaybackGain : 0.0f;
        }
        total += bounded;
        if (!advanceStreamPosition(algorithm->streamSourceFrame,
                                   algorithm->streamSourceFraction,
                                   algorithm->streamedSample.frames,
                                   bounded,
                                   algorithm->sampleSpeed)) {
            algorithm->samplePlaying = false;
            break;
        }

        if (bounded == requested) break;
        const float requestedAdvance = sourceFractionBefore +
            static_cast<float>(requested) * algorithm->sampleSpeed;
        const bool expectedEnd = std::isfinite(requestedAdvance) &&
            requestedAdvance >= static_cast<float>(
                algorithm->streamedSample.frames - sourceFrameBefore);
        if (!expectedEnd || restartedThisBlock) break;

        const SampleStreamSpec loop = algorithm->streamedSample;
        if (!openSampleStream(algorithm, loop)) break;
        restartedThisBlock = true;
    }
    return total;
}

void renderStoppedSampleFadeOut(Algorithm* algorithm, int frames) {
    std::memset(algorithm->scratchLeft, 0,
                static_cast<std::size_t>(frames) * sizeof(float));
    std::memset(algorithm->scratchRight, 0,
                static_cast<std::size_t>(frames) * sizeof(float));
    const float reciprocal = 1.0f /
        static_cast<float>(algorithm->sampleTransitionFrames);
    int i = 0;
    for (; i < frames &&
           algorithm->sampleTransitionFrame < algorithm->sampleTransitionFrames;
         ++i, ++algorithm->sampleTransitionFrame) {
        const uint32_t remaining = algorithm->sampleTransitionFrames -
            algorithm->sampleTransitionFrame - 1U;
        const float gain = static_cast<float>(remaining) * reciprocal;
        algorithm->scratchLeft[i] = algorithm->sampleTransitionStartLeft * gain;
        algorithm->scratchRight[i] = algorithm->sampleTransitionStartRight * gain;
    }
    if (algorithm->sampleTransitionFrame >=
        algorithm->sampleTransitionFrames) {
        algorithm->sampleTransitionFrame = 0U;
        algorithm->sampleTransitionPhase =
            algorithm->sampleTransitionFadeInPending
                ? kSampleTransitionFadeIn : kSampleTransitionNone;
        algorithm->sampleTransitionFadeInPending = false;
    }
}

bool processSampleSegment(Algorithm* algorithm,
                          int offset,
                          int frames,
                          bool& restartedThisBlock) {
    const uint32_t count = renderStreamedSample(
        algorithm, offset, frames, restartedThisBlock);
    if (count == 0U || algorithm->processorFaulted) {
        std::memset(algorithm->scratchLeft + offset, 0,
                    static_cast<std::size_t>(frames) * sizeof(float));
        std::memset(algorithm->scratchRight + offset, 0,
                    static_cast<std::size_t>(frames) * sizeof(float));
        return false;
    }
    algorithm->processor->process(algorithm->sampleLeft + offset,
                                  algorithm->sampleRight + offset,
                                  algorithm->scratchLeft + offset,
                                  algorithm->scratchRight + offset,
                                  static_cast<std::size_t>(frames));
    if (!outputsAreFinite(algorithm->scratchLeft + offset,
                          algorithm->scratchRight + offset, frames)) {
        algorithm->processorFaulted = true;
        return false;
    }
    return true;
}

void applySampleTransitionGain(Algorithm* algorithm,
                               int offset,
                               int frames) {
    const float reciprocal = 1.0f /
        static_cast<float>(algorithm->sampleTransitionFrames);
    for (int i = 0; i < frames; ++i) {
        float gain = 1.0f;
        if (algorithm->sampleTransitionPhase == kSampleTransitionFadeOut) {
            const uint32_t remaining = algorithm->sampleTransitionFrames -
                algorithm->sampleTransitionFrame - 1U;
            gain = static_cast<float>(remaining) * reciprocal;
            ++algorithm->sampleTransitionFrame;
            if (algorithm->sampleTransitionFrame >=
                algorithm->sampleTransitionFrames) {
                algorithm->sampleTransitionFrame = 0U;
                if (algorithm->sampleTransitionFadeInPending &&
                    algorithm->pendingSamplePrimed) {
                    activatePendingSampleStream(algorithm);
                } else {
                    algorithm->sampleTransitionPhase = kSampleTransitionNone;
                    algorithm->sampleTransitionFadeInPending = false;
                }
            }
        } else if (algorithm->sampleTransitionPhase ==
                   kSampleTransitionFadeIn) {
            gain = static_cast<float>(
                algorithm->sampleTransitionFrame + 1U) * reciprocal;
            ++algorithm->sampleTransitionFrame;
            if (algorithm->sampleTransitionFrame >=
                algorithm->sampleTransitionFrames) {
                algorithm->sampleTransitionFrame = 0U;
                algorithm->sampleTransitionPhase = kSampleTransitionNone;
            }
        } else {
            break;
        }
        algorithm->scratchLeft[offset + i] *= gain;
        algorithm->scratchRight[offset + i] *= gain;
    }
}

bool processStreamedSampleBlock(Algorithm* algorithm, int frames) {
    bool restartedThisBlock = false;
    bool allSegmentsRendered = true;
    int offset = 0;
    while (offset < frames) {
        int segmentFrames = frames - offset;
        if (algorithm->sampleTransitionPhase == kSampleTransitionFadeOut) {
            if (!algorithm->pendingSamplePrimed) {
                const bool rendered = processSampleSegment(
                    algorithm, offset, segmentFrames, restartedThisBlock);
                const float gain = static_cast<float>(
                    algorithm->sampleTransitionFrames -
                    algorithm->sampleTransitionFrame) /
                    static_cast<float>(algorithm->sampleTransitionFrames);
                for (int i = 0; i < segmentFrames; ++i) {
                    algorithm->scratchLeft[offset + i] *= gain;
                    algorithm->scratchRight[offset + i] *= gain;
                }
                return rendered;
            }
            const uint32_t untilMidpoint =
                algorithm->sampleTransitionFrames -
                algorithm->sampleTransitionFrame;
            if (static_cast<uint32_t>(segmentFrames) > untilMidpoint) {
                segmentFrames = static_cast<int>(untilMidpoint);
            }
        }

        const bool rendered = processSampleSegment(
            algorithm, offset, segmentFrames, restartedThisBlock);
        allSegmentsRendered = allSegmentsRendered && rendered;
        applySampleTransitionGain(algorithm, offset, segmentFrames);
        offset += segmentFrames;
    }
    return allSegmentsRendered;
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
    if (algorithm->activeSource == kSourceSample &&
        algorithm->pendingSampleOpen &&
        !algorithm->pendingSamplePrimed) {
        primePendingSampleStream(algorithm, static_cast<uint32_t>(frames));
    }
    if (algorithm->activeSource == kSourceSample &&
        algorithm->pendingSamplePrimed &&
        algorithm->sampleTransitionPhase == kSampleTransitionNone) {
        beginSampleTransition(algorithm, true, true,
                              algorithm->lastOutputLeft,
                              algorithm->lastOutputRight);
    }

    const bool stoppedSampleFadingOut =
        algorithm->activeSource == kSourceSample &&
        algorithm->sampleTransitionPhase == kSampleTransitionFadeOut &&
        !algorithm->sampleTransitionFadeInPending;
    const float* left = nullptr;
    const float* right = nullptr;
    bool sourceAvailable = !stoppedSampleFadingOut;
    bool sampleBlockProcessed = false;
    bool sampleBlockRendered = false;
    if (stoppedSampleFadingOut) {
        // A missing or refused replacement has no stream to render. Retain a
        // bounded fade of the last valid output instead of cutting it off.
    } else if (algorithm->activeSource == kSourceLive) {
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
        sampleBlockProcessed = true;
        sampleBlockRendered = processStreamedSampleBlock(algorithm, frames);
    }

    bool rendered = sampleBlockProcessed
        ? sampleBlockRendered
        : sourceAvailable && !algorithm->processorFaulted;
    if (rendered && !sampleBlockProcessed) {
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
    if (stoppedSampleFadingOut) {
        renderStoppedSampleFadeOut(algorithm, frames);
        rendered = false;
    } else if (!rendered) {
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

template <std::size_t N>
void copyLiteral(char* destination, std::size_t size, const char (&source)[N]) {
    if (size == 0) return;
    const std::size_t count = N < size ? N : size;
    std::memcpy(destination, source, count);
    if (count == size) destination[size - 1U] = '\0';
}

void compactSampleName(const char* filename, char* destination,
                       std::size_t size) {
    constexpr std::size_t kVisibleCharacters = 32U;
    constexpr std::size_t kLeadingCharacters = 16U;
    constexpr std::size_t kTrailingCharacters = 13U;
    if (size == 0U) return;
    destination[0] = '\0';
    if (filename == nullptr || filename[0] == '\0') {
        copyLiteral(destination, size, "SAMPLE");
        return;
    }

    std::size_t stemLength = std::strlen(filename);
    static const char* const kAudioExtensions[] = {
        "wav", "wave", "aif", "aiff", "flac", "mp3",
    };
    bool stripped = true;
    while (stripped) {
        stripped = false;
        for (const char* extension : kAudioExtensions) {
            const std::size_t extensionLength = std::strlen(extension);
            if (stemLength <= extensionLength + 1U ||
                filename[stemLength - extensionLength - 1U] != '.') {
                continue;
            }
            bool matches = true;
            for (std::size_t i = 0U; i < extensionLength; ++i) {
                char character = filename[
                    stemLength - extensionLength + i];
                if (character >= 'A' && character <= 'Z') {
                    character = static_cast<char>(character - 'A' + 'a');
                }
                if (character != extension[i]) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                stemLength -= extensionLength + 1U;
                stripped = true;
                break;
            }
        }
    }

    char compact[kVisibleCharacters + 1U] = {};
    if (stemLength <= kVisibleCharacters) {
        std::memcpy(compact, filename, stemLength);
        compact[stemLength] = '\0';
    } else {
        std::memcpy(compact, filename, kLeadingCharacters);
        compact[kLeadingCharacters] = '.';
        compact[kLeadingCharacters + 1U] = '.';
        compact[kLeadingCharacters + 2U] = '.';
        std::memcpy(compact + kLeadingCharacters + 3U,
                    filename + stemLength - kTrailingCharacters,
                    kTrailingCharacters);
        compact[kVisibleCharacters] = '\0';
    }

    const std::size_t compactLength = std::strlen(compact);
    const std::size_t copyLength = compactLength < size - 1U
        ? compactLength : size - 1U;
    std::memcpy(destination, compact, copyLength);
    destination[copyLength] = '\0';
}

int32_t displayedValue(const Algorithm* algorithm, int parameter) {
    const uint32_t bit = uint32_t{1} << static_cast<uint32_t>(parameter);
    return (algorithm->pendingUiValueMask & bit) != 0U
        ? algorithm->pendingUiValues[parameter]
        : algorithm->v[parameter];
}

void formatControlValue(const Algorithm* algorithm, int parameter,
                        char* text, std::size_t size) {
    const long value = static_cast<long>(displayedValue(algorithm, parameter));
    if (parameter == kParamPitch) {
        const long magnitude = value < 0 ? -value : value;
        std::snprintf(text, size, "%c%ld.%ld st",
                      value < 0 ? '-' : '+', magnitude / 10, magnitude % 10);
    } else if (parameter == kParamGrainSize) {
        std::snprintf(text, size, "%ld", value);
    } else if (parameter == kParamStretch) {
        if (value >= 100) {
            copyLiteral(text, size, "FREEZE");
        } else {
            const float factor = 1.0f / stretchValue(value);
            if (factor >= 999.5f) {
                copyLiteral(text, size, "999x+");
            } else if (factor < 10.0f) {
                const long tenths = static_cast<long>(factor * 10.0f + 0.5f);
                std::snprintf(text, size, "%ld.%ldx",
                              tenths / 10L, tenths % 10L);
            } else {
                std::snprintf(text, size, "%ldx",
                              static_cast<long>(factor + 0.5f));
            }
        }
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
    if (!sampleMode) {
        copyLiteral(text, sizeof(text), "CAPICOLA   LIVE");
        NT_drawText(0, 10, text);
    } else if (algorithm->samplePlaying &&
               algorithm->streamedSampleName[0] != '\0') {
        NT_drawText(0, 10, "CAPICOLA");
        char sample[33] = {};
        compactSampleName(algorithm->streamedSampleName,
                          sample, sizeof(sample));
        NT_drawText(72, 10, sample, 15, kNT_textLeft, kNT_textTiny);
    } else {
        copyLiteral(text, sizeof(text), "CAPICOLA   WAIT");
        NT_drawText(0, 10, text);
    }

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

    std::snprintf(text, sizeof(text), "MIX %ld%%",
                  static_cast<long>(displayedValue(algorithm, kParamMix)));
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
    algorithm->pendingUiValues[parameter] = static_cast<int16_t>(value);
    algorithm->pendingUiValueMask |=
        uint32_t{1} << static_cast<uint32_t>(parameter);
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
                openSelectedSample(algorithm, true);
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
