#pragma once

#include <cmath>
#include <cstddef>

#include "KeyframeRecorder.h"
#include "Shapers.h"
#include "filter.h"

namespace capicola_nt {

template <typename Channel>
class StereoLivePath {
public:
    void init() {
        channels_[0].init();
        channels_[1].init();
    }

    void process(const float* left, const float* right, float* outLeft,
                 float* outRight, std::size_t frames) {
        channels_[0].process(left, outLeft, frames);
        channels_[1].process(right != nullptr ? right : left, outRight, frames);
    }

private:
    Channel channels_[2];
};

/**
 * Two linked Capicola channels. A null right input applies NT's left-only
 * normalization rule. Processing controls are shared by both channels.
 */
template <int RingFrames, int MaxBlockFrames = 256>
class CapicolaStereoLivePath {
public:
    void init() {
        // Tabulation is construction-time work; source changes only reset the
        // bounded realtime state and must not rebuild the tables.
        if (!shapersInitialized_) {
            shapers_.Init();
            shapersInitialized_ = true;
        }
        for (int channel = 0; channel < 2; ++channel) {
            channels_[channel].Init(&shapers_);
            channels_[channel].SubmitRequest(capicola::Request::LIVE_EFFECT);
            feedbackFilters_[channel].Init();
            feedbackFilters_[channel].SetControls(0.02f, 0.01f);
            for (int i = 0; i < MaxBlockFrames; ++i) {
                feedback_[channel][i] = 0.0f;
            }
        }
        feedbackAmount_ = 0.0f;
        mix_ = 1.0f;
    }

    void setPitchSemitones(float semitones) {
        const float ratio = std::exp2(semitones / 12.0f);
        for (auto& channel : channels_) channel.SetGrainPitch(ratio);
    }

    void setStretch(float stretch) {
        for (auto& channel : channels_) channel.SetGrainStretch(stretch);
    }

    void setTransientThreshold(float threshold) {
        for (auto& channel : channels_) channel.SetTransientThreshold(threshold);
    }

    void setGrainSize(int keyframes) {
        for (auto& channel : channels_) channel.SetGrainLeash(keyframes);
    }

    void setQuality(float epsilon) {
        for (auto& channel : channels_) channel.SetThreshold(epsilon);
    }

    void setFeedback(float amount) {
        feedbackAmount_ = amount < 0.0f ? 0.0f : amount;
    }

    void setEnvelopeSmoothing(float cutoff) {
        for (auto& channel : channels_) channel.SetTkeoCutoff(cutoff);
    }

    void setFade(float frames) {
        for (auto& channel : channels_) channel.SetGrainFade(frames);
    }

    void setDrive(float drive) {
        for (auto& channel : channels_) channel.SetDistortDrive(drive);
    }

    void setDriveCharacter(float character) {
        for (auto& channel : channels_) channel.SetDistortCharacter(character);
    }

    void setFeedbackTone(float cutoff) {
        for (auto& filter : feedbackFilters_) filter.SetControls(cutoff, 0.01f);
    }

    void setMix(float wet) {
        mix_ = wet < 0.0f ? 0.0f : (wet > 1.0f ? 1.0f : wet);
    }

    void triggerSlice() {
        for (auto& channel : channels_) channel.SubmitRequest(capicola::Request::SLICE);
    }

    void process(const float* left,
                 const float* right,
                 float* outLeft,
                 float* outRight,
                 std::size_t frames) {
        const float* inputs[2] = {left, right != nullptr ? right : left};
        float* outputs[2] = {outLeft, outRight};
        const bool useFeedback = feedbackAmount_ > 0.0f &&
                                 frames <= static_cast<std::size_t>(MaxBlockFrames);

        for (int channel = 0; channel < 2; ++channel) {
            const float* source = inputs[channel];
            if (useFeedback) {
                for (std::size_t i = 0; i < frames; ++i) {
                    feedbackFilters_[channel].Tick(
                        0.5f * shapers_.ReadSinc(feedback_[channel][i]));
                    float injection = feedbackFilters_[channel].GetBandpass() *
                                      feedbackAmount_;
                    if (injection > 1.0f) injection = 1.0f;
                    if (injection < -1.0f) injection = -1.0f;
                    injected_[channel][i] = source[i] + injection;
                }
                source = injected_[channel];
            }
            channels_[channel].ProcessBlock(source, outputs[channel], frames);
            if (frames <= static_cast<std::size_t>(MaxBlockFrames)) {
                for (std::size_t i = 0; i < frames; ++i) {
                    feedback_[channel][i] = outputs[channel][i];
                }
            }
            const float dry = 1.0f - mix_;
            for (std::size_t i = 0; i < frames; ++i) {
                outputs[channel][i] = mix_ * outputs[channel][i] +
                                      dry * inputs[channel][i];
            }
        }
    }

private:
    capicola::Shapers shapers_;
    bool shapersInitialized_ = false;
    capicola::KeyframeRecorder<RingFrames> channels_[2];
    capicola::StateVariable feedbackFilters_[2];
    float feedback_[2][MaxBlockFrames];
    float injected_[2][MaxBlockFrames];
    float feedbackAmount_;
    float mix_;
};

} // namespace capicola_nt
