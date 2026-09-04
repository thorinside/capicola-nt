// Capicola for disting NT: independently maintained wrapper integration, 2026.
// SPDX-License-Identifier: AGPL-3.0-only

#pragma once

#include <cmath>
#include <cstddef>

#include "Detector.h"
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
        outputDetector_.Init();
        feedbackAmount_ = 0.0f;
        mix_ = 1.0f;
        inputEnvelope_ = 0.0f;
        outputEnvelope_ = 0.0f;
        inputTransient_ = false;
        outputTransient_ = false;
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
        outputDetector_.SetThreshold(threshold);
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
        outputDetector_.SetCutoff(cutoff);
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
        for (auto& channel : channels_) {
            // Preserve the queued LIVE_EFFECT request until audio starts.
            if (channel.GetState() == capicola::State::LIVE_EFFECT) {
                channel.SubmitRequest(capicola::Request::SLICE);
            }
        }
    }

    bool inputTransient() const { return inputTransient_; }
    bool outputTransient() const { return outputTransient_; }
    float inputEnvelope() const { return inputEnvelope_; }
    float outputEnvelope() const { return outputEnvelope_; }

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

        // Preserve upstream's stereo guard: an automatic catch-up on one side
        // also catches the other side when their source times drift over 1 s.
        const bool leftFired = channels_[0].FiredThisBlock();
        const bool rightFired = channels_[1].FiredThisBlock();
        if (leftFired != rightFired) {
            const double leftLag = channels_[0].GridLag();
            const double rightLag = channels_[1].GridLag();
            const double drift = leftLag > rightLag
                ? leftLag - rightLag : rightLag - leftLag;
            if (drift > 48000.0) {
                channels_[leftFired ? 1 : 0].SubmitRequest(capicola::Request::SLICE);
            }
        }

        // Mirror Capicola's audited panel/CV analysis: the output detector sees
        // the post-mix mono sum while the input analysis comes from the linked
        // per-channel recorders.
        for (std::size_t i = 0; i < frames; ++i) {
            outputDetector_.Analyze(outLeft[i] + outRight[i]);
        }
        const float leftEnvelope = channels_[0].TkeoEnvelope();
        const float rightEnvelope = channels_[1].TkeoEnvelope();
        inputEnvelope_ = normalizeEnvelope(
            leftEnvelope > rightEnvelope ? leftEnvelope : rightEnvelope);
        outputEnvelope_ = normalizeEnvelope(outputDetector_.Envelope());
        inputTransient_ = channels_[0].DetectorGate() ||
                          channels_[1].DetectorGate();
        outputTransient_ = outputDetector_.Gate();
    }

private:
    static float normalizeEnvelope(float envelope) {
        float normalized = envelope * 200.0f;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        return std::sqrt(normalized);
    }

    capicola::Shapers shapers_;
    bool shapersInitialized_ = false;
    capicola::KeyframeRecorder<RingFrames> channels_[2];
    capicola::StateVariable feedbackFilters_[2];
    capicola::Detector outputDetector_;
    float feedback_[2][MaxBlockFrames];
    float injected_[2][MaxBlockFrames];
    float feedbackAmount_;
    float mix_;
    float inputEnvelope_;
    float outputEnvelope_;
    bool inputTransient_;
    bool outputTransient_;
};

} // namespace capicola_nt
