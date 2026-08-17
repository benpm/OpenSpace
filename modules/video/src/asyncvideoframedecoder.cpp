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

#include <modules/video/include/asyncvideoframedecoder.h>

#include <modules/video/include/videotiming.h>
#include <ghoul/format.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <algorithm>

namespace {
    constexpr std::string_view _loggerCat = "AsyncVideoFrameDecoder";
} // namespace

namespace openspace {

AsyncVideoFrameDecoder::AsyncVideoFrameDecoder(
                                            std::unique_ptr<TextureVideoStream> stream,
                                               uint32_t prefetchCount,
                                               uint32_t cacheCapacity)
    : _stream(std::move(stream))
    , _prefetchCount(prefetchCount)
    , _cacheCapacity(std::max(cacheCapacity, 2 * prefetchCount + 1))
{
    ghoul_assert(_stream, "Stream must not be null");
    _worker = std::thread([this]() { decodeLoop(); });
}

AsyncVideoFrameDecoder::~AsyncVideoFrameDecoder() {
    _shouldStop = true;
    _cv.notify_all();
    if (_worker.joinable()) {
        _worker.join();
    }
}

void AsyncVideoFrameDecoder::setTarget(uint32_t frameIndex, int direction) {
    {
        const std::lock_guard guard = std::lock_guard(_mutex);
        if (_targetFrame == frameIndex && _direction == direction) {
            return;
        }
        _targetFrame = frameIndex;
        _direction = direction;
        _hasNewTarget = true;
    }
    _cv.notify_all();
}

std::shared_ptr<const TextureVideoStream::Frame> AsyncVideoFrameDecoder::frameFor(
                                                              uint32_t frameIndex) const
{
    const std::lock_guard guard = std::lock_guard(_mutex);
    if (_cache.empty()) {
        return nullptr;
    }
    const auto it = _cache.find(frameIndex);
    if (it != _cache.end()) {
        return it->second;
    }
    // Fall back to the nearest already decoded frame, preferring the most recent one
    // before the requested index
    const auto upper = _cache.upper_bound(frameIndex);
    if (upper != _cache.begin()) {
        return std::prev(upper)->second;
    }
    return upper->second;
}

const TextureVideoStream& AsyncVideoFrameDecoder::stream() const {
    return *_stream;
}

void AsyncVideoFrameDecoder::decodeLoop() {
    while (!_shouldStop) {
        uint32_t target = 0;
        int direction = 1;
        {
            std::unique_lock lock = std::unique_lock(_mutex);
            _cv.wait(lock, [this]() { return _shouldStop || _hasNewTarget; });
            if (_shouldStop) {
                return;
            }
            _hasNewTarget = false;
            target = _targetFrame;
            direction = _direction;
        }

        const texturevideo::DecodePlan plan = texturevideo::planDecode(
            _stream->keyframes(),
            _stream->currentPosition(),
            target,
            direction,
            _prefetchCount,
            _stream->metadata().frameCount
        );
        if (plan.firstFrame > plan.lastFrame) {
            continue;
        }
        if (plan.resetToKeyframe) {
            _stream->resetToKeyframe(plan.firstFrame);
        }

        for (uint32_t i = plan.firstFrame; i <= plan.lastFrame && !_shouldStop; i++) {
            {
                // A decode chain can be long (e.g. a backwards seek in a file without
                // intermediate keyframes); abandon it as soon as the target moves
                const std::lock_guard guard = std::lock_guard(_mutex);
                if (_hasNewTarget) {
                    break;
                }
            }

            auto frame = std::make_shared<TextureVideoStream::Frame>();
            if (!_stream->transcodeNext(*frame)) {
                LWARNING(std::format("Abandoning decode chain at frame {}", i));
                break;
            }

            const std::lock_guard guard = std::lock_guard(_mutex);
            _cache[frame->frameIndex] = std::move(frame);
            while (_cache.size() > _cacheCapacity) {
                evictWorstFrame(target, direction);
            }
        }
    }
}

void AsyncVideoFrameDecoder::evictWorstFrame(uint32_t target, int direction) {
    // Evict the frame least likely to be shown soon: distance from the target, with
    // frames behind the playback direction penalized double
    auto worst = _cache.begin();
    int64_t worstScore = -1;
    for (auto it = _cache.begin(); it != _cache.end(); it++) {
        const int64_t signedDist =
            static_cast<int64_t>(it->first) - static_cast<int64_t>(target);
        const bool isBehind = direction < 0 ? signedDist > 0 : signedDist < 0;
        const int64_t score = std::abs(signedDist) * (isBehind ? 2 : 1);
        if (score > worstScore) {
            worstScore = score;
            worst = it;
        }
    }
    _cache.erase(worst);
}

} // namespace openspace
