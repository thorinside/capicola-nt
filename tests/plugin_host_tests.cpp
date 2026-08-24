#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <distingnt/api.h>
#include <distingnt/wav.h>

namespace {

constexpr int kFrames = 64;
constexpr float kPi = 3.14159265358979323846f;
float gWorkBuffer[kFrames * 2] = {};
bool gCardMounted = true;
uint32_t gNumFolders = 2;
uint32_t gDrumsSampleCount = 2;
bool gSampleMetadataSupported = true;
bool gStreamOpenSucceeds = true;
uint32_t gStreamRenderCalls = 0;
uint32_t gStreamOpenCalls = 0;
uint32_t gFolderInfoCalls = 0;
uint32_t gFileInfoCalls = 0;
uint32_t gCardMountChecks = 0;
uint32_t gParameterDefinitionUpdates = 0;
bool gInvalidCatalogLookup = false;
uint32_t gStreamClock = 0;
uint32_t gOpenedFolder = 0;
uint32_t gOpenedSample = 0;
float gOpenedSpeed = 0.0f;
_NT_algorithm* gAlgorithm = nullptr;
const _NT_factory* gFactory = nullptr;
std::vector<std::string> gDrawnText;

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

bool pageContains(const _NT_parameterPages* pages, const char* pageName, int parameter) {
    if (pages == nullptr) return false;
    for (uint32_t page = 0; page < pages->numPages; ++page) {
        const _NT_parameterPage& candidate = pages->pages[page];
        if (std::strcmp(candidate.name, pageName) != 0) continue;
        for (uint32_t i = 0; i < candidate.numParams; ++i) {
            if (candidate.params[i] == parameter) return true;
        }
    }
    return false;
}

bool drawnTextContains(const char* fragment) {
    for (const std::string& text : gDrawnText) {
        if (text.find(fragment) != std::string::npos) return true;
    }
    return false;
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
    ++gCardMountChecks;
    return gCardMounted;
}

extern "C" uint32_t NT_getNumSampleFolders() {
    return gNumFolders;
}

extern "C" void NT_getSampleFolderInfo(uint32_t folder, _NT_wavFolderInfo& info) {
    ++gFolderInfoCalls;
    if (folder >= gNumFolders || folder >= 2) {
        gInvalidCatalogLookup = true;
        return;
    }
    static const char* names[] = {"Drums", "Textures"};
    info.name = names[folder];
    info.numSampleFiles = folder == 0 ? gDrumsSampleCount : 1;
}

extern "C" void NT_getSampleFileInfo(uint32_t folder,
                                       uint32_t sample,
                                       _NT_wavInfo& info) {
    ++gFileInfoCalls;
    if (folder >= gNumFolders || folder >= 2 ||
        sample >= (folder == 0 ? gDrumsSampleCount : 1U)) {
        gInvalidCatalogLookup = true;
        return;
    }
    static const char* names[] = {"Mono.wav", "Stereo.wav", "Cloud.wav"};
    info.name = folder == 0 ? names[sample] : names[2];
    info.numFrames = 48000;
    info.sampleRate = gSampleMetadataSupported
        ? (folder == 1 ? 24000U : 48000U) : 0U;
    info.channels = sample == 0 ? kNT_WavMono : kNT_WavStereo;
    info.bits = kNT_WavBits16;
}

extern "C" bool NT_streamOpen(_NT_stream, const _NT_streamOpenData& data) {
    ++gStreamOpenCalls;
    gOpenedFolder = data.folder;
    gOpenedSample = data.sample;
    gStreamClock = 0;
    return gStreamOpenSucceeds;
}

extern "C" uint32_t NT_streamRender(_NT_stream,
                                      _NT_frame* renderBuffer,
                                      uint32_t numFrames,
                                      float speed) {
    ++gStreamRenderCalls;
    gOpenedSpeed = speed;
    if (!gCardMounted) {
        return 0;
    }
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

extern "C" void NT_updateParameterDefinition(uint32_t, uint32_t) {
    ++gParameterDefinitionUpdates;
}

extern "C" uint32_t NT_parameterOffset() {
    return 0;
}

extern "C" void NT_setParameterFromUi(uint32_t, uint32_t parameter, int16_t value) {
    if (gAlgorithm != nullptr && gFactory != nullptr) {
        const_cast<int16_t*>(gAlgorithm->v)[parameter] = value;
        gFactory->parameterChanged(gAlgorithm, static_cast<int>(parameter));
    }
}

void NT_drawText(int, int, const char* text, int, _NT_textAlignment, _NT_textSize) {
    gDrawnText.emplace_back(text == nullptr ? "" : text);
}

int main() {
    const auto* factory = reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0));
    gFactory = factory;
    if (factory == nullptr || pluginEntry(kNT_selector_version, 0) != kNT_apiVersion13) {
        return fail("API v13 factory is unavailable");
    }
    if (factory->guid != NT_MULTICHAR('T', 'h', 'C', 'a')) {
        return fail("factory GUID is not the stable ThCa release identity");
    }

    _NT_algorithmRequirements requirements{};
    factory->calculateRequirements(requirements, nullptr);
    // The API exposes byte pointers, so deliberately offset each allocation.
    // Construction must align every typed object within the requested budget.
    std::vector<uint8_t> sram(requirements.sram + 1U);
    std::vector<uint8_t> dram(requirements.dram + 1U);
    std::vector<uint8_t> dtc(requirements.dtc + 1U);
    _NT_algorithmMemoryPtrs memory{
        sram.data() + 1,
        dram.data() + 1,
        dtc.data() + 1,
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
    gAlgorithm = algorithm;
    const int sample = findParameter(algorithm, count, "Sample");
    const int pitch = findParameter(algorithm, count, "Pitch");
    const int stretch = findParameter(algorithm, count, "Stretch");
    const int threshold = findParameter(algorithm, count, "Threshold");
    const int grain = findParameter(algorithm, count, "Grain Size");
    const int quality = findParameter(algorithm, count, "Quality");
    const int feedback = findParameter(algorithm, count, "Feedback");
    const int envelopeSmoothing = findParameter(algorithm, count, "Envelope Smoothing");
    const int fade = findParameter(algorithm, count, "Fade");
    const int drive = findParameter(algorithm, count, "Drive");
    const int driveCharacter = findParameter(algorithm, count, "Drive Character");
    const int mix = findParameter(algorithm, count, "Mix");
    const int feedbackTone = findParameter(algorithm, count, "Feedback Tone");
    const int inputTransientOutput = findParameter(
        algorithm, count, "Input Transient output");
    const int outputTransientOutput = findParameter(
        algorithm, count, "Output Transient output");
    const int inputEnvelopeOutput = findParameter(
        algorithm, count, "Input Envelope output");
    const int outputEnvelopeOutput = findParameter(
        algorithm, count, "Output Envelope output");
    if (leftInput < 0 || rightInput < 0 || leftOutput < 0 || leftMode < 0 ||
        rightOutput < 0 || rightMode < 0 || source < 0 || folder < 0 || sample < 0 ||
        pitch < 0 || stretch < 0 || threshold < 0 || grain < 0 || quality < 0 ||
        feedback < 0 || envelopeSmoothing < 0 || fade < 0 || drive < 0 ||
        driveCharacter < 0 || mix < 0 || feedbackTone < 0 ||
        inputTransientOutput < 0 || outputTransientOutput < 0 ||
        inputEnvelopeOutput < 0 || outputEnvelopeOutput < 0) {
        return fail("expected performance, source, and routing parameters are unavailable");
    }

    if (algorithm->parameters[source].unit != kNT_unitEnum ||
        algorithm->parameters[folder].unit != kNT_unitHasStrings ||
        algorithm->parameters[sample].unit != kNT_unitConfirm ||
        factory->parameterString == nullptr) {
        return fail("source parameters do not expose the host folder/sample pickers");
    }

    const int analysisOutputs[] = {
        inputTransientOutput, outputTransientOutput,
        inputEnvelopeOutput, outputEnvelopeOutput,
    };
    for (int parameter : analysisOutputs) {
        const _NT_parameter& definition = algorithm->parameters[parameter];
        if (definition.unit != kNT_unitCvOutput || definition.min != 0 ||
            definition.def != 0 || definition.max != kNT_lastBus ||
            !pageContains(algorithm->parameterPages, "Routing", parameter)) {
            return fail("analysis CV selector is not default-disconnected routing");
        }
    }

    const int auditedControls[] = {
        pitch, stretch, threshold, grain, quality, feedback,
        envelopeSmoothing, fade, drive, driveCharacter, mix, feedbackTone,
    };
    int audioInputParameters = 0;
    for (int parameter = 0; parameter < count; ++parameter) {
        if (algorithm->parameters[parameter].unit == kNT_unitAudioInput) {
            ++audioInputParameters;
        }
    }
    if (audioInputParameters != 2 ||
        algorithm->parameters[leftInput].unit != kNT_unitAudioInput ||
        algorithm->parameters[rightInput].unit != kNT_unitAudioInput) {
        return fail("processing added a plug-in-specific direct CV input");
    }
    for (int parameter : auditedControls) {
        if (!pageContains(algorithm->parameterPages, "Performance", parameter) ||
            algorithm->parameters[parameter].unit == kNT_unitAudioInput) {
            return fail("an audited control is not an ordinary NT parameter");
        }
    }
    const int secondaryControls[] = {
        envelopeSmoothing, fade, drive, driveCharacter, feedbackTone,
    };
    const int expectedDefaults[] = {4259, 2153, 1429, 10000, 3769};
    for (int i = 0; i < 5; ++i) {
        const _NT_parameter& definition = algorithm->parameters[secondaryControls[i]];
        if (definition.min != 0 || definition.max != 10000 ||
            definition.def != expectedDefaults[i] ||
            definition.scaling != kNT_scaling100) {
            return fail("secondary control does not preserve its audited normalized sweep");
        }
    }

    std::vector<int16_t> values(requirements.numParameters, 0);
    for (int i = 0; i < count; ++i) values[i] = algorithm->parameters[i].def;
    values[leftInput] = 1;
    values[rightInput] = 0;
    values[leftOutput] = 13;
    values[leftMode] = 1;
    values[rightOutput] = 14;
    values[rightMode] = 1;
    values[source] = 0;
    values[grain] = 128;
    values[quality] = 100;
    values[mix] = 100;
    algorithm->v = values.data();

    // The host persists ordinary parameter values in its preset. Recreate a
    // fresh instance with those values already restored, including Sample
    // mode. Host parameter callbacks restore the valid resource before the
    // first audio step; step itself must do no catalogue or stream-open work.
    std::vector<uint8_t> restoredSram(requirements.sram + 1U);
    std::vector<uint8_t> restoredDram(requirements.dram + 1U);
    std::vector<uint8_t> restoredDtc(requirements.dtc + 1U);
    _NT_algorithmMemoryPtrs restoredMemory{
        restoredSram.data() + 1,
        restoredDram.data() + 1,
        restoredDtc.data() + 1,
        nullptr,
    };
    _NT_algorithm* restored = factory->construct(
        restoredMemory, requirements, nullptr);
    std::vector<int16_t> restoredValues = values;
    restoredValues[source] = 1;
    restoredValues[folder] = 0;
    restoredValues[sample] = 1;
    restored->v = restoredValues.data();
    gAlgorithm = restored;
    const uint32_t opensBeforePresetRestore = gStreamOpenCalls;
    factory->parameterChanged(restored, source);
    factory->parameterChanged(restored, folder);
    factory->parameterChanged(restored, sample);
    const uint32_t folderCallsBeforeRestoredStep = gFolderInfoCalls;
    const uint32_t fileCallsBeforeRestoredStep = gFileInfoCalls;
    const uint32_t definitionUpdatesBeforeRestoredStep =
        gParameterDefinitionUpdates;
    std::vector<float> restoredBuses(kNT_lastBus * kFrames, 0.0f);
    factory->step(restored, restoredBuses.data(), kFrames / 4);
    if (gStreamOpenCalls != opensBeforePresetRestore + 1 ||
        gOpenedFolder != 0 || gOpenedSample != 1 ||
        gStreamRenderCalls == 0 || restoredValues[source] != 1 ||
        gFolderInfoCalls != folderCallsBeforeRestoredStep ||
        gFileInfoCalls != fileCallsBeforeRestoredStep ||
        gParameterDefinitionUpdates != definitionUpdatesBeforeRestoredStep) {
        return fail("valid host-persisted sample reference was not restored");
    }
    gAlgorithm = algorithm;

    const uint32_t requiredPerformanceControls =
        kNT_potL | kNT_potC | kNT_potR |
        kNT_potButtonL | kNT_potButtonC | kNT_potButtonR |
        kNT_encoderL | kNT_encoderR | kNT_encoderButtonL | kNT_encoderButtonR;
    if (factory->hasCustomUi == nullptr || factory->customUi == nullptr ||
        factory->setupUi == nullptr || factory->draw == nullptr ||
        (factory->hasCustomUi(algorithm) & requiredPerformanceControls) !=
            requiredPerformanceControls) {
        return fail("approved performance controls are not exposed by a custom UI");
    }
    _NT_float3 initialPots{};
    factory->setupUi(algorithm, initialPots);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("CAPICOLA   LIVE") ||
        !drawnTextContains("STRETCH") || !drawnTextContains("THRESH") ||
        !drawnTextContains("FEEDBACK") || !drawnTextContains("MAIN") ||
        !drawnTextContains("IN ") || !drawnTextContains("OUT ") ||
        !drawnTextContains("MIX 100%")) {
        return fail("persistent performance screen omitted source, controls, or activity");
    }

    _NT_uiData ui{};
    ui.controls = kNT_potL;
    ui.pots[0] = 1.0f;
    factory->customUi(algorithm, ui);
    if (values[stretch] != 100) {
        return fail("main performance pot did not control Stretch");
    }
    ui = {};
    ui.controls = kNT_potButtonL;
    factory->customUi(algorithm, ui);
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.75f;
    factory->customUi(algorithm, ui);
    if (values[pitch] != 60) {
        return fail("alternate performance pot did not control Pitch");
    }
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("ALT") || !drawnTextContains("PITCH") ||
        !drawnTextContains("+6.0 st") || !drawnTextContains("GRAIN") ||
        !drawnTextContains("QUALITY")) {
        return fail("alternate pot functions did not show their identity and value");
    }
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.25f;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (values[pitch] != -60 || !drawnTextContains("-6.0 st")) {
        return fail("negative Pitch value did not update on the performance screen");
    }
    // Every pressable pot switches the shared MAIN/ALT performance bank.
    ui = {};
    ui.controls = kNT_potButtonC;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("MAIN") || !drawnTextContains("STRETCH")) {
        return fail("centre pot press did not restore the visible main bank");
    }
    ui = {};
    ui.controls = kNT_potButtonR;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("ALT") || !drawnTextContains("QUALITY")) {
        return fail("right pot press did not expose the visible alternate bank");
    }
    ui = {};
    ui.controls = kNT_potButtonR;
    factory->customUi(algorithm, ui);

    ui = {};
    ui.encoders[1] = -10;
    factory->customUi(algorithm, ui);
    if (values[mix] != 90) {
        return fail("right encoder did not control Mix");
    }
    // Restore neutral processing for source-path assertions below.
    values[pitch] = 0;
    values[stretch] = 0;
    values[mix] = 100;
    factory->parameterChanged(algorithm, pitch);
    factory->parameterChanged(algorithm, stretch);
    factory->parameterChanged(algorithm, mix);

    std::vector<float> buses(kNT_lastBus * kFrames, 0.0f);
    // No analysis bus may be claimed on first load while all four selectors
    // retain their zero defaults.
    for (int bus = 14; bus < 18; ++bus) {
        std::fill_n(buses.data() + bus * kFrames, kFrames,
                    10.0f + static_cast<float>(bus));
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    for (int bus = 14; bus < 18; ++bus) {
        for (int i = 0; i < kFrames; ++i) {
            if (buses[bus * kFrames + i] != 10.0f + static_cast<float>(bus)) {
                return fail("default-disconnected analysis selector claimed a bus");
            }
        }
    }
    std::fill(buses.begin(), buses.end(), 0.0f);

    // Simulate SD/catalogue activity caused by opening an external Source panel
    // while audio is running. Even rapid host-mapped Source changes must keep
    // step() independent of card state, catalogue queries, definition updates,
    // and stream opens, and every produced frame must remain finite.
    const uint32_t mountChecksBeforeSourceActivity = gCardMountChecks;
    const uint32_t folderCallsBeforeSourceActivity = gFolderInfoCalls;
    const uint32_t fileCallsBeforeSourceActivity = gFileInfoCalls;
    const uint32_t updatesBeforeSourceActivity = gParameterDefinitionUpdates;
    const uint32_t opensBeforeSourceActivity = gStreamOpenCalls;
    const uint32_t rendersBeforeSourceActivity = gStreamRenderCalls;
    for (int block = 0; block < 256; ++block) {
        gCardMounted = (block & 1) == 0;
        values[source] = block & 1;
        std::fill(buses.begin(), buses.end(), 0.0f);
        for (int i = 0; i < kFrames; ++i) {
            buses[i] = 0.25f;
        }
        factory->step(algorithm, buses.data(), kFrames / 4);
        for (int i = 0; i < kFrames; ++i) {
            if (!std::isfinite(buses[12 * kFrames + i]) ||
                !std::isfinite(buses[13 * kFrames + i])) {
                return fail("source activity produced an invalid audio frame");
            }
        }
    }
    gCardMounted = true;
    values[source] = 0;
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (gCardMountChecks != mountChecksBeforeSourceActivity ||
        gFolderInfoCalls != folderCallsBeforeSourceActivity ||
        gFileInfoCalls != fileCallsBeforeSourceActivity ||
        gParameterDefinitionUpdates != updatesBeforeSourceActivity ||
        gStreamOpenCalls != opensBeforeSourceActivity ||
        gStreamRenderCalls != rendersBeforeSourceActivity) {
        return fail("audio step performed SD catalogue or stream setup work");
    }
    std::fill(buses.begin(), buses.end(), 0.0f);

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

    // Assign every audited signal to a separate CV bus. Gates are exactly 0/5 V
    // and normalized envelopes remain in 0..5 V. Repeated impulses guarantee
    // both input- and post-mix-output detector activity is observed.
    values[inputTransientOutput] = 15;
    values[outputTransientOutput] = 16;
    values[inputEnvelopeOutput] = 17;
    values[outputEnvelopeOutput] = 18;
    bool sawInputTransient = false;
    bool sawOutputTransient = false;
    bool sawInputEnvelope = false;
    bool sawOutputEnvelope = false;
    bool screenShowedEnvelopeActivity = false;
    bool screenShowedTransientActivity = false;
    for (int block = 0; block < 360; ++block) {
        std::fill(buses.begin(), buses.end(), 0.0f);
        float* left = buses.data();
        if (block % 12 == 0) left[0] = 1.0f;
        factory->step(algorithm, buses.data(), kFrames / 4);
        const float inputGate = buses[14 * kFrames];
        const float outputGate = buses[15 * kFrames];
        const float inputEnvelope = buses[16 * kFrames];
        const float outputEnvelope = buses[17 * kFrames];
        if ((inputGate != 0.0f && inputGate != 5.0f) ||
            (outputGate != 0.0f && outputGate != 5.0f) ||
            inputEnvelope < 0.0f || inputEnvelope > 5.0f ||
            outputEnvelope < 0.0f || outputEnvelope > 5.0f) {
            return fail("analysis CV output exceeded its audited voltage range");
        }
        sawInputTransient = sawInputTransient || inputGate == 5.0f;
        sawOutputTransient = sawOutputTransient || outputGate == 5.0f;
        sawInputEnvelope = sawInputEnvelope || inputEnvelope > 0.0f;
        sawOutputEnvelope = sawOutputEnvelope || outputEnvelope > 0.0f;
        gDrawnText.clear();
        factory->draw(algorithm);
        screenShowedEnvelopeActivity = screenShowedEnvelopeActivity ||
            (!drawnTextContains("IN 00") && !drawnTextContains("OUT 00"));
        screenShowedTransientActivity = screenShowedTransientActivity ||
            drawnTextContains("!");
    }
    if (!sawInputTransient || !sawOutputTransient ||
        !sawInputEnvelope || !sawOutputEnvelope) {
        return fail("an assigned analysis signal did not reach its CV bus");
    }
    if (!screenShowedEnvelopeActivity || !screenShowedTransientActivity) {
        return fail("performance screen did not report envelope/transient activity");
    }

    // The shared Transient Threshold must govern the post-mix detector just as
    // it governs both input detectors upstream. At the audited top-of-range
    // setting, allow any existing gate to expire and then prove fresh output
    // peaks remain muted on the still-assigned CV bus.
    values[threshold] = 100;
    for (int block = 0; block < 20; ++block) {
        std::fill(buses.begin(), buses.end(), 0.0f);
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    for (int block = 0; block < 180; ++block) {
        std::fill(buses.begin(), buses.end(), 0.0f);
        if (block % 12 == 0) buses[0] = 1.0f;
        factory->step(algorithm, buses.data(), kFrames / 4);
        if (buses[15 * kFrames] != 0.0f) {
            return fail("top-range Transient Threshold did not mute Output Transient");
        }
    }

    // Restore the permissive end of the same control and prove Output
    // Transient resumes normal 0/5 V behavior without changing its assignment.
    values[threshold] = 0;
    bool sawRestoredOutputTransient = false;
    for (int block = 0; block < 360; ++block) {
        std::fill(buses.begin(), buses.end(), 0.0f);
        if (block % 12 == 0) buses[0] = 1.0f;
        factory->step(algorithm, buses.data(), kFrames / 4);
        const float outputGate = buses[15 * kFrames];
        if (outputGate != 0.0f && outputGate != 5.0f) {
            return fail("restored Output Transient left its assigned gate range");
        }
        sawRestoredOutputTransient =
            sawRestoredOutputTransient || outputGate == 5.0f;
    }
    if (!sawRestoredOutputTransient) {
        return fail("Output Transient did not resume on its assigned CV bus");
    }

    // Selecting 0 disconnects every assignment immediately and leaves each
    // previously selected physical/auxiliary bus untouched.
    for (int parameter : analysisOutputs) values[parameter] = 0;
    for (int bus = 14; bus < 18; ++bus) {
        std::fill_n(buses.data() + bus * kFrames, kFrames, 23.0f);
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    for (int bus = 14; bus < 18; ++bus) {
        for (int i = 0; i < kFrames; ++i) {
            if (buses[bus * kFrames + i] != 23.0f) {
                return fail("selector 0 did not disconnect an analysis CV output");
            }
        }
    }
    std::fill(buses.begin(), buses.end(), 0.0f);

    // Select the host-catalogued stereo sample. NaN on both live buses proves
    // that sample mode does not read or mix either live source.
    values[folder] = 0;
    values[sample] = 1;
    const uint32_t opensBeforeSampleSource = gStreamOpenCalls;
    ui = {};
    ui.encoders[0] = 1;
    factory->customUi(algorithm, ui);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeSampleSource) {
        return fail("performance UI did not select Sample mode without opening early");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource ||
        !drawnTextContains("SELECT FOLDER") || !drawnTextContains("Drums") ||
        !drawnTextContains("PRESS: NEXT")) {
        return fail("sample loading did not open the temporary folder selection");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource ||
        !drawnTextContains("SELECT SAMPLE") ||
        !drawnTextContains("Stereo.wav") || !drawnTextContains("PRESS: LOAD")) {
        return fail("folder confirmation did not open temporary sample selection");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource + 1 ||
        !drawnTextContains("CAPICOLA   SAMPLE PLAY")) {
        return fail("sample confirmation did not return to the performance screen");
    }

    // All five audited secondary controls must alter their intended linked
    // Capicola/feedback path while the selected sample is the sole source.
    // Reopening the sample resets both the host stream and engine, making each
    // signature deterministic and independent of the previous control trial.
    auto sampleSignature = [&](int parameter, int value, int feedbackValue,
                               int characterValue) {
        for (int control : secondaryControls) {
            values[control] = algorithm->parameters[control].def;
        }
        values[feedback] = feedbackValue;
        values[driveCharacter] = characterValue;
        values[parameter] = value;
        factory->parameterChanged(algorithm, parameter);
        factory->parameterChanged(algorithm, sample);
        double signature = 0.0;
        for (int block = 0; block < 360; ++block) {
            factory->step(algorithm, buses.data(), kFrames / 4);
            const float* renderedLeft = buses.data() + 12 * kFrames;
            const float* renderedRight = buses.data() + 13 * kFrames;
            if (block >= 240) {
                for (int i = 0; i < kFrames; ++i) {
                    signature += renderedLeft[i] * (1.0 + i * 0.001) +
                                 renderedRight[i] * (1.7 + i * 0.002);
                }
            }
        }
        return signature;
    };
    const double smoothingLow = sampleSignature(envelopeSmoothing, 0, 0, 10000);
    const double smoothingHigh = sampleSignature(envelopeSmoothing, 10000, 0, 10000);
    const double fadeLow = sampleSignature(fade, 0, 0, 10000);
    const double fadeHigh = sampleSignature(fade, 10000, 0, 10000);
    const double driveLow = sampleSignature(drive, 0, 0, 10000);
    const double driveHigh = sampleSignature(drive, 10000, 0, 10000);
    const double characterLow = sampleSignature(driveCharacter, 0, 0, 0);
    const double characterHigh = sampleSignature(driveCharacter, 10000, 0, 10000);
    const double toneLow = sampleSignature(feedbackTone, 0, 100, 10000);
    const double toneHigh = sampleSignature(feedbackTone, 10000, 100, 10000);
    if (std::fabs(smoothingLow - smoothingHigh) < 1.0e-4 ||
        std::fabs(fadeLow - fadeHigh) < 1.0e-4 ||
        std::fabs(driveLow - driveHigh) < 1.0e-4 ||
        std::fabs(characterLow - characterHigh) < 1.0e-4 ||
        std::fabs(toneLow - toneHigh) < 1.0e-4) {
        return fail("a secondary control did not reach selected-sample processing");
    }
    // Restore audited defaults before continuing the source-isolation checks.
    for (int control : secondaryControls) values[control] = algorithm->parameters[control].def;
    values[feedback] = algorithm->parameters[feedback].def;
    factory->parameterChanged(algorithm, sample);

    // Simulate the host's ordinary parameter-to-CV mapping by replacing the
    // effective Mix value in v[] between audio callbacks. Mapping is host-owned,
    // so it does not call the plug-in's UI path or require parameterChanged().
    // step() must consume the mapped value and apply it to Capicola.
    values[mix] = 0;
    const uint32_t dryClock = gStreamClock;
    factory->step(algorithm, buses.data(), kFrames / 4);
    const float* dryLeft = buses.data() + 12 * kFrames;
    const float* dryRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        const float expectedLeft = std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(dryClock + i) / 48000.0f);
        const float expectedRight = 0.35f * std::sin(
            2.0f * kPi * 710.0f * static_cast<float>(dryClock + i) / 48000.0f);
        if (std::fabs(dryLeft[i] - expectedLeft) > 1.0e-6f ||
            std::fabs(dryRight[i] - expectedRight) > 1.0e-6f) {
            return fail("host-mapped dry Mix value did not reach selected-sample processing");
        }
    }
    // Move the same host-mapped effective parameter to fully wet, again
    // without a plug-in-specific CV parameter or a parameter callback.
    values[mix] = 100;

    double sampleEnergy = 0.0;
    double stereoDifference = 0.0;
    double mappedWetDifference = 0.0;
    for (int block = 0; block < 320; ++block) {
        std::fill_n(buses.data(), 2 * kFrames,
                    std::numeric_limits<float>::quiet_NaN());
        const uint32_t blockClock = gStreamClock;
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
                const float dryExpectedLeft = std::sin(
                    2.0f * kPi * 330.0f * static_cast<float>(blockClock + i) /
                    48000.0f);
                const float dryExpectedRight = 0.35f * std::sin(
                    2.0f * kPi * 710.0f * static_cast<float>(blockClock + i) /
                    48000.0f);
                mappedWetDifference += std::fabs(outLeft[i] - dryExpectedLeft) +
                                       std::fabs(outRight[i] - dryExpectedRight);
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
    if (mappedWetDifference < 1.0) {
        return fail("host-mapped Mix value did not modulate Capicola processing");
    }

    // A folder change updates the Sample range and invalidates the old stream,
    // but only explicit Sample confirmation may open the new resource.
    const uint32_t opensBeforeFolderChange = gStreamOpenCalls;
    const uint32_t rendersBeforeFolderChange = gStreamRenderCalls;
    values[folder] = 1;
    values[sample] = 0;
    factory->parameterChanged(algorithm, folder);
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (gStreamOpenCalls != opensBeforeFolderChange ||
        gStreamRenderCalls != rendersBeforeFolderChange) {
        return fail("folder change opened or rendered before Sample confirmation");
    }
    factory->parameterChanged(algorithm, sample);
    if (gStreamOpenCalls != opensBeforeFolderChange + 1 ||
        gOpenedFolder != 1 || gOpenedSample != 0) {
        return fail("Sample confirmation did not open the selected folder resource");
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

    // Card loss or a zero-frame stream result is handled inside step() only by
    // dropping/fading the block and closing the stream. Remounting must not run
    // catalogue discovery or reopen work on the audio thread. Explicit Sample
    // confirmation performs recovery outside that thread.
    const uint32_t opensBeforeRemount = gStreamOpenCalls;
    const uint32_t rendersBeforeRemount = gStreamRenderCalls;
    const uint32_t mountChecksBeforeRemount = gCardMountChecks;
    const uint32_t folderCallsBeforeRemount = gFolderInfoCalls;
    const uint32_t fileCallsBeforeRemount = gFileInfoCalls;
    const uint32_t updatesBeforeRemount = gParameterDefinitionUpdates;
    gCardMounted = false;
    factory->step(algorithm, buses.data(), kFrames / 4);
    gCardMounted = true;
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (gStreamOpenCalls != opensBeforeRemount ||
        gStreamRenderCalls != rendersBeforeRemount + 1 ||
        gCardMountChecks != mountChecksBeforeRemount ||
        gFolderInfoCalls != folderCallsBeforeRemount ||
        gFileInfoCalls != fileCallsBeforeRemount ||
        gParameterDefinitionUpdates != updatesBeforeRemount) {
        return fail("audio step performed card recovery or catalogue work");
    }
    for (int i = 0; i < kFrames; ++i) {
        if (buses[12 * kFrames + i] != 0.0f ||
            buses[13 * kFrames + i] != 0.0f) {
            return fail("zero-frame stream failure did not fade to silence");
        }
    }
    factory->parameterChanged(algorithm, sample);
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (gStreamOpenCalls != opensBeforeRemount + 1 ||
        gStreamRenderCalls != rendersBeforeRemount + 2 ||
        gOpenedFolder != 1 || gOpenedSample != 0) {
        return fail("explicit Sample confirmation did not recover after remount");
    }

    // Missing or moved catalogue entry: the saved folder index is no longer
    // present. Explicit confirmation performs no invalid lookup/open and Sample
    // mode remains selected.
    gCardMounted = false;
    factory->step(algorithm, buses.data(), kFrames / 4);
    gNumFolders = 1;
    const uint32_t opensBeforeMissing = gStreamOpenCalls;
    const uint32_t filesBeforeMissing = gFileInfoCalls;
    gCardMounted = true;
    factory->step(algorithm, buses.data(), kFrames / 4);
    factory->parameterChanged(algorithm, sample);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeMissing ||
        gFileInfoCalls != filesBeforeMissing || gInvalidCatalogLookup) {
        return fail("missing sample reference substituted or fell back to live input");
    }
    for (int i = 0; i < kFrames; ++i) {
        if (buses[12 * kFrames + i] != 0.0f ||
            buses[13 * kFrames + i] != 0.0f) {
            return fail("missing sample reference did not keep Sample mode silent");
        }
    }

    // Unsupported metadata is rejected before stream open.
    gNumFolders = 2;
    values[folder] = 0;
    values[sample] = 1;
    gSampleMetadataSupported = false;
    const uint32_t opensBeforeUnsupported = gStreamOpenCalls;
    factory->parameterChanged(algorithm, sample);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeUnsupported) {
        return fail("unsupported sample substituted or fell back to live input");
    }

    // An unreadable resource reaches the host streamer, whose failed open is
    // authoritative. The wrapper retains Sample mode and renders silence.
    gSampleMetadataSupported = true;
    gStreamOpenSucceeds = false;
    const uint32_t opensBeforeUnreadable = gStreamOpenCalls;
    const uint32_t rendersBeforeUnreadable = gStreamRenderCalls;
    factory->parameterChanged(algorithm, sample);
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeUnreadable + 1 ||
        gStreamRenderCalls != rendersBeforeUnreadable) {
        return fail("unreadable sample substituted, rendered, or fell back to live input");
    }
    for (int i = 0; i < kFrames; ++i) {
        if (buses[12 * kFrames + i] != 0.0f ||
            buses[13 * kFrames + i] != 0.0f) {
            return fail("unreadable sample did not keep Sample mode silent");
        }
    }
    gStreamOpenSucceeds = true;

    // Invalid catalogue values keep Sample mode silent and never substitute a
    // different folder or file in lookups, display strings, or stream opens.
    const uint32_t opensBeforeInvalid = gStreamOpenCalls;
    const uint32_t rendersBeforeInvalid = gStreamRenderCalls;
    const uint32_t folderInfoBeforeInvalid = gFolderInfoCalls;
    const uint32_t fileInfoBeforeInvalid = gFileInfoCalls;
    values[folder] = 99;
    values[sample] = 99;
    factory->parameterChanged(algorithm, folder);
    factory->parameterChanged(algorithm, sample);
    char invalidText[kNT_parameterStringSize] = {};
    if (factory->parameterString(algorithm, folder, 99, invalidText) != 0 ||
        factory->parameterString(algorithm, sample, 99, invalidText) != 0) {
        return fail("invalid catalogue value displayed a substituted name");
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeInvalid ||
        gStreamRenderCalls != rendersBeforeInvalid ||
        gFolderInfoCalls != folderInfoBeforeInvalid ||
        gFileInfoCalls != fileInfoBeforeInvalid || gInvalidCatalogLookup) {
        return fail("invalid folder substituted or accessed another resource");
    }
    const float* silentLeft = buses.data() + 12 * kFrames;
    const float* silentRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        if (silentLeft[i] != 0.0f || silentRight[i] != 0.0f) {
            return fail("invalid folder did not leave Sample mode silent");
        }
    }
    values[folder] = -1;
    factory->parameterChanged(algorithm, folder);
    factory->parameterChanged(algorithm, sample);
    if (factory->parameterString(algorithm, folder, -1, invalidText) != 0 ||
        gStreamOpenCalls != opensBeforeInvalid || gInvalidCatalogLookup) {
        return fail("negative folder value substituted another resource");
    }

    values[folder] = 0;
    factory->parameterChanged(algorithm, folder);
    const uint32_t opensBeforeInvalidSample = gStreamOpenCalls;
    const uint32_t filesBeforeInvalidSample = gFileInfoCalls;
    values[sample] = 99;
    factory->parameterChanged(algorithm, sample);
    values[sample] = -1;
    factory->parameterChanged(algorithm, sample);
    if (factory->parameterString(algorithm, sample, -1, invalidText) != 0 ||
        gStreamOpenCalls != opensBeforeInvalidSample ||
        gFileInfoCalls != filesBeforeInvalidSample || gInvalidCatalogLookup) {
        return fail("invalid sample substituted or opened another file");
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    silentLeft = buses.data() + 12 * kFrames;
    silentRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        if (values[source] != 1 || silentLeft[i] != 0.0f ||
            silentRight[i] != 0.0f) {
            return fail("invalid sample did not leave Sample mode silent");
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
