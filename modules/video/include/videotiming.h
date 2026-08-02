/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2026                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_MODULE_VIDEO___VIDEOTIMING___H__
#define __OPENSPACE_MODULE_VIDEO___VIDEOTIMING___H__

#include <cstdint>
#include <optional>
#include <vector>

namespace openspace::texturevideo {

/**
 * Maps a simulation time to a frame index by linear interpolation over
 * [\p startJ2000, \p endJ2000], clamped to the first/last frame outside that interval.
 *
 * \param nowJ2000 The current simulation time in seconds past the J2000 epoch
 * \param startJ2000 The simulation time of the first frame
 * \param endJ2000 The simulation time of the last frame
 * \param frameCount The total number of frames, must be > 0
 * \return The frame index in [0, frameCount - 1]
 */
uint32_t frameIndexFromSimTime(double nowJ2000, double startJ2000, double endJ2000,
    uint32_t frameCount);

/**
 * Maps a wall-clock playback position to a frame index.
 *
 * \param applicationTime The current application time in seconds
 * \param startTime The application time at which playback started
 * \param fps The video frame rate, must be > 0
 * \param frameCount The total number of frames, must be > 0
 * \param loop If `true` the position wraps around; otherwise it clamps to the last frame
 * \return The frame index in [0, frameCount - 1]
 */
uint32_t frameIndexRealTime(double applicationTime, double startTime, double fps,
    uint32_t frameCount, bool loop);

/**
 * The frames that must be transcoded to make a target frame (plus prefetch) available.
 * If `resetToKeyframe` is `true`, the transcoder state must be reset and decoding
 * restarted at `firstFrame` (which is then a keyframe); otherwise decoding continues
 * sequentially from the current position. If `firstFrame > lastFrame` nothing needs to
 * be decoded.
 */
struct DecodePlan {
    bool resetToKeyframe = false;
    uint32_t firstFrame = 0;
    uint32_t lastFrame = 0;
};

/**
 * Plans the sequential decode work required to make \p targetFrame available, together
 * with `prefetchCount` frames of read-ahead in the playback direction. ETC1S texture
 * video must be transcoded strictly in order starting at a keyframe (out-of-order
 * transcoding silently produces wrong frames), so the plan either continues from
 * \p currentPos or restarts at the nearest keyframe, whichever reaches the goal with
 * fewer frames.
 *
 * \param keyframes Sorted frame indices of the I-frames; must contain at least frame 0
 * \param currentPos The index of the last sequentially decoded frame, or `std::nullopt`
 *        if the transcoder state is invalid
 * \param targetFrame The frame that playback needs now
 * \param direction The playback direction; negative means backwards
 * \param prefetchCount The number of frames to decode ahead in the playback direction
 * \param frameCount The total number of frames, must be > 0
 * \return The resulting DecodePlan
 */
DecodePlan planDecode(const std::vector<uint32_t>& keyframes,
    std::optional<uint32_t> currentPos, uint32_t targetFrame, int direction,
    uint32_t prefetchCount, uint32_t frameCount);

} // namespace openspace::texturevideo

#endif // __OPENSPACE_MODULE_VIDEO___VIDEOTIMING___H__
