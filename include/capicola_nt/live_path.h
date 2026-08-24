#pragma once

#include <cstddef>

#include "KeyframeRecorder.h"

namespace capicola_nt {

/**
 * Two independent Capicola channels with NT's left-only normalization rule.
 * A null right input means "unconnected" and routes the left signal to both
 * processors. A non-null right input restores independent stereo immediately.
 */
template <typename Channel>
class StereoLivePath {
public:
    void init() {
        channels_[0].init();
        channels_[1].init();
    }

    void process(const float* left,
                 const float* right,
                 float* outLeft,
                 float* outRight,
                 std::size_t frames) {
        channels_[0].process(left, outLeft, frames);
        channels_[1].process(right != nullptr ? right : left, outRight, frames);
    }

private:
    Channel channels_[2];
};

template <int RingFrames>
class CapicolaLiveChannel {
public:
    void init() {
        recorder_.Init();
        recorder_.SubmitRequest(capicola::Request::LIVE_EFFECT);
    }

    void process(const float* input, float* output, std::size_t frames) {
        recorder_.ProcessBlock(input, output, frames);
    }

private:
    capicola::KeyframeRecorder<RingFrames> recorder_;
};

template <int RingFrames>
using CapicolaStereoLivePath = StereoLivePath<CapicolaLiveChannel<RingFrames>>;

} // namespace capicola_nt
