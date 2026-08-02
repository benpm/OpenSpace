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

#include <modules/video/include/videotiming.h>

#include <ghoul/misc/assert.h>
#include <algorithm>
#include <cmath>

namespace openspace::texturevideo {

uint32_t frameIndexFromSimTime(double nowJ2000, double startJ2000, double endJ2000,
                               uint32_t frameCount)
{
    ghoul_assert(frameCount > 0, "frameCount must be positive");
    ghoul_assert(endJ2000 > startJ2000, "End time must be after start time");

    const double percentage = std::clamp(
        (nowJ2000 - startJ2000) / (endJ2000 - startJ2000),
        0.0,
        1.0
    );
    const uint32_t frame = static_cast<uint32_t>(percentage * frameCount);
    return std::min(frame, frameCount - 1);
}

uint32_t frameIndexRealTime(double applicationTime, double startTime, double fps,
                            uint32_t frameCount, bool loop)
{
    ghoul_assert(frameCount > 0, "frameCount must be positive");
    ghoul_assert(fps > 0.0, "fps must be positive");

    const double elapsed = std::max(applicationTime - startTime, 0.0);
    const double framePos = elapsed * fps;
    if (loop) {
        return static_cast<uint32_t>(std::fmod(framePos, frameCount));
    }
    const uint32_t frame = static_cast<uint32_t>(framePos);
    return std::min(frame, frameCount - 1);
}

DecodePlan planDecode(const std::vector<uint32_t>& keyframes,
                      std::optional<uint32_t> currentPos, uint32_t targetFrame,
                      int direction, uint32_t prefetchCount, uint32_t frameCount)
{
    ghoul_assert(frameCount > 0, "frameCount must be positive");
    ghoul_assert(
        !keyframes.empty() && keyframes.front() == 0,
        "Keyframe list must contain frame 0"
    );

    targetFrame = std::min(targetFrame, frameCount - 1);

    // The frames that should end up decoded: the target plus prefetch in the playback
    // direction
    uint32_t firstNeeded = targetFrame;
    uint32_t lastNeeded = targetFrame;
    if (direction < 0) {
        firstNeeded = targetFrame >= prefetchCount ? targetFrame - prefetchCount : 0;
    }
    else {
        lastNeeded = std::min(targetFrame + prefetchCount, frameCount - 1);
    }

    // Nearest keyframe at or before the first needed frame
    const auto it = std::upper_bound(keyframes.begin(), keyframes.end(), firstNeeded);
    ghoul_assert(it != keyframes.begin(), "Keyframe list must contain frame 0");
    const uint32_t keyframe = *std::prev(it);

    // Decoding is strictly sequential, so continuing is only possible when the current
    // position has not passed the last needed frame. Restarting at the keyframe is
    // preferred when it skips ahead of the current position
    const bool canContinue =
        currentPos.has_value() && *currentPos <= lastNeeded && *currentPos >= keyframe;

    if (canContinue) {
        return DecodePlan {
            .resetToKeyframe = false,
            .firstFrame = *currentPos + 1,
            .lastFrame = lastNeeded
        };
    }
    return DecodePlan {
        .resetToKeyframe = true,
        .firstFrame = keyframe,
        .lastFrame = lastNeeded
    };
}

} // namespace openspace::texturevideo
