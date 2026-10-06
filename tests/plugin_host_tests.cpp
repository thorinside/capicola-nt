#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <distingnt/api.h>
#define _DISTINGNT_SERIALISATION_INTERNAL
#define _DISTINGNT_SLOT_INTERNAL
#include <distingnt/serialisation.h>
#include <distingnt/slot.h>
#include <distingnt/wav.h>

#include "capicola_nt/live_path.h"
#include "capicola_nt/preset_format.h"

namespace {

constexpr int kFrames = 64;
constexpr float kPi = 3.14159265358979323846f;
float gWorkBuffer[kFrames * 2] = {};
bool gCardMounted = true;
uint32_t gNumFolders = 2;
uint32_t gDrumsSampleCount = 2;
uint32_t gSampleFrameCount = 48000;
bool gSampleMetadataSupported = true;
bool gStreamOpenSucceeds = true;
uint32_t gStreamRenderCalls = 0;
uint32_t gStreamOpenCalls = 0;
uint32_t gSampleReadCalls = 0;
uint32_t gFolderInfoCalls = 0;
uint32_t gFileInfoCalls = 0;
uint32_t gCardMountChecks = 0;
uint32_t gParameterDefinitionUpdates = 0;
bool gInvalidCatalogLookup = false;
bool gDeferParameterUiCommit = false;
int32_t gPendingParameter = -1;
int16_t gPendingParameterValue = 0;
uint32_t gStreamClock = 0;
float gStreamSourcePosition = 0.0f;
uint32_t gOpenedFolder = 0;
uint32_t gOpenedSample = 0;
uint32_t gStreamInitialEmptyRenders = 0;
uint32_t gOpenedStreamFrameCount = 0;
bool gConstantSample = false;
_NT_algorithm* gAlgorithm = nullptr;
const _NT_factory* gFactory = nullptr;
bool gSlotAvailable = true;
uint32_t gAudioParameterWrites = 0;
uint32_t gSlotReads = 0;
uint32_t gParameterOffset = 0;
uint32_t gLastAudioParameter = 0;
struct JsonMember { std::string name; int value; bool numeric = true; };
struct HostJson {
    bool object = true;
    std::vector<JsonMember> members;
    std::string pendingName;
};
struct HostStreamState {
    uint32_t folder;
    uint32_t sample;
    uint32_t clock;
    uint32_t emptyRendersRemaining;
    uint32_t frameCount;
    float sourcePosition;
    bool open;
};
static_assert(sizeof(HostStreamState) <= 64U);
struct DrawCall {
    std::string text;
    int x;
    _NT_textSize size;
};
std::vector<DrawCall> gDrawnText;

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
    for (const DrawCall& call : gDrawnText) {
        if (call.text.find(fragment) != std::string::npos) return true;
    }
    return false;
}

bool drawnTextAt(const char* fragment, int x, _NT_textSize size) {
    for (const DrawCall& call : gDrawnText) {
        if (call.x == x && call.size == size &&
            call.text.find(fragment) != std::string::npos) {
            return true;
        }
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
    static const char* names[] = {
        "Mono.wav", "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcd.wav.WAV",
        "Cloud.wav",
    };
    info.name = folder == 0 ? names[sample] : names[2];
    info.numFrames = gSampleFrameCount;
    info.sampleRate = gSampleMetadataSupported
        ? (folder == 1 ? 24000U : 48000U) : 0U;
    info.channels = sample == 0 ? kNT_WavMono : kNT_WavStereo;
    info.bits = kNT_WavBits16;
}

extern "C" bool NT_readSampleFrames(const _NT_wavRequest&) {
    ++gSampleReadCalls;
    return false;
}

extern "C" bool NT_streamOpen(_NT_stream stream,
                               const _NT_streamOpenData& data) {
    ++gStreamOpenCalls;
    if (stream == nullptr || data.streamBuffer == nullptr ||
        reinterpret_cast<uintptr_t>(stream) % alignof(uint32_t) != 0U) {
        gInvalidCatalogLookup = true;
        return false;
    }
    gOpenedFolder = data.folder;
    gOpenedSample = data.sample;
    HostStreamState* state = static_cast<HostStreamState*>(stream);
    *state = {
        data.folder,
        data.sample,
        0U,
        gStreamInitialEmptyRenders,
        gOpenedStreamFrameCount == 0U
            ? gSampleFrameCount : gOpenedStreamFrameCount,
        0.0f,
        gStreamOpenSucceeds,
    };
    return state->open;
}

extern "C" uint32_t NT_streamRender(_NT_stream stream,
                                      _NT_frame* renderBuffer,
                                      uint32_t numFrames,
                                      float speed) {
    ++gStreamRenderCalls;
    HostStreamState* state = static_cast<HostStreamState*>(stream);
    if (state == nullptr || !state->open || !gCardMounted ||
        !std::isfinite(speed) || speed <= 0.0f) {
        return 0;
    }
    if (state->emptyRendersRemaining != 0U) {
        --state->emptyRendersRemaining;
        gStreamClock = state->clock;
        gStreamSourcePosition = state->sourcePosition;
        return 0;
    }
    const uint32_t sourceRate = state->folder == 1 ? 24000U : 48000U;
    uint32_t rendered = 0;
    for (; rendered < numFrames &&
           state->sourcePosition < static_cast<float>(state->frameCount);
         ++rendered, ++state->clock) {
        const uint32_t index = static_cast<uint32_t>(state->sourcePosition);
        const float fraction = state->sourcePosition - static_cast<float>(index);
        const uint32_t next = index + 1U < state->frameCount ? index + 1U : index;
        const float source0 = std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(index) /
            static_cast<float>(sourceRate));
        const float source1 = std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(next) /
            static_cast<float>(sourceRate));
        renderBuffer[rendered][0] = source0 + fraction * (source1 - source0);
        renderBuffer[rendered][1] = state->sample == 0
            ? renderBuffer[rendered][0]
            : 0.35f * std::sin(
                2.0f * kPi * 710.0f * state->sourcePosition /
                static_cast<float>(sourceRate));
        if (gConstantSample) {
            renderBuffer[rendered][0] = renderBuffer[rendered][1] = 0.5f;
        }
        state->sourcePosition += speed;
    }
    gStreamClock = state->clock;
    gStreamSourcePosition = state->sourcePosition;
    return rendered;
}

extern "C" int32_t NT_algorithmIndex(const _NT_algorithm* algorithm) {
    gAlgorithm = const_cast<_NT_algorithm*>(algorithm);
    return 0;
}

extern "C" bool NT_getSlot(_NT_slot& slot, uint32_t index) {
    ++gSlotReads;
    if (!gSlotAvailable || index != 0 || gAlgorithm == nullptr) return false;
    slot.refCon = gAlgorithm;
    return true;
}

int16_t _NT_slot::parameterPresetValue(uint32_t index) const {
    const auto* algorithm = static_cast<const _NT_algorithm*>(refCon);
    const int16_t* base = algorithm->vIncludingCommon != nullptr
        ? algorithm->vIncludingCommon : algorithm->v;
    return base[index - gParameterOffset];
}

extern "C" void NT_updateParameterDefinition(uint32_t, uint32_t) {
    ++gParameterDefinitionUpdates;
}

extern "C" uint32_t NT_parameterOffset() {
    return gParameterOffset;
}

void commitHostParameter(_NT_algorithm* algorithm, uint32_t parameter, int16_t value) {
    const uint32_t p = parameter - gParameterOffset;
    int delta = 0;
    if (algorithm->vIncludingCommon != nullptr) {
        delta = algorithm->v[p] - algorithm->vIncludingCommon[p];
        const_cast<int16_t*>(algorithm->vIncludingCommon)[p] = value;
    }
    const auto& definition = algorithm->parameters[p];
    const_cast<int16_t*>(algorithm->v)[p] = static_cast<int16_t>(
        capicola_nt::clampControl(value + delta, definition.min, definition.max));
    gFactory->parameterChanged(algorithm, static_cast<int>(p));
}

extern "C" void NT_setParameterFromAudio(uint32_t, uint32_t parameter, int16_t value) {
    ++gAudioParameterWrites;
    gLastAudioParameter = parameter;
    commitHostParameter(gAlgorithm, parameter, value);
}

extern "C" void NT_setParameterFromUi(uint32_t, uint32_t parameter, int16_t value) {
    if (gAlgorithm != nullptr && gFactory != nullptr) {
        if (gDeferParameterUiCommit) {
            gPendingParameter = static_cast<int32_t>(parameter);
            gPendingParameterValue = value;
            return;
        }
        commitHostParameter(gAlgorithm, parameter, value);
    }
}

void NT_drawText(int x, int, const char* text, int,
                 _NT_textAlignment, _NT_textSize size) {
    gDrawnText.push_back({text == nullptr ? "" : text, x, size});
}

_NT_jsonStream::_NT_jsonStream(void* context) : refCon(context) {}
_NT_jsonStream::~_NT_jsonStream() = default;
void _NT_jsonStream::addMemberName(const char* name) {
    static_cast<HostJson*>(refCon)->pendingName = name;
}
void _NT_jsonStream::addNumber(int value) {
    auto& json = *static_cast<HostJson*>(refCon);
    json.members.push_back({json.pendingName, value});
}
_NT_jsonParse::_NT_jsonParse(void* context, int index) : refCon(context), i(index) {}
_NT_jsonParse::~_NT_jsonParse() = default;
bool _NT_jsonParse::numberOfObjectMembers(int& number) {
    const auto& json = *static_cast<const HostJson*>(refCon);
    number = static_cast<int>(json.members.size());
    return json.object;
}
bool _NT_jsonParse::matchName(const char* name) {
    const auto& json = *static_cast<const HostJson*>(refCon);
    if ((i & 1) != 0 || i / 2 >= static_cast<int>(json.members.size()) ||
        json.members[i / 2].name != name) return false;
    ++i;
    return true;
}
bool _NT_jsonParse::number(int& value) {
    const auto& json = *static_cast<const HostJson*>(refCon);
    if ((i & 1) == 0 || i / 2 >= static_cast<int>(json.members.size()) ||
        !json.members[i / 2].numeric) return false;
    value = json.members[i / 2].value;
    ++i;
    return true;
}
bool _NT_jsonParse::skipMember() {
    const auto& json = *static_cast<const HostJson*>(refCon);
    if ((i & 1) != 0 || i / 2 >= static_cast<int>(json.members.size())) return false;
    i += 2;
    return true;
}

namespace {

// Exercise the exported plug-in callbacks with a fresh host-owned instance.
// Only the external NT catalogue, streaming service, and controls are simulated.
struct HostPlugin {
    const _NT_factory* factory;
    _NT_algorithmRequirements requirements{};
    std::vector<uint8_t> sram, dram, dtc;
    std::vector<int16_t> values;
    std::vector<int16_t> baseValues;
    std::vector<float> buses;
    _NT_algorithm* algorithm;

    explicit HostPlugin(bool unity = true) : factory(reinterpret_cast<const _NT_factory*>(
        pluginEntry(kNT_selector_factoryInfo, 0))) {
        gCardMounted = true;
        gNumFolders = 2;
        gDrumsSampleCount = 2;
        gSampleFrameCount = 48000;
        gOpenedStreamFrameCount = 0;
        gConstantSample = false;
        gSampleMetadataSupported = true;
        gStreamOpenSucceeds = true;
        gStreamInitialEmptyRenders = 0;
        gDeferParameterUiCommit = false;
        gSlotAvailable = true;
        gParameterOffset = 0;
        factory->calculateRequirements(requirements, nullptr);
        sram.resize(requirements.sram);
        dram.resize(requirements.dram);
        dtc.resize(requirements.dtc);
        const _NT_algorithmMemoryPtrs memory{
            sram.data(), dram.data(), dtc.data(), nullptr};
        algorithm = factory->construct(memory, requirements, nullptr);
        values.resize(requirements.numParameters);
        for (uint32_t i = 0; i < requirements.numParameters; ++i) {
            values[i] = algorithm->parameters[i].def;
        }
        algorithm->v = values.data();
        baseValues = values;
        algorithm->vIncludingCommon = baseValues.data();
        buses.resize(kNT_lastBus * kFrames);
        activate();
        set("Left output mode", 1);
        set("Right output mode", 1);
        // Most source-path tests need unity processing. Tests of actual fresh
        // defaults explicitly opt out, so the center stop/freeze stays tested.
        if (unity) {
            if (!useNativeFormat()) std::abort();
            set("Pitch", 5000); set("Stretch", 10000);
        }
    }

    void activate() { gAlgorithm = algorithm; gFactory = factory; }
    int parameter(const char* name) const {
        return findParameter(algorithm, requirements.numParameters, name);
    }
    void set(const char* name, int value) {
        activate();
        const int p = parameter(name);
        values[p] = static_cast<int16_t>(value);
        baseValues[p] = static_cast<int16_t>(value);
        factory->parameterChanged(algorithm, p);
    }
    void step() {
        activate();
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    void map(const char* name, int delta) {
        const int p = parameter(name);
        const auto& definition = algorithm->parameters[p];
        values[p] = static_cast<int16_t>(capicola_nt::clampControl(
            baseValues[p] + delta, definition.min, definition.max));
    }
    bool restore(HostJson& json, bool pointersUnavailable = false) {
        activate();
        const int16_t* savedValues = algorithm->v;
        const int16_t* savedBase = algorithm->vIncludingCommon;
        if (pointersUnavailable) { algorithm->v = nullptr; algorithm->vIncludingCommon = nullptr; }
        const uint32_t reads = gSlotReads, writes = gAudioParameterWrites;
        _NT_jsonParse parse(&json, 0);
        const bool result = factory->deserialise(algorithm, parse);
        algorithm->v = savedValues;
        algorithm->vIncludingCommon = savedBase;
        if (gSlotReads != reads || gAudioParameterWrites != writes) return false;
        return result;
    }
    bool useNativeFormat() {
        HostJson json{true, {{"capicolaFormatVersion", 2}}, ""};
        return restore(json, true);
    }
    bool loadParameters(const std::vector<int16_t>& saved) {
        if (saved.size() != values.size()) return false;
        std::copy(saved.begin(), saved.end(), values.begin());
        std::copy(saved.begin(), saved.end(), baseValues.begin());
        activate();
        for (uint32_t i = 0; i < requirements.numParameters; ++i) {
            factory->parameterChanged(algorithm, i);
        }
        return true;
    }
    HostJson save() {
        activate();
        HostJson json;
        _NT_jsonStream stream(&json);
        factory->serialise(algorithm, stream);
        return json;
    }
    void ui(const _NT_uiData& event) {
        activate();
        factory->customUi(algorithm, event);
    }
    float output(int frame, int channel = 0) const {
        return buses[(12 + channel) * kFrames + frame];
    }
};

bool testReleasedParameterAndPagePrefixes() {
    HostPlugin plugin(false);
    if (plugin.values[9] != 0 || plugin.values[10] != 50 ||
        plugin.algorithm->parameters[10].def != 50) return false;
    plugin.step();
    struct Definition { const char* name; int min, max, def, unit, scaling; };
    const Definition released[] = {
        {"Left input", 1, 64, 1, kNT_unitAudioInput, 0},
        {"Right input", 0, 64, 2, kNT_unitAudioInput, 0},
        {"Left output", 1, 64, 13, kNT_unitAudioOutput, 0},
        {"Left output mode", 0, 1, 0, kNT_unitOutputMode, 0},
        {"Right output", 1, 64, 14, kNT_unitAudioOutput, 0},
        {"Right output mode", 0, 1, 0, kNT_unitOutputMode, 0},
        {"Source", 0, 1, 0, kNT_unitEnum, 0},
        {"Folder", 0, 1, 0, kNT_unitHasStrings, 0},
        {"Sample", 0, 1, 0, kNT_unitConfirm, 0},
        {"Pitch", -10000, 10000, 0, kNT_unitPercent, kNT_scaling100},
        {"Stretch", -10000, 10000, 0, kNT_unitPercent, kNT_scaling100},
        {"Threshold", 0, 100, 22, kNT_unitPercent, 0},
        {"Grain Size", 32, 4096, 128, kNT_unitNone, 0},
        {"Quality", 0, 100, 100, kNT_unitPercent, 0},
        {"Feedback", 0, 150, 0, kNT_unitPercent, 0},
        {"Envelope Smoothing", 0, 10000, 4259, kNT_unitPercent, kNT_scaling100},
        {"Fade", 0, 10000, 2153, kNT_unitPercent, kNT_scaling100},
        {"Drive", 0, 10000, 1429, kNT_unitPercent, kNT_scaling100},
        {"Drive Character", 0, 10000, 10000, kNT_unitPercent, kNT_scaling100},
        {"Mix", 0, 100, 100, kNT_unitPercent, 0},
        {"Feedback Tone", 0, 10000, 3769, kNT_unitPercent, kNT_scaling100},
        {"Input Transient output", 0, 64, 0, kNT_unitCvOutput, 0},
        {"Output Transient output", 0, 64, 0, kNT_unitCvOutput, 0},
        {"Input Envelope output", 0, 64, 0, kNT_unitCvOutput, 0},
        {"Output Envelope output", 0, 64, 0, kNT_unitCvOutput, 0},
        {"Input Gain", -60, 0, 0, kNT_unitDb, kNT_scalingNone},
    };
    if (plugin.requirements.numParameters != 26 || plugin.values[9] != 0 ||
        plugin.values[10] != 0) return false;
    for (int i = 0; i < 26; ++i) {
        const auto& actual = plugin.algorithm->parameters[i];
        const auto& expected = released[i];
        if (std::strcmp(actual.name, expected.name) != 0 ||
            actual.min != expected.min || actual.max != expected.max ||
            actual.def != expected.def || actual.unit != expected.unit ||
            actual.scaling != expected.scaling) return false;
        if (i == 6) {
            if (std::strcmp(actual.enumStrings[0], "Live") != 0 ||
                std::strcmp(actual.enumStrings[1], "Sample") != 0) return false;
        } else if (actual.enumStrings != nullptr) return false;
    }
    const uint8_t performance[] = {25,9,10,11,12,13,14,15,16,17,18,19,20};
    const uint8_t source[] = {6,7,8};
    const uint8_t routing[] = {0,1,2,3,4,5,21,22,23,24};
    const uint8_t* indices[] = {performance, source, routing};
    const int counts[] = {13,3,10};
    const char* names[] = {"Performance", "Source", "Routing"};
    const auto* pages = plugin.algorithm->parameterPages;
    if (pages->numPages != 3) return false;
    for (int i = 0; i < 3; ++i) {
        const auto& page = pages->pages[i];
        if (std::strcmp(page.name, names[i]) || page.group != 0 ||
            page.numParams != counts[i] ||
            std::memcmp(page.params, indices[i], counts[i]) != 0) return false;
    }
    return true;
}

bool testUnversionedPresetMigratesToNewAudioAndCvBehavior() {
    for (int source : {0, 1}) {
      // Original releases saved no custom data. Exercise a skipped callback,
      // as well as empty custom data before or after generic parameters.
      for (int customOrder : {0, 1, 2}) {
        HostPlugin restored(false), reference(false);
        if (!reference.useNativeFormat()) return false;
        // Literal v0.5.3 vector, copied into a newly defaulted factory instance.
        const int16_t preset[] = {
            1,0,13,1,14,1,static_cast<int16_t>(source),0,1,35,70,100,
            128,92,45,4259,2153,1429,5000,100,3769,0,0,0,0,-6,
        };
        HostJson unversioned;
        if (customOrder == 1 && !restored.restore(unversioned, true)) return false;
        std::copy(preset, preset + 26, restored.values.begin());
        std::copy(preset, preset + 26, restored.baseValues.begin());
        for (int i = 0; i < 26; ++i) {
            restored.activate();
            restored.factory->parameterChanged(restored.algorithm, i);
            const int value = i == 9 ? capicola_nt::pitchValueFromLegacy(preset[i]) :
                i == 10 ? capicola_nt::stretchValueFromLegacy(preset[i]) : preset[i];
            if (i != 6) reference.set(reference.algorithm->parameters[i].name, value);
        }
        reference.set("Source", source);
        if (customOrder == 2 && !restored.restore(unversioned)) return false;
        // The reference is a fresh format-2 instance configured with the new
        // bipolar values. Restored presets must use exactly this behavior.
        double energy = 0.0;
        for (int block = 0; block < 500; ++block) {
            const int delta = block / 100 - 2;
            restored.map("Pitch", delta * 40);
            restored.map("Stretch", delta * 30);
            reference.map("Pitch", delta * 40);
            reference.map("Stretch", delta * 30);
            for (int i = 0; i < kFrames; ++i) {
                restored.buses[i] = reference.buses[i] = 3.0f * std::sin(
                    2.0f * kPi * 237.0f * (block * kFrames + i) / 48000.0f);
            }
            restored.step();
            reference.step();
            for (int i = 0; i < kFrames; ++i) {
                if (restored.output(i) != reference.output(i)) return false;
                energy += std::fabs(restored.output(i));
            }
        }
        if (energy < 100.0 || restored.baseValues[9] !=
            capicola_nt::pitchValueFromLegacy(35) || restored.baseValues[10] != 4000 ||
            restored.save().members.size() != 1)
            return false;
      }
    }
    return true;
}

bool testPresetFormatRoundTripsAndRejectsMalformedData() {
    HostPlugin fresh(false);
    HostJson saved = fresh.save();
    if (saved.members.size() != 1 || saved.members[0].name != "capicolaFormatVersion" ||
        saved.members[0].value != 1) return false;
    // A save before first operation preserves its old-domain initialization.
    if (!fresh.restore(saved, true)) return false;
    fresh.step();
    if (fresh.baseValues[9] != 0 || fresh.baseValues[10] != 0 ||
        fresh.save().members[0].value != 2) return false;
    for (bool customFirst : {false, true}) {
        HostPlugin legacy(false), reloaded(false);
        legacy.set("Pitch", -120);
        legacy.set("Stretch", 0);
        HostJson missing;
        missing.object = false;
        if (!legacy.restore(missing)) return false;
        legacy.step();
        saved = legacy.save();
        if (saved.members.size() != 1 || saved.members[0].value != 2 ||
            legacy.baseValues[9] != -10000 || legacy.baseValues[10] != -10000 ||
            legacy.algorithm->parameters[10].max != 10000) return false;
        if (customFirst && !reloaded.restore(saved, true)) return false;
        reloaded.values = legacy.values;
        reloaded.baseValues = legacy.baseValues;
        if (!customFirst && !reloaded.restore(saved)) return false;
        reloaded.step();
        if (reloaded.baseValues != legacy.baseValues ||
            reloaded.save().members.size() != 1) return false;
        // Converted presets use the same signed range and CV response as new
        // instances, with no compatibility data remaining after conversion.
        legacy.map("Stretch", 50);
        reloaded.map("Stretch", 50);
        if (reloaded.values[10] != -9950) return false;
        legacy.step(); reloaded.step();
        reloaded.set("Pitch", -5000);
        reloaded.step();
        HostJson edited = reloaded.save();
        if (edited.members.size() != 1) return false;
        reloaded.set("Stretch", -5000);
        reloaded.step();
        if (reloaded.save().members.size() != 1 ||
            reloaded.algorithm->parameters[10].max != 10000) return false;
    }
    const HostJson malformed[] = {
        {true, {{"capicolaFormatVersion", 3}}, ""},
        {true, {{"capicolaFormatVersion", 0}}, ""},
        {true, {{"capicolaFormatVersion", 2, false}}, ""},
        {true, {{"capicolaFormatVersion", 2}, {"capicolaFormatVersion", 2}}, ""},
        {true, {{"capicolaFormatVersion", 999}}, ""},
        {true, {{"capicolaFormatVersion", 1}, {"capicolaFormatVersion", 2}}, ""},
    };
    for (HostJson json : malformed) {
        if (fresh.restore(json, true)) return false;
        const HostJson unchanged = fresh.save();
        if (unchanged.members.size() != 1 || unchanged.members[0].value != 2)
            return false;
    }
    HostJson withUnknown{true, {{"futureField", 123, false}, {"capicolaFormatVersion", 2}}, ""};
    return fresh.restore(withUnknown, true);
}

bool testDefaultOldFormatAndSaveBeforeFirstStep() {
    // Fresh and old loaded values can both be saved while loading is inactive.
    // Every save must identify the range actually used by its BASE values.
    for (bool oldPreset : {false, true}) {
      for (bool initialized : {false, true}) {
        HostPlugin savedPlugin(false);
        if (oldPreset) {
            savedPlugin.set("Pitch", 35);
            savedPlugin.set("Stretch", 70);
        }
        if (initialized) savedPlugin.step();
        const std::vector<int16_t> savedBases = savedPlugin.baseValues;
        HostJson saved = savedPlugin.save();
        if (saved.members.size() != 1 ||
            saved.members[0].name != "capicolaFormatVersion" ||
            saved.members[0].value != (initialized ? 2 : 1)) return false;
        for (bool customFirst : {false, true}) {
            HostPlugin reloaded(false);
            if (customFirst && !reloaded.restore(saved, true)) return false;
            if (!reloaded.loadParameters(savedBases)) return false;
            if (!customFirst && !reloaded.restore(saved)) return false;
            const uint32_t writes = gAudioParameterWrites;
            const uint32_t definitions = gParameterDefinitionUpdates;
            reloaded.step();
            if (reloaded.baseValues[9] != (oldPreset ? 2917 : 0) ||
                reloaded.baseValues[10] != (oldPreset ? 4000 : 0) ||
                reloaded.values != reloaded.baseValues ||
                reloaded.algorithm->parameters[9].def != 0 ||
                reloaded.algorithm->parameters[10].def != 0 ||
                gAudioParameterWrites != writes + (initialized ? 0 : 2) ||
                gParameterDefinitionUpdates != definitions + 1 ||
                reloaded.save().members[0].value != 2) return false;
            reloaded.step();
            if (gAudioParameterWrites != writes + (initialized ? 0 : 2) ||
                gParameterDefinitionUpdates != definitions + 1) return false;
        }
      }
    }
    // The tag overrides OLD even when native numbers overlap the old range.
    // Normalizing the public default must not alter any restored native BASE.
    for (bool customFirst : {false, true}) {
        HostPlugin tagged(false);
        HostJson native{true, {{"capicolaFormatVersion", 2}}, ""};
        if (customFirst && !tagged.restore(native, true)) return false;
        tagged.set("Pitch", -4500);
        tagged.set("Stretch", 70);
        if (!customFirst && !tagged.restore(native)) return false;
        const uint32_t writes = gAudioParameterWrites;
        tagged.step();
        if (tagged.baseValues[9] != -4500 || tagged.baseValues[10] != 70 ||
            tagged.algorithm->parameters[10].def != 0 ||
            gAudioParameterWrites != writes) return false;
        // Source/processor resets preserve the completed format. Default resets
        // subsequently use native center values and never convert again.
        tagged.set("Source", 1); tagged.step();
        tagged.set("Source", 0); tagged.step();
        tagged.set("Pitch", tagged.algorithm->parameters[9].def);
        tagged.set("Stretch", tagged.algorithm->parameters[10].def);
        tagged.step();
        if (tagged.baseValues[9] != 0 || tagged.baseValues[10] != 0 ||
            gAudioParameterWrites != writes || tagged.save().members[0].value != 2)
            return false;
    }
    return true;
}

bool testMigrationUsesBaseValuesAndCommonOffset() {
    HostPlugin plugin(false), reference(false);
    if (!reference.useNativeFormat()) return false;
    // Loading restores BASE and custom data while the plugin is inactive.
    // The first valid step initializes the ranges before normal processing.
    plugin.set("Pitch", 35);
    plugin.set("Stretch", 20);
    HostJson unversioned;
    if (!plugin.restore(unversioned, true)) return false;
    plugin.map("Pitch", 30);
    plugin.map("Stretch", 10);
    reference.set("Pitch", capicola_nt::pitchValueFromLegacy(35));
    reference.set("Stretch", capicola_nt::stretchValueFromLegacy(20));
    reference.map("Pitch", 30);
    reference.map("Stretch", 10);
    gParameterOffset = 7;
    const uint32_t writes = gAudioParameterWrites;
    for (int block = 0; block < 24; ++block) {
        for (int i = 0; i < kFrames; ++i) {
            plugin.buses[i] = reference.buses[i] = 3.0f * std::sin(
                2.0f * kPi * 237.0f * (block * kFrames + i) / 48000.0f);
        }
        plugin.step(); reference.step();
        if (gAudioParameterWrites != writes + 2 || gLastAudioParameter != 17 ||
            plugin.baseValues[9] != capicola_nt::pitchValueFromLegacy(35) ||
            plugin.baseValues[10] != -6000 || plugin.values[9] != 2947 ||
            plugin.values[10] != -5990) return false;
        for (int i = 0; i < kFrames; ++i) {
            if (plugin.output(i) != reference.output(i) ||
                plugin.output(i, 1) != reference.output(i, 1)) return false;
        }
    }
    gParameterOffset = 0;
    const HostJson saved = plugin.save();
    return saved.members.size() == 1 && saved.members[0].value == 2;
}

bool testBipolarMappedValuesApplyWithoutCallbacks() {
    for (int source : {0, 1}) {
        for (const char* control : {"Pitch", "Stretch"}) {
            HostPlugin notified, mapped, centered;
            for (HostPlugin* plugin : {&notified, &mapped, &centered}) {
                plugin->set("Right input", 0);
                plugin->set("Threshold", 100);
                plugin->set("Drive Character", 5000);
                plugin->set("Source", source);
            }
            double difference = 0.0;
            const int positions[] = {-10000, 0, -2500, 5000, 10000};
            for (int block = 0; block < 1600; ++block) {
                if (block >= 400 && block % 200 == 0) {
                    const int position = positions[((block - 400) / 200) % 5];
                    notified.set(control, position);
                    mapped.values[mapped.parameter(control)] = position;
                }
                for (int i = 0; i < kFrames; ++i) {
                    const float input = 3.0f * std::sin(
                        2.0f * kPi * 237.0f * (block * kFrames + i) / 48000.0f);
                    for (HostPlugin* plugin : {&notified, &mapped, &centered}) {
                        plugin->buses[i] = input;
                    }
                }
                notified.step(); mapped.step(); centered.step();
                for (int i = 0; i < kFrames; ++i) {
                    if (!std::isfinite(mapped.output(i)) ||
                        mapped.output(i) != notified.output(i) ||
                        mapped.output(i) != mapped.output(i, 1)) return false;
                    difference += std::fabs(mapped.output(i) - centered.output(i));
                }
            }
            if (difference < 100.0) return false;
        }
    }
    return true;
}

bool testEditsAfterMigrationRemainNative() {
    for (bool useCustomUi : {false, true}) {
        HostPlugin edited(false), restored(false);
        edited.set("Pitch", 120);
        edited.set("Stretch", 100);
        HostJson unversioned;
        if (!edited.restore(unversioned, true)) return false;
        const uint32_t writes = gAudioParameterWrites;
        edited.step(); // Parameter loading is complete before operation.
        if (edited.baseValues[9] != 10000 || edited.baseValues[10] != 10000 ||
            gAudioParameterWrites != writes + 2) return false;
        if (useCustomUi) {
            _NT_uiData bank{};
            bank.controls = kNT_potButtonL;
            bank.pots[0] = 1.0f;
            edited.ui(bank);
            _NT_uiData edit{};
            edit.controls = kNT_potL;
            edit.pots[0] = 1.0f;
            edited.ui(edit); // Same value as the completed conversion.
            edit.pots[0] = 0.25f;
            edited.ui(edit); // New native raw-5000.
        } else {
            edited.set("Pitch", 10000);
            edited.set("Pitch", -5000);
        }
        edited.map("Pitch", 200);
        edited.step();
        if (edited.baseValues[9] != -5000 || edited.values[9] != -4800 ||
            gAudioParameterWrites != writes + 2) return false;
        HostJson saved = edited.save();
        if (saved.members.size() != 1 || saved.members[0].value != 2) return false;
        restored.values = edited.values;
        restored.baseValues = edited.baseValues;
        if (!restored.restore(saved, true)) return false;
        restored.step();
        if (restored.baseValues[9] != -5000 || restored.values[9] != -4800 ||
            restored.baseValues[10] != 10000 ||
            gAudioParameterWrites != writes + 2) return false;
    }
    return true;
}

bool testTransientBusPulseTimingAndUnavailableSource() {
    HostPlugin plugin;
    plugin.set("Threshold", 100);
    plugin.set("Input Transient output", 15);
    plugin.step();
    _NT_uiData slice{};
    slice.controls = kNT_encoderButtonR;
    plugin.ui(slice);
    for (int block = 0; block < 9; ++block) {
        if (block == 2) { plugin.ui({}); plugin.ui(slice); }
        plugin.step();
        for (int i = 0; i < kFrames; ++i) {
            if (plugin.buses[14 * kFrames + i] !=
                (block * kFrames + i < 480 ? 5.0f : 0.0f)) {
                std::printf("pulse mismatch block %d frame %d: %f\n", block, i,
                            plugin.buses[14 * kFrames + i]);
                return false;
            }
        }
    }
    // Missing Sample resets and silences CV, including a pending manual slice.
    plugin.ui({}); plugin.ui(slice);
    gStreamInitialEmptyRenders = 100;
    plugin.set("Source", 1);
    for (int block = 0; block < 10; ++block) {
        plugin.step();
        for (int i = 0; i < kFrames; ++i) {
            if (plugin.buses[14 * kFrames + i] != 0.0f) return false;
        }
    }
    return true;
}

bool testTransientPulseSpansWarmHandoffAndShortReads() {
    HostPlugin plugin;
    plugin.set("Threshold", 100);
    plugin.set("Input Transient output", 15);
    plugin.set("Source", 1);
    for (int block = 0; block < 100; ++block) plugin.step();
    plugin.set("Sample", 1);
    for (int block = 0; block < 37; ++block) plugin.step();
    // The next 64-frame block splits into two 32-frame handoff segments.
    _NT_uiData slice{};
    slice.controls = kNT_encoderButtonR;
    plugin.ui(slice);
    for (int block = 0; block < 9; ++block) {
        plugin.step();
        for (int i = 0; i < kFrames; ++i) {
            if (plugin.buses[14 * kFrames + i] !=
                (block * kFrames + i < 480 ? 5.0f : 0.0f)) return false;
        }
    }

    HostPlugin shortRead;
    gSampleFrameCount = 16;
    shortRead.set("Threshold", 100);
    shortRead.set("Input Transient output", 15);
    shortRead.set("Output Transient output", 16);
    shortRead.set("Source", 1);
    shortRead.step();
    shortRead.ui(slice);
    for (int block = 0; block < 9; ++block) {
        shortRead.step();
        // The first block consumed the file and one loop; later blocks can
        // reopen it once, yielding 16 valid frames and 48 unavailable frames.
        for (int i = 0; i < kFrames; ++i) {
            const float expected = block * kFrames + i < 480 && i < 16 ? 5.0f : 0.0f;
            if (shortRead.buses[14 * kFrames + i] != expected ||
                shortRead.buses[15 * kFrames + i] != 0.0f) {
                std::printf("short-read pulse block %d frame %d: in=%f expected=%f out=%f\n",
                            block, i, shortRead.buses[14 * kFrames + i], expected,
                            shortRead.buses[15 * kFrames + i]);
                return false;
            }
        }
    }
    return true;
}

bool testBipolarDisplayShowsEffectiveRates() {
    HostPlugin plugin;
    const auto draw = [&]() {
        plugin.activate();
        gDrawnText.clear();
        plugin.factory->draw(plugin.algorithm);
    };
    plugin.set("Stretch", -10000);
    draw();
    if (!drawnTextContains("-1.0x")) return false;
    plugin.set("Stretch", 0);
    draw();
    if (!drawnTextContains("FREEZE")) return false;
    _NT_uiData bank{};
    bank.controls = kNT_potButtonL;
    plugin.ui(bank);
    plugin.set("Pitch", -10000);
    draw();
    if (!drawnTextContains("-2.00x")) return false;
    plugin.set("Pitch", 0);
    draw();
    if (!drawnTextContains("HOLD")) return false;
    plugin.set("Pitch", 5000);
    draw();
    return drawnTextContains("+1.00x");
}

bool testInactiveFolderPreservesLiveAudio() {
    HostPlugin uninterrupted, browsing;
    for (HostPlugin* plugin : {&uninterrupted, &browsing}) {
        plugin->set("Right input", 0);
        plugin->set("Stretch", 2000);
        plugin->set("Threshold", 100);
        plugin->set("Drive Character", 5000);
        plugin->set("Grain Size", 4096);
        plugin->set("Feedback", 50);
    }
    for (int block = 0; block < 532; ++block) {
        if (block == 500) browsing.set("Folder", 1);
        if (block == 510 || block == 520) {
            gCardMounted = block == 520;
            browsing.set("Folder", 0);
        }
        for (int i = 0; i < kFrames; ++i) {
            const float input = 3.0f * std::sin(
                2.0f * kPi * 220.0f * (block * kFrames + i) / 48000.0f);
            uninterrupted.buses[i] = browsing.buses[i] = input;
        }
        uninterrupted.step();
        browsing.step();
        for (int i = 0; i < kFrames; ++i) {
            if (uninterrupted.output(i) != browsing.output(i)) return false;
        }
    }
    return true;
}

bool testPotBankRequiresPickup() {
    HostPlugin plugin(false);
    plugin.step();
    _NT_uiData event{};
    event.controls = kNT_potL;
    plugin.ui(event); // MAIN: Stretch at its physical minimum.
    event = {};
    event.controls = kNT_potButtonL | kNT_potL;
    event.pots[0] = 0.001f;
    event.pots[1] = 0.8f;
    plugin.ui(event); // Press jitter must not write ALT Pitch.
    if (plugin.values[plugin.parameter("Pitch")] != 0) return false;
    event.controls = kNT_potL;
    event.pots[0] = 0.1f;
    plugin.ui(event);
    if (plugin.values[plugin.parameter("Pitch")] != 0) return false;
    event.pots[0] = 0.6f; // Cross the saved midpoint in one update.
    plugin.ui(event);
    if (plugin.values[plugin.parameter("Pitch")] != 2000) return false;
    event.pots[0] = 0.7f;
    plugin.ui(event);
    if (plugin.values[plugin.parameter("Pitch")] != 4000) return false;
    event.controls = kNT_potC;
    event.pots[1] = 0.9f; // Other pots remain independently locked.
    plugin.ui(event);
    if (plugin.values[plugin.parameter("Grain Size")] != 128) return false;
    event.controls = kNT_potButtonC;
    plugin.ui(event);
    event.controls = kNT_potL;
    event.pots[0] = 0.6f;
    plugin.ui(event); // Switching back must pick up MAIN again.
    if (plugin.values[plugin.parameter("Stretch")] != -10000) return false;
    event.pots[0] = 0.0f;
    plugin.ui(event);
    event.pots[0] = 0.1f;
    plugin.ui(event);
    return plugin.values[plugin.parameter("Stretch")] == -8000;
}

bool testShorterPhysicalSamplesKeepLooping() {
    for (uint32_t actualFrames : {128U, 149U}) {
        HostPlugin plugin;
        gSampleFrameCount = 4096;
        gOpenedStreamFrameCount = actualFrames;
        plugin.set("Mix", 0);
        plugin.set("Source", 1);
        double lateEnergy = 0.0;
        for (int block = 0; block < 320; ++block) {
            plugin.step();
            if (block >= 150) {
                for (int i = 0; i < kFrames; ++i) {
                    lateEnergy += std::fabs(plugin.output(i));
                }
            }
        }
        if (lateEnergy < 100.0) return false;
    }
    return true;
}

bool testSmallSampleCanWaitForItsFirstFrames() {
    HostPlugin plugin;
    gSampleFrameCount = 16;
    gStreamInitialEmptyRenders = 100;
    plugin.set("Mix", 0);
    plugin.set("Source", 1);
    double energy = 0.0;
    for (int block = 0; block < 250; ++block) {
        plugin.step();
        for (int i = 0; i < kFrames; ++i) energy += std::fabs(plugin.output(i));
    }
    return energy > 1.0;
}

bool testSourceChangesPreserveOutputContinuity() {
    HostPlugin plugin;
    plugin.set("Mix", 0);
    // A sustained voltage isolates the switching discontinuity from waveform slew.
    std::fill(plugin.buses.begin(), plugin.buses.begin() + 2 * kFrames, 4.0f);
    plugin.step();
    float previous = plugin.output(kFrames - 1);
    plugin.set("Source", 1);
    plugin.step();
    if (std::fabs(plugin.output(0) - previous) > 0.05f) return false;
    for (int block = 0; block < 100; ++block) plugin.step();
    previous = plugin.output(kFrames - 1);
    std::fill(plugin.buses.begin(), plugin.buses.begin() + 2 * kFrames, -4.0f);
    plugin.set("Source", 0);
    plugin.step();
    if (std::fabs(plugin.output(0) - previous) > 0.05f) return false;
    for (int block = 0; block < 10; ++block) plugin.step();
    if (plugin.output(kFrames - 1) != -4.0f) return false;
    // Two selections before the next audio block still start at the audible voltage.
    plugin.set("Source", 1);
    plugin.set("Source", 0);
    plugin.step();
    return std::fabs(plugin.output(0) + 4.0f) < 0.05f;
}

bool testSourceBridgeRemainsSmoothWhileSampleWaits() {
    for (bool makeUnavailable : {false, true}) {
        HostPlugin plugin;
        plugin.set("Mix", 0);
        std::fill(plugin.buses.begin(), plugin.buses.begin() + 2 * kFrames, 4.0f);
        plugin.step();
        gStreamInitialEmptyRenders = 100;
        plugin.set("Source", 1);
        float previous = 4.0f;
        for (int block = 0; block < 60; ++block) {
            if (makeUnavailable && block == 1) plugin.set("Folder", 99);
            plugin.step();
            for (int i = 0; i < kFrames; ++i) {
                const float sample = plugin.output(i);
                if (sample > previous || std::fabs(sample - previous) > 0.05f) return false;
                previous = sample;
            }
        }
        if (previous != 0.0f) return false;
    }
    return true;
}

bool testBriefUnderrunsResumeAtTheSameAudioFrame() {
    HostPlugin paused, interrupted;
    for (HostPlugin* plugin : {&paused, &interrupted}) {
        plugin->set("Mix", 0);
        plugin->set("Source", 1);
    }
    for (int block = 0; block < 100; ++block) {
        paused.step();
        interrupted.step();
    }
    for (int interruption = 0; interruption < 4; ++interruption) {
        gCardMounted = false;
        for (int block = 0; block < 40; ++block) interrupted.step();
        gCardMounted = true;
        paused.step();
        interrupted.step();
        for (int i = 0; i < kFrames; ++i) {
            if (paused.output(i) != interrupted.output(i)) return false;
        }
    }
    return true;
}

bool testSliceDuringSdStartupPreservesWetAudio() {
    HostPlugin reference, sliced;
    gStreamInitialEmptyRenders = 100;
    for (HostPlugin* plugin : {&reference, &sliced}) {
        plugin->set("Pitch", 10000);
        plugin->set("Source", 1);
    }
    double energy = 0.0;
    for (int block = 0; block < 400; ++block) {
        if (block == 50) {
            _NT_uiData event{};
            event.controls = kNT_encoderButtonR;
            sliced.ui(event);
        }
        reference.step();
        sliced.step();
        for (int i = 0; i < kFrames; ++i) {
            if (reference.output(i) != sliced.output(i)) return false;
            energy += std::fabs(sliced.output(i));
        }
    }
    return energy > 100.0;
}

bool testDelayedSampleFadesFromItsFirstAudibleFrame() {
    HostPlugin plugin;
    gConstantSample = true;
    gStreamInitialEmptyRenders = 100;
    plugin.set("Mix", 0);
    plugin.set("Source", 1);
    for (int block = 0; block < 100; ++block) plugin.step();
    plugin.step();
    float previous = 0.0f;
    for (int i = 0; i < kFrames; ++i) {
        const float sample = plugin.output(i);
        if (sample < previous || sample - previous > 0.01f) return false;
        previous = sample;
    }
    for (int block = 0; block < 80; ++block) plugin.step();
    return previous > 0.0f && plugin.output(kFrames - 1) == 4.0f;
}

} // namespace

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
    if (requirements.dram >= 1024U * 1024U ||
        requirements.dtc < 2U * NT_globals.streamSizeBytes) {
        return fail("sample source did not reserve two fixed streaming slots");
    }
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
    const int inputGain = findParameter(algorithm, count, "Input Gain");
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
        driveCharacter < 0 || mix < 0 || feedbackTone < 0 || inputGain < 0 ||
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
        inputGain,
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
    const _NT_parameter& inputGainDefinition =
        algorithm->parameters[inputGain];
    if (inputGain != 25 || inputGainDefinition.min != -60 ||
        inputGainDefinition.max != 0 || inputGainDefinition.def != 0 ||
        inputGainDefinition.unit != kNT_unitDb ||
        inputGainDefinition.scaling != kNT_scalingNone) {
        return fail("Input Gain is not an appended attenuation-only dB control");
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
    values[pitch] = 5000;
    values[stretch] = 10000;
    algorithm->v = values.data();
    HostJson nativeState{true, {{"capicolaFormatVersion", 2}}, ""};
    _NT_jsonParse nativeParse(&nativeState, 0);
    if (!factory->deserialise(algorithm, nativeParse)) {
        return fail("native audio fixture could not load its format tag");
    }

    // The host persists ordinary parameter values in its preset. Recreate a
    // fresh instance with those values already restored, including Sample
    // mode. Host parameter callbacks restore the valid resource before the
    // first audio step; step itself must do no catalogue or sample-read work.
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
    _NT_jsonParse restoredParse(&nativeState, 0);
    if (!factory->deserialise(restored, restoredParse)) {
        return fail("native restored fixture could not load its format tag");
    }
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
    double restoredEnergy = 0.0;
    for (int i = 0; i < kFrames; ++i) {
        restoredEnergy += std::fabs(restoredBuses[12 * kFrames + i]) +
                          std::fabs(restoredBuses[13 * kFrames + i]);
    }
    if (gStreamOpenCalls != opensBeforePresetRestore + 1 ||
        gOpenedFolder != 0 || gOpenedSample != 1 ||
        restoredEnergy == 0.0 || restoredValues[source] != 1 ||
        gFolderInfoCalls != folderCallsBeforeRestoredStep ||
        gFileInfoCalls != fileCallsBeforeRestoredStep ||
        gParameterDefinitionUpdates != definitionUpdatesBeforeRestoredStep + 1 ||
        restored->parameters[stretch].def != 0) {
        return fail("preset restore did not load its sample exactly once");
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
        !drawnTextContains("FEEDBACK") || !drawnTextContains("1.0x") ||
        !drawnTextContains("MIX 100%") || drawnTextContains("MAIN") ||
        drawnTextContains("IN ") || drawnTextContains("OUT ")) {
        return fail("performance screen did not use the simplified live layout");
    }

    _NT_uiData ui{};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.5f;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (values[stretch] != 0 || !drawnTextContains("FREEZE")) {
        return fail("Stretch midpoint did not show freeze");
    }
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 1.0f;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (values[stretch] != 10000 || !drawnTextContains("1.0x")) {
        return fail("maximum Stretch did not show forward unity");
    }
    ui = {};
    ui.controls = kNT_potButtonL;
    ui.pots[0] = 1.0f;
    factory->customUi(algorithm, ui);
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.5f; // Pick up ALT Pitch's saved centre before adjusting it.
    factory->customUi(algorithm, ui);
    ui.pots[0] = 0.75f;
    factory->customUi(algorithm, ui);
    if (values[pitch] != 5000) {
        return fail("alternate performance pot did not control Pitch");
    }
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("PITCH") ||
        !drawnTextContains("+1.00x") || !drawnTextContains("GRAIN") ||
        !drawnTextContains("QUALITY") || drawnTextContains("ALT")) {
        return fail("alternate pot functions did not show their identity and value");
    }
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.25f;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (values[pitch] != -5000 || !drawnTextContains("-1.00x")) {
        return fail("negative Pitch value did not update on the performance screen");
    }
    // The NT may publish an NT_setParameterFromUi() value after the custom UI
    // draw that follows the physical pot event. The screen must show the pot's
    // new Pitch immediately instead of waiting on the host value commit.
    gDeferParameterUiCommit = true;
    ui = {};
    ui.controls = kNT_potL;
    ui.pots[0] = 0.625f;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (values[pitch] != -5000 || !drawnTextContains("+0.50x") ||
        gPendingParameter != pitch || gPendingParameterValue != 2500) {
        return fail("Pitch display waited for the host pot-value commit");
    }
    gDeferParameterUiCommit = false;
    const_cast<int16_t*>(algorithm->v)[gPendingParameter] =
        gPendingParameterValue;
    factory->parameterChanged(algorithm, gPendingParameter);
    gPendingParameter = -1;
    // Every pressable pot switches the shared performance bank; the three
    // visible control names identify the bank without a redundant footer tag.
    ui = {};
    ui.controls = kNT_potButtonC;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("STRETCH") || drawnTextContains("MAIN")) {
        return fail("centre pot press did not restore the visible main bank");
    }
    ui = {};
    ui.controls = kNT_potButtonR;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("QUALITY") || drawnTextContains("ALT")) {
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
    values[pitch] = 5000;
    values[stretch] = 10000;
    values[mix] = 100;
    factory->parameterChanged(algorithm, pitch);
    factory->parameterChanged(algorithm, stretch);
    factory->parameterChanged(algorithm, mix);

    std::vector<float> buses(kNT_lastBus * kFrames, 0.0f);
    // Differentially compare the complete host wrapper against the same
    // upstream processor fed normalized audio. A 4 V NT signal must arrive at
    // Capicola as 0.8 normalized and return to the bus at 5 V per normalized
    // unit. This catches accidental volt-domain overdrive without asserting a
    // new limiter or otherwise changing the DSP.
    {
        using ReferenceProcessor =
            capicola_nt::CapicolaStereoLivePath<16384>;
        values[threshold] = 100;
        values[grain] = 32;
        values[quality] = 0;
        values[envelopeSmoothing] = 0;
        values[fade] = 0;
        values[drive] = 0;
        values[driveCharacter] = 5000;
        values[feedbackTone] = 0;
        auto reference = std::make_unique<ReferenceProcessor>();
        reference->init();
        reference->setPitchSemitones(0.0f);
        reference->setStretch(1.0f);
        reference->setTransientThreshold(1.0e9f);
        reference->setGrainSize(32);
        reference->setQuality(0.1f);
        reference->setFeedback(0.0f);
        reference->setEnvelopeSmoothing(5.0e-5f);
        reference->setFade(480.0f);
        reference->setDrive(0.5f);
        reference->setDriveCharacter(0.5f);
        reference->setMix(1.0f);
        reference->setFeedbackTone(2.0e-3f);

        std::vector<float> normalizedInput(kFrames, 0.0f);
        std::vector<float> referenceLeft(kFrames, 0.0f);
        std::vector<float> referenceRight(kFrames, 0.0f);
        int levelClock = 0;
        for (int block = 0; block < 320; ++block) {
            std::fill(buses.begin(), buses.end(), 0.0f);
            for (int i = 0; i < kFrames; ++i, ++levelClock) {
                normalizedInput[i] = 0.8f * std::sin(
                    2.0f * kPi * 220.0f * levelClock / 48000.0f);
                buses[i] = normalizedInput[i] * 5.0f;
            }
            reference->process(normalizedInput.data(), nullptr,
                               referenceLeft.data(), referenceRight.data(),
                               kFrames);
            factory->step(algorithm, buses.data(), kFrames / 4);
            for (int i = 0; i < kFrames; ++i) {
                const float expectedLeft = referenceLeft[i] * 5.0f;
                const float expectedRight = referenceRight[i] * 5.0f;
                if (std::fabs(buses[12 * kFrames + i] - expectedLeft) >
                        1.0e-5f ||
                    std::fabs(buses[13 * kFrames + i] - expectedRight) >
                        1.0e-5f) {
                    return fail("NT volts were not normalized around Capicola DSP");
                }
            }
        }
        for (int parameter : auditedControls) {
            values[parameter] = algorithm->parameters[parameter].def;
        }
        values[pitch] = 5000;
        values[stretch] = 10000;
    }
    std::fill(buses.begin(), buses.end(), 0.0f);

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

    // Capicola's upstream processor uses normalized audio while disting NT
    // buses use volts. Exercise the real wrapper boundary with a full-scale
    // +/-5 V signal: 0 dB must remain unity at dry Mix, and -12 dB must
    // provide predictable headroom without changing the DSP implementation.
    values[mix] = 0;
    values[inputGain] = 0;
    for (int i = 0; i < kFrames; ++i) {
        buses[i] = (i & 1) == 0 ? 5.0f : -5.0f;
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    for (int i = 0; i < kFrames; ++i) {
        const float expected = (i & 1) == 0 ? 5.0f : -5.0f;
        if (std::fabs(buses[12 * kFrames + i] - expected) > 1.0e-5f ||
            std::fabs(buses[13 * kFrames + i] - expected) > 1.0e-5f) {
            return fail("0 dB Input Gain did not preserve dry NT audio level");
        }
    }

    values[inputGain] = -12;
    for (int block = 0; block < 120; ++block) {
        for (int i = 0; i < kFrames; ++i) {
            buses[i] = (i & 1) == 0 ? 5.0f : -5.0f;
        }
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    const float expectedAttenuated = 5.0f * std::exp2(-12.0f / 6.020599913f);
    for (int i = 0; i < kFrames; ++i) {
        const float expected = (i & 1) == 0
            ? expectedAttenuated : -expectedAttenuated;
        if (std::fabs(buses[12 * kFrames + i] - expected) > 1.0e-3f ||
            std::fabs(buses[13 * kFrames + i] - expected) > 1.0e-3f) {
            return fail("Input Gain did not create the requested dry-path headroom");
        }
    }
    values[inputGain] = 0;
    values[mix] = 100;
    std::fill(buses.begin(), buses.end(), 0.0f);

    // Simulate SD/catalogue activity caused by opening an external Source panel
    // while audio is running. Even rapid host-mapped Source changes must keep
    // step() independent of card state, catalogue queries, definition updates,
    // and sample opens, and every produced frame must remain finite.
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
        return fail("audio step performed SD catalogue or sample-read work");
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
    }
    if (!sawInputTransient || !sawOutputTransient ||
        !sawInputEnvelope || !sawOutputEnvelope) {
        return fail("an assigned analysis signal did not reach its CV bus");
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
    const uint32_t readsBeforeSampleSource = gSampleReadCalls;
    ui = {};
    ui.encoders[0] = 1;
    factory->customUi(algorithm, ui);
    if (values[source] != 1 ||
        gStreamOpenCalls != opensBeforeSampleSource + 1 ||
        gOpenedFolder != 0 || gOpenedSample != 1) {
        return fail("entering Sample mode did not load the selected sample");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource + 1 ||
        !drawnTextContains("SELECT FOLDER") || !drawnTextContains("Drums") ||
        !drawnTextContains("PRESS: NEXT")) {
        return fail("sample mode did not open the temporary folder selection");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource + 1 ||
        !drawnTextContains("SELECT SAMPLE") ||
        !drawnTextContains(
            "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcd.wav.WAV") ||
        !drawnTextContains("PRESS: LOAD")) {
        return fail("folder confirmation did not open temporary sample selection");
    }
    ui = {};
    ui.controls = kNT_encoderButtonL;
    factory->customUi(algorithm, ui);
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeSampleSource + 2 ||
        gSampleReadCalls != readsBeforeSampleSource ||
        !drawnTextContains("CAPICOLA") ||
        !drawnTextAt("1234567890ABCDEF...RSTUVWXYZabcd",
                     72, kNT_textTiny) ||
        drawnTextContains(".wav") || drawnTextContains(".WAV")) {
        return fail("sample LOAD did not restart the compactly titled stream");
    }
    const uint32_t folderCallsBeforeSampleTitle = gFolderInfoCalls;
    const uint32_t fileCallsBeforeSampleTitle = gFileInfoCalls;
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("CAPICOLA") ||
        !drawnTextContains("1234567890ABCDEF...RSTUVWXYZabcd") ||
        gFolderInfoCalls != folderCallsBeforeSampleTitle ||
        gFileInfoCalls != fileCallsBeforeSampleTitle) {
        return fail("streamed sample did not retain its compact title");
    }
    auto pressSampleLoad = [&]() {
        for (int press = 0; press < 3; ++press) {
            _NT_uiData loadUi{};
            loadUi.controls = kNT_encoderButtonL;
            factory->customUi(algorithm, loadUi);
        }
    };
    auto settleSampleTransition = [&]() {
        // Each replacement uses a 50 ms (2400-frame) fade-out and a matching
        // fade-in. Seventy-five 64-frame blocks cover both phases at 48 kHz.
        for (int block = 0; block < 75; ++block) {
            factory->step(algorithm, buses.data(), kFrames / 4);
        }
    };

    // A pending selection opens without a full-file read. Returning to the
    // still-audible selection cancels that pending handoff.
    values[sample] = 0;
    const uint32_t opensBeforeReplacement = gStreamOpenCalls;
    factory->parameterChanged(algorithm, sample);
    values[sample] = 1;
    factory->parameterChanged(algorithm, sample);
    if (gStreamOpenCalls != opensBeforeReplacement + 1 ||
        gSampleReadCalls != readsBeforeSampleSource) {
        return fail("pending sample selection was not opened and cancelled");
    }
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("CAPICOLA") ||
        !drawnTextContains("1234567890ABCDEF...RSTUVWXYZabcd")) {
        return fail("replacement stream did not title the playing sample");
    }

    // A refused stream open leaves Sample mode safely waiting. A later
    // explicit LOAD action can recover.
    gStreamOpenSucceeds = false;
    pressSampleLoad();
    gDrawnText.clear();
    factory->draw(algorithm);
    if (!drawnTextContains("CAPICOLA   WAIT")) {
        return fail("failed stream open did not leave Sample mode waiting");
    }
    gStreamOpenSucceeds = true;
    pressSampleLoad();

    // Long samples use the same fixed stream memory and are not truncated or
    // copied into a construction-time full-file buffer.
    gSampleFrameCount = 48000U * 40U;
    const uint32_t opensBeforeLongStream = gStreamOpenCalls;
    pressSampleLoad();
    if (gStreamOpenCalls != opensBeforeLongStream + 1 ||
        gSampleReadCalls != readsBeforeSampleSource) {
        return fail("long sample did not use fixed-memory streaming");
    }
    gSampleFrameCount = 48000U;
    pressSampleLoad();

    // All five audited secondary controls must alter their intended linked
    // Capicola/feedback path while the selected sample is the sole source.
    // Reopening the sample resets both the stream position and engine, making each
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
        pressSampleLoad();
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
    pressSampleLoad();
    settleSampleTransition();

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
        const float expectedLeft = 8.0f * std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(dryClock + i) / 48000.0f);
        const float expectedRight = 8.0f * 0.35f * std::sin(
            2.0f * kPi * 710.0f * static_cast<float>(dryClock + i) / 48000.0f);
        if (std::fabs(dryLeft[i] - expectedLeft) > 1.0e-6f ||
            std::fabs(dryRight[i] - expectedRight) > 1.0e-6f) {
            return fail("host-mapped dry Mix value did not reach selected-sample processing");
        }
    }
    values[inputGain] = -12;
    uint32_t trimmedSampleClock = 0U;
    for (int block = 0; block < 120; ++block) {
        trimmedSampleClock = gStreamClock;
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    const float sampleTrim = std::exp2(-12.0f / 6.020599913f);
    for (int i = 0; i < kFrames; ++i) {
        const float expectedLeft = 8.0f * sampleTrim * std::sin(
            2.0f * kPi * 330.0f *
            static_cast<float>(trimmedSampleClock + i) / 48000.0f);
        const float expectedRight = 8.0f * sampleTrim * 0.35f * std::sin(
            2.0f * kPi * 710.0f *
            static_cast<float>(trimmedSampleClock + i) / 48000.0f);
        if (std::fabs(buses[12 * kFrames + i] - expectedLeft) > 1.0e-5f ||
            std::fabs(buses[13 * kFrames + i] - expectedRight) > 1.0e-5f) {
            return fail("Input Gain did not attenuate the dry sample source");
        }
    }
    values[inputGain] = 0;
    for (int block = 0; block < 120; ++block) {
        factory->step(algorithm, buses.data(), kFrames / 4);
    }

    // Prove the streamed WAV side of the same boundary independently of its
    // dry level. A source switch resets both processors; normalized stream
    // frames must produce the same wet signal as the pinned Capicola path,
    // with the established 8 V sample-player scaling applied only afterward.
    values[threshold] = 100;
    values[grain] = 32;
    values[quality] = 0;
    values[envelopeSmoothing] = 0;
    values[fade] = 0;
    values[drive] = 0;
    values[driveCharacter] = 5000;
    values[feedbackTone] = 0;
    values[mix] = 100;
    values[source] = 0;
    factory->parameterChanged(algorithm, source);
    values[source] = 1;
    factory->parameterChanged(algorithm, source);
    {
        using ReferenceProcessor =
            capicola_nt::CapicolaStereoLivePath<16384>;
        auto reference = std::make_unique<ReferenceProcessor>();
        reference->init();
        reference->setPitchSemitones(0.0f);
        reference->setStretch(1.0f);
        reference->setTransientThreshold(1.0e9f);
        reference->setGrainSize(32);
        reference->setQuality(0.1f);
        reference->setFeedback(0.0f);
        reference->setEnvelopeSmoothing(5.0e-5f);
        reference->setFade(480.0f);
        reference->setDrive(0.5f);
        reference->setDriveCharacter(0.5f);
        reference->setMix(1.0f);
        reference->setFeedbackTone(2.0e-3f);

        std::vector<float> sampleLeft(kFrames, 0.0f);
        std::vector<float> sampleRight(kFrames, 0.0f);
        std::vector<float> referenceLeft(kFrames, 0.0f);
        std::vector<float> referenceRight(kFrames, 0.0f);
        uint32_t referenceClock = 0U;
        for (int block = 0; block < 320; ++block) {
            for (int i = 0; i < kFrames; ++i, ++referenceClock) {
                sampleLeft[i] = std::sin(
                    2.0f * kPi * 330.0f *
                    static_cast<float>(referenceClock) / 48000.0f);
                sampleRight[i] = 0.35f * std::sin(
                    2.0f * kPi * 710.0f *
                    static_cast<float>(referenceClock) / 48000.0f);
            }
            reference->process(sampleLeft.data(), sampleRight.data(),
                               referenceLeft.data(), referenceRight.data(),
                               kFrames);
            factory->step(algorithm, buses.data(), kFrames / 4);
            // Initial Sample selection has its documented 50 ms fade-in.
            // Compare after that wrapper-only transition has completed.
            if (block < 40) continue;
            for (int i = 0; i < kFrames; ++i) {
                if (std::fabs(buses[12 * kFrames + i] -
                              referenceLeft[i] * 8.0f) > 1.0e-5f ||
                    std::fabs(buses[13 * kFrames + i] -
                              referenceRight[i] * 8.0f) > 1.0e-5f) {
                    return fail("stream frames were not normalized around Capicola DSP");
                }
            }
        }
    }
    for (int parameter : auditedControls) {
        values[parameter] = algorithm->parameters[parameter].def;
    }
    values[source] = 0;
    factory->parameterChanged(algorithm, source);
    values[source] = 1;
    factory->parameterChanged(algorithm, source);

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
                const float dryExpectedLeft = 8.0f * std::sin(
                    2.0f * kPi * 330.0f * static_cast<float>(blockClock + i) /
                    48000.0f);
                const float dryExpectedRight = 8.0f * 0.35f * std::sin(
                    2.0f * kPi * 710.0f * static_cast<float>(blockClock + i) /
                    48000.0f);
                mappedWetDifference += std::fabs(outLeft[i] - dryExpectedLeft) +
                                       std::fabs(outRight[i] - dryExpectedRight);
            }
        }
    }
    if (gStreamOpenCalls == 0 || gStreamRenderCalls == 0 ||
        gOpenedFolder != 0 || gOpenedSample != 1) {
        return fail("selected sample was not rendered from its stream");
    }
    if (sampleEnergy < 1.0 || stereoDifference < 1.0) {
        return fail("stereo sample was not processed through both Capicola channels");
    }
    if (mappedWetDifference < 1.0) {
        return fail("host-mapped Mix value did not modulate Capicola processing");
    }

    // A wet folder change keeps the already-running processor warm while the
    // old stream fades out and the primed replacement fades in over 50 ms each.
    // Only the single midpoint sample may be forced to zero; there must be no
    // cold-engine silence between the samples.
    const uint32_t opensBeforeFolderChange = gStreamOpenCalls;
    const uint32_t rendersBeforeFolderChange = gStreamRenderCalls;
    gStreamInitialEmptyRenders = 1U;
    values[folder] = 1;
    values[sample] = 1;
    factory->parameterChanged(algorithm, folder);
    if (gStreamOpenCalls != opensBeforeFolderChange + 1 ||
        gStreamRenderCalls != rendersBeforeFolderChange ||
        values[sample] != 0 ||
        gOpenedFolder != 1 || gOpenedSample != 0) {
        return fail("folder change did not synchronize and open its valid sample");
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    gStreamInitialEmptyRenders = 0U;
    double waitingEnergy = 0.0;
    for (int i = 0; i < kFrames; ++i) {
        waitingEnergy += std::fabs(buses[12 * kFrames + i]) +
                         std::fabs(buses[13 * kFrames + i]);
    }
    if (gStreamRenderCalls != rendersBeforeFolderChange + 2U ||
        waitingEnergy == 0.0) {
        return fail("old stream did not continue while replacement waited for frames");
    }
    const uint32_t rendersBeforeWetFade = gStreamRenderCalls;
    int consecutiveSilentFrames = 0;
    int longestSilentRun = 0;
    for (int block = 0; block < 75; ++block) {
        factory->step(algorithm, buses.data(), kFrames / 4);
        if (gStreamRenderCalls != rendersBeforeWetFade +
                static_cast<uint32_t>(block + 2)) {
            return fail("replacement stream stopped during the wet transition");
        }
        for (int i = 0; i < kFrames; ++i) {
            const bool silent = buses[12 * kFrames + i] == 0.0f &&
                                buses[13 * kFrames + i] == 0.0f;
            consecutiveSilentFrames = silent ? consecutiveSilentFrames + 1 : 0;
            if (consecutiveSilentFrames > longestSilentRun) {
                longestSilentRun = consecutiveSilentFrames;
            }
        }
    }
    if (longestSilentRun > 2) {
        return fail("wet sample replacement retained a cold-engine silence gap");
    }

    // Reopen the same sample at dry Mix so the exact two-phase gain and source
    // position can be checked. The old stream runs to the zero-gain midpoint;
    // the primed replacement then starts at frame zero without waiting for a
    // new audio block.
    values[mix] = 0;
    const float oldSourcePosition = gStreamSourcePosition;
    pressSampleLoad();
    const uint32_t rendersBeforeDryTransition = gStreamRenderCalls;
    constexpr int kSampleFadeFrames = 2400;
    for (int block = 0; block < 75; ++block) {
        factory->step(algorithm, buses.data(), kFrames / 4);
        if (gStreamRenderCalls != rendersBeforeDryTransition +
                static_cast<uint32_t>(block + 2)) {
            return fail("old and replacement streams did not render at the handoff");
        }
        for (int i = 0; i < kFrames; ++i) {
            const int transitionFrame = block * kFrames + i;
            const float position = transitionFrame < kSampleFadeFrames
                ? oldSourcePosition + static_cast<float>(transitionFrame) * 0.5f
                : static_cast<float>(transitionFrame - kSampleFadeFrames) * 0.5f;
            const uint32_t index = static_cast<uint32_t>(position);
            const float fraction = position - static_cast<float>(index);
            const float source0 = std::sin(
                2.0f * kPi * 330.0f * static_cast<float>(index) / 24000.0f);
            const float source1 = std::sin(
                2.0f * kPi * 330.0f * static_cast<float>(index + 1U) /
                24000.0f);
            const float fadeGain = transitionFrame < kSampleFadeFrames
                ? static_cast<float>(kSampleFadeFrames - transitionFrame - 1) /
                    static_cast<float>(kSampleFadeFrames)
                : static_cast<float>(
                    transitionFrame - kSampleFadeFrames + 1) /
                    static_cast<float>(kSampleFadeFrames);
            const float expected = 8.0f *
                (source0 + fraction * (source1 - source0)) * fadeGain;
            if (std::fabs(buses[12 * kFrames + i] - expected) > 1.0e-5f ||
                buses[12 * kFrames + i] != buses[13 * kFrames + i]) {
                return fail("sample replacement gain or stream position was discontinuous");
            }
        }
    }
    const float sourcePosition = gStreamSourcePosition;
    factory->step(algorithm, buses.data(), kFrames / 4);
    const float* monoLeft = buses.data() + 12 * kFrames;
    const float* monoRight = buses.data() + 13 * kFrames;
    for (int i = 0; i < kFrames; ++i) {
        const float position = sourcePosition + static_cast<float>(i) * 0.5f;
        const uint32_t index = static_cast<uint32_t>(position);
        const float fraction = position - static_cast<float>(index);
        const float source0 = std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(index) / 24000.0f);
        const float source1 = std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(index + 1U) / 24000.0f);
        const float expected = 8.0f *
            (source0 + fraction * (source1 - source0));
        if (std::fabs(monoLeft[i] - expected) > 1.0e-5f ||
            monoLeft[i] != monoRight[i]) {
            return fail("loaded mono sample did not use metadata-rate stereo playback");
        }
    }
    values[mix] = 100;

    // Sample mode has no transport trigger. A short stream proves that the
    // end reopens once, fills the rest of the block from the start, and keeps
    // the compact sample name in the title.
    gSampleFrameCount = 96;
    values[folder] = 0;
    values[sample] = 0;
    factory->parameterChanged(algorithm, folder);
    values[mix] = 0;
    settleSampleTransition();
    const uint32_t opensBeforeLoopStep = gStreamOpenCalls;
    const uint32_t rendersBeforeLoopStep = gStreamRenderCalls;
    factory->step(algorithm, buses.data(), kFrames / 4);
    for (int i = 0; i < kFrames; ++i) {
        const float expected = 8.0f * std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(i) / 48000.0f);
        if (std::fabs(buses[12 * kFrames + i] - expected) > 1.0e-6f ||
            buses[12 * kFrames + i] != buses[13 * kFrames + i]) {
            return fail("sample stream did not restart from its loop boundary");
        }
    }
    factory->step(algorithm, buses.data(), kFrames / 4);
    for (int i = 0; i < kFrames; ++i) {
        const uint32_t sourceIndex = i < 32
            ? 64U + static_cast<uint32_t>(i)
            : static_cast<uint32_t>(i - 32);
        const float expected = 8.0f * std::sin(
            2.0f * kPi * 330.0f * static_cast<float>(sourceIndex) / 48000.0f);
        if (std::fabs(buses[12 * kFrames + i] - expected) > 1.0e-6f ||
            buses[12 * kFrames + i] != buses[13 * kFrames + i]) {
            return fail("sample stream did not loop at its boundary");
        }
    }
    gDrawnText.clear();
    factory->draw(algorithm);
    if (gStreamOpenCalls != opensBeforeLoopStep + 2 ||
        gStreamRenderCalls != rendersBeforeLoopStep + 4 ||
        !drawnTextContains("CAPICOLA") ||
        !drawnTextContains("Mono")) {
        return fail("sample stream loop or compact title was not retained");
    }
    gSampleFrameCount = 48000;
    values[folder] = 1;
    factory->parameterChanged(algorithm, folder);
    values[mix] = 100;
    settleSampleTransition();

    // Streaming is the only allowed SD operation in step(). A temporary host
    // underrun drops the affected block, performs no catalogue work, and
    // resumes safely when the stream can render again.
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
        gStreamRenderCalls != rendersBeforeRemount + 2 ||
        gCardMountChecks != mountChecksBeforeRemount ||
        gFolderInfoCalls != folderCallsBeforeRemount ||
        gFileInfoCalls != fileCallsBeforeRemount ||
        gParameterDefinitionUpdates != updatesBeforeRemount) {
        return fail("stream underrun performed unbounded card recovery work");
    }
    double remountEnergy = 0.0;
    for (int i = 0; i < kFrames; ++i) {
        remountEnergy += std::fabs(buses[12 * kFrames + i]) +
                         std::fabs(buses[13 * kFrames + i]);
    }
    if (remountEnergy == 0.0) {
        return fail("sample stream did not resume after a transient underrun");
    }
    pressSampleLoad();
    factory->step(algorithm, buses.data(), kFrames / 4);
    if (gStreamOpenCalls != opensBeforeRemount + 1 ||
        gStreamRenderCalls != rendersBeforeRemount + 4 ||
        gOpenedFolder != 1 || gOpenedSample != 0) {
        return fail("explicit sample LOAD did not recover after remount");
    }
    settleSampleTransition();

    // The opened stream is the playback authority. Filename conventions can
    // make its physical variant longer than the catalogue entry used to open
    // it, so valid rendered frames must not be cut off at the reported count.
    gSampleFrameCount = 2048;
    gOpenedStreamFrameCount = 8192;
    pressSampleLoad();
    settleSampleTransition();
    const uint32_t opensBeforeReportedLength = gStreamOpenCalls;
    for (int block = 0; block < 40; ++block) {
        factory->step(algorithm, buses.data(), kFrames / 4);
    }
    if (gStreamOpenCalls != opensBeforeReportedLength) {
        return fail("sample stream restarted at stale catalogue length");
    }
    gSampleFrameCount = 48000;
    gOpenedStreamFrameCount = 0;
    pressSampleLoad();
    settleSampleTransition();

    // Missing or moved catalogue entry: the saved folder index is no longer
    // present. Explicit LOAD performs no invalid lookup/open and Sample
    // mode remains selected.
    gCardMounted = false;
    factory->step(algorithm, buses.data(), kFrames / 4);
    gNumFolders = 1;
    const uint32_t opensBeforeMissing = gStreamOpenCalls;
    const uint32_t filesBeforeMissing = gFileInfoCalls;
    gCardMounted = true;
    factory->step(algorithm, buses.data(), kFrames / 4);
    pressSampleLoad();
    settleSampleTransition();
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

    // Unsupported metadata is rejected before opening a sample stream.
    gNumFolders = 2;
    values[folder] = 0;
    values[sample] = 1;
    gSampleMetadataSupported = false;
    const uint32_t opensBeforeUnsupported = gStreamOpenCalls;
    factory->parameterChanged(algorithm, sample);
    if (values[source] != 1 || gStreamOpenCalls != opensBeforeUnsupported) {
        return fail("unsupported sample substituted or fell back to live input");
    }

    // An unreadable resource reaches the host reader, whose refusal is
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
    // different folder or file in lookups, display strings, or sample opens.
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
    // Once the folder is valid, externally supplied Sample values are
    // synchronized to the instance-owned range through the host setter. This
    // is the same path that makes a folder with fewer files immediately usable.
    values[sample] = 99;
    factory->parameterChanged(algorithm, sample);
    values[sample] = -1;
    factory->parameterChanged(algorithm, sample);
    if (factory->parameterString(algorithm, sample, -1, invalidText) != 0 ||
        values[sample] != 0 ||
        gStreamOpenCalls != opensBeforeInvalidSample + 1 ||
        gFileInfoCalls != filesBeforeInvalidSample + 2 ||
        gOpenedFolder != 0 || gOpenedSample != 1 || gInvalidCatalogLookup) {
        return fail("Sample value was not synchronized to the folder range");
    }
    settleSampleTransition();
    double synchronizedSampleEnergy = 0.0;
    for (int i = 0; i < kFrames; ++i) {
        synchronizedSampleEnergy += std::fabs(buses[12 * kFrames + i]) +
                                    std::fabs(buses[13 * kFrames + i]);
    }
    if (values[source] != 1 || synchronizedSampleEnergy == 0.0) {
        return fail("range-synchronized Sample value did not play");
    }

    // Switching back to live resets Capicola and cannot invoke the sample
    // stream renderer.
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
        return fail("live mode invoked the sample stream renderer");
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

    if (!testReleasedParameterAndPagePrefixes()) {
        return fail("released parameter metadata/page prefixes changed");
    }
    if (!testUnversionedPresetMigratesToNewAudioAndCvBehavior()) {
        return fail("unversioned presets did not adopt the new audio/CV behavior");
    }
    if (!testPresetFormatRoundTripsAndRejectsMalformedData()) {
        return fail("preset format roundtrip or transactional validation failed");
    }
    if (!testDefaultOldFormatAndSaveBeforeFirstStep()) {
        return fail("default-old format, fresh center, or pre-operation save/reload failed");
    }
    if (!testMigrationUsesBaseValuesAndCommonOffset()) {
        return fail("preset initialization changed a mapped base or common offset");
    }
    if (!testEditsAfterMigrationRemainNative()) {
        return fail("edits after initialization were converted again or lost on reload");
    }
    if (!testBipolarMappedValuesApplyWithoutCallbacks()) {
        return fail("bipolar mapped controls failed in Live or Sample");
    }
    if (!testTransientBusPulseTimingAndUnavailableSource()) {
        return fail("transient bus pulse timing/reset is incorrect");
    }
    if (!testTransientPulseSpansWarmHandoffAndShortReads()) {
        return fail("transient pulse failed across warm handoff/short reads");
    }
    if (!testBipolarDisplayShowsEffectiveRates()) {
        return fail("bipolar display hid effective stop/reverse rates");
    }
    if (!testInactiveFolderPreservesLiveAudio()) {
        return fail("browsing an inactive sample folder disrupted live audio");
    }
    if (!testPotBankRequiresPickup()) {
        return fail("switching pot banks changed a control before pickup");
    }
    if (!testShorterPhysicalSamplesKeepLooping()) {
        return fail("shorter physical sample variants stopped instead of looping");
    }
    if (!testSmallSampleCanWaitForItsFirstFrames()) {
        return fail("delayed startup repeatedly reopened a small sample before playback");
    }
    if (!testSourceChangesPreserveOutputContinuity()) {
        return fail("changing Live/Sample introduced a full-level output discontinuity");
    }
    if (!testSourceBridgeRemainsSmoothWhileSampleWaits()) {
        return fail("source bridge clicked between blocks while Sample waited");
    }
    if (!testBriefUnderrunsResumeAtTheSameAudioFrame()) {
        return fail("brief underruns restarted or advanced the sample incorrectly");
    }
    if (!testSliceDuringSdStartupPreservesWetAudio()) {
        return fail("Slice during SD startup bypassed wet processing");
    }
    if (!testDelayedSampleFadesFromItsFirstAudibleFrame()) {
        return fail("delayed sample startup consumed its fade before audio arrived");
    }
    std::printf("plugin host live/sample path: ok\n");
    return 0;
}
