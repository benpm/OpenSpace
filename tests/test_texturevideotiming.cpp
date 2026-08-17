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

#include <catch2/catch_test_macros.hpp>

#include <modules/video/include/videotiming.h>

using namespace openspace::texturevideo;

TEST_CASE("TextureVideoTiming: frameIndexFromSimTime", "[texturevideotiming]") {
    // 100 frames over [0, 100]: one frame per second
    CHECK(frameIndexFromSimTime(0.0, 0.0, 100.0, 100) == 0);
    CHECK(frameIndexFromSimTime(50.0, 0.0, 100.0, 100) == 50);
    CHECK(frameIndexFromSimTime(99.99, 0.0, 100.0, 100) == 99);

    // Clamped outside the interval
    CHECK(frameIndexFromSimTime(-10.0, 0.0, 100.0, 100) == 0);
    CHECK(frameIndexFromSimTime(100.0, 0.0, 100.0, 100) == 99);
    CHECK(frameIndexFromSimTime(1e6, 0.0, 100.0, 100) == 99);

    // Single frame is always frame 0
    CHECK(frameIndexFromSimTime(0.5, 0.0, 1.0, 1) == 0);
}

TEST_CASE("TextureVideoTiming: frameIndexRealTime", "[texturevideotiming]") {
    // 24 frames at 12 fps = 2 seconds
    CHECK(frameIndexRealTime(0.0, 0.0, 12.0, 24, false) == 0);
    CHECK(frameIndexRealTime(1.0, 0.0, 12.0, 24, false) == 12);
    CHECK(frameIndexRealTime(5.0, 0.0, 12.0, 24, false) == 23);

    // Looping wraps
    CHECK(frameIndexRealTime(2.0, 0.0, 12.0, 24, true) == 0);
    CHECK(frameIndexRealTime(2.5, 0.0, 12.0, 24, true) == 6);
    CHECK(frameIndexRealTime(4.5, 0.0, 12.0, 24, true) == 6);

    // Start offset shifts the origin; times before the start clamp to frame 0
    CHECK(frameIndexRealTime(10.0, 10.0, 12.0, 24, false) == 0);
    CHECK(frameIndexRealTime(11.0, 10.0, 12.0, 24, false) == 12);
    CHECK(frameIndexRealTime(9.0, 10.0, 12.0, 24, true) == 0);
}

TEST_CASE("TextureVideoTiming: planDecode forward", "[texturevideotiming]") {
    const std::vector<uint32_t> onlyFirst = { 0 };

    // Fresh state: restart at the keyframe, decode through target + prefetch
    const DecodePlan fresh = planDecode(onlyFirst, std::nullopt, 10, 1, 8, 100);
    CHECK(fresh.resetToKeyframe);
    CHECK(fresh.firstFrame == 0);
    CHECK(fresh.lastFrame == 18);

    // Continue sequentially from the current position
    const DecodePlan cont = planDecode(onlyFirst, 18, 19, 1, 8, 100);
    CHECK_FALSE(cont.resetToKeyframe);
    CHECK(cont.firstFrame == 19);
    CHECK(cont.lastFrame == 27);

    // Already decoded: nothing to do
    const DecodePlan done = planDecode(onlyFirst, 27, 19, 1, 8, 100);
    CHECK_FALSE(done.resetToKeyframe);
    CHECK(done.firstFrame > done.lastFrame);

    // Prefetch clamps at the end of the video
    const DecodePlan tail = planDecode(onlyFirst, 95, 98, 1, 8, 100);
    CHECK_FALSE(tail.resetToKeyframe);
    CHECK(tail.lastFrame == 99);
}

TEST_CASE("TextureVideoTiming: planDecode backward and jumps", "[texturevideotiming]") {
    const std::vector<uint32_t> onlyFirst = { 0 };

    // Backward: the current position is past the needed window, so with only frame 0
    // as a keyframe the decode must restart from the beginning
    const DecodePlan back = planDecode(onlyFirst, 50, 40, -1, 8, 100);
    CHECK(back.resetToKeyframe);
    CHECK(back.firstFrame == 0);
    CHECK(back.lastFrame == 40);

    // With keyframes everywhere (random-access file), backward decode starts at the
    // beginning of the prefetch window instead
    std::vector<uint32_t> allKeyframes(100);
    for (uint32_t i = 0; i < 100; i++) {
        allKeyframes[i] = i;
    }
    const DecodePlan backRa = planDecode(allKeyframes, 50, 40, -1, 8, 100);
    CHECK(backRa.resetToKeyframe);
    CHECK(backRa.firstFrame == 32);
    CHECK(backRa.lastFrame == 40);

    // Long forward jump with keyframes available skips ahead instead of decoding
    // through the gap
    const DecodePlan jump = planDecode(allKeyframes, 3, 90, 1, 8, 100);
    CHECK(jump.resetToKeyframe);
    CHECK(jump.firstFrame == 90);
    CHECK(jump.lastFrame == 98);

    // The same jump without intermediate keyframes decodes through the gap
    const DecodePlan slowJump = planDecode(onlyFirst, 3, 90, 1, 8, 100);
    CHECK_FALSE(slowJump.resetToKeyframe);
    CHECK(slowJump.firstFrame == 4);
    CHECK(slowJump.lastFrame == 98);

    // Target beyond the end is clamped
    const DecodePlan past = planDecode(onlyFirst, std::nullopt, 500, 1, 8, 100);
    CHECK(past.lastFrame == 99);
}
