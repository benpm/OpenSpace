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

#ifndef __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOSTREAM___H__
#define __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOSTREAM___H__

#include <basisu_transcoder.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace openspace {

/**
 * A Basis Universal ETC1S texture video loaded from a KTX2 file, transcoded frame by
 * frame to a block-compressed GPU format. ETC1S video uses conditional replenishment
 * (P-frames), so frames must be transcoded strictly in order starting at a keyframe;
 * this class enforces the sequential position and `resetToKeyframe` restarts. All
 * methods except the constructor are intended to be called from a single (worker)
 * thread.
 */
class TextureVideoStream {
public:
    struct Metadata {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t frameCount = 0;
        uint32_t mipLevels = 0;
        /// Frames per second from the KTX2 animation metadata, or 0 if not present
        double fps = 0.0;
        bool hasAlpha = false;
    };

    struct LevelInfo {
        /// Byte offset of this mipmap level within Frame::blocks
        size_t offset = 0;
        size_t byteSize = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t totalBlocks = 0;
    };

    /// One fully transcoded frame: the block data of all mipmap levels, tightly packed
    struct Frame {
        uint32_t frameIndex = 0;
        std::vector<std::byte> blocks;
    };

    /**
     * Loads and validates the KTX2 file. Only BasisLZ/ETC1S encoded files are accepted.
     *
     * \param file The path of the .ktx2 file to load
     * \param format The transcode target; must be a block-based format
     *
     * \throw ghoul::RuntimeError If the file cannot be read or is not an ETC1S KTX2 file
     */
    explicit TextureVideoStream(std::filesystem::path file,
        basist::transcoder_texture_format format =
            basist::transcoder_texture_format::cTFBC7_RGBA);

    const Metadata& metadata() const;

    /// Per-mipmap-level layout of every transcoded frame
    const std::vector<LevelInfo>& levels() const;

    /// Total byte size of one transcoded frame (all mipmap levels)
    size_t frameByteSize() const;

    /// Sorted indices of the I-frames; always contains frame 0
    const std::vector<uint32_t>& keyframes() const;

    /// The last sequentially transcoded frame, or `std::nullopt` after a reset
    std::optional<uint32_t> currentPosition() const;

    /**
     * Resets the transcoder state and restarts the sequential decode at \p keyframe.
     *
     * \param keyframe The frame to restart at; must be an I-frame
     */
    void resetToKeyframe(uint32_t keyframe);

    /**
     * Transcodes the next sequential frame into \p frame.
     *
     * \param frame The frame object receiving the block data
     * \return `true` on success; on failure the sequential position becomes invalid
     */
    bool transcodeNext(Frame& frame);

private:
    std::vector<uint8_t> _fileData;
    basist::ktx2_transcoder _transcoder;
    basist::ktx2_transcoder_state _state;
    basist::transcoder_texture_format _format;
    uint32_t _bytesPerBlock = 16;

    Metadata _metadata;
    std::vector<LevelInfo> _levels;
    size_t _frameByteSize = 0;
    std::vector<uint32_t> _keyframes;
    uint32_t _nextFrame = 0;
    bool _hasValidPosition = false;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOSTREAM___H__
