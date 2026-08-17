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

#ifndef __OPENSPACE_MODULE_VIDEO___ASYNCVIDEOFRAMEDECODER___H__
#define __OPENSPACE_MODULE_VIDEO___ASYNCVIDEOFRAMEDECODER___H__

#include <modules/video/include/texturevideostream.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <thread>

namespace openspace {

/**
 * Decodes texture video frames ahead of playback on a single worker thread. ETC1S
 * conditional-replenishment decoding is inherently sequential, so one stateful worker is
 * used rather than a thread pool. The render thread announces the frame it needs via
 * `setTarget` and picks up finished frames with `frameFor`, which never blocks.
 */
class AsyncVideoFrameDecoder {
public:
    /**
     * Creates the decoder and starts its worker thread.
     *
     * \param stream The stream to decode from; the decoder takes ownership and accesses
     *        it exclusively from the worker thread
     * \param prefetchCount The number of frames decoded ahead in the playback direction
     * \param cacheCapacity The maximum number of decoded frames kept in memory
     */
    explicit AsyncVideoFrameDecoder(std::unique_ptr<TextureVideoStream> stream,
        uint32_t prefetchCount = 8, uint32_t cacheCapacity = 32);
    ~AsyncVideoFrameDecoder();

    /**
     * Announces the frame that playback needs. Cheap to call every render frame.
     *
     * \param frameIndex The frame that should be made available
     * \param direction The playback direction; negative means backwards
     */
    void setTarget(uint32_t frameIndex, int direction);

    /**
     * Returns the decoded frame for \p frameIndex, the nearest already decoded frame if
     * the exact one is not available yet, or `nullptr` if nothing is decoded. Never
     * blocks.
     */
    std::shared_ptr<const TextureVideoStream::Frame> frameFor(uint32_t frameIndex) const;

    const TextureVideoStream& stream() const;

private:
    void decodeLoop();
    void evictWorstFrame(uint32_t target, int direction);

    std::unique_ptr<TextureVideoStream> _stream;
    const uint32_t _prefetchCount;
    const uint32_t _cacheCapacity;

    mutable std::mutex _mutex;
    std::condition_variable _cv;
    std::atomic_bool _shouldStop = false;
    uint32_t _targetFrame = 0;
    int _direction = 1;
    bool _hasNewTarget = true;

    /// Decoded frames keyed by frame index; guarded by _mutex
    std::map<uint32_t, std::shared_ptr<const TextureVideoStream::Frame>> _cache;

    std::thread _worker;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_VIDEO___ASYNCVIDEOFRAMEDECODER___H__
