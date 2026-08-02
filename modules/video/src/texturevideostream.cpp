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

#include <modules/video/include/texturevideostream.h>

#include <ghoul/format.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/exception.h>
#include <algorithm>
#include <cstring>
#include <fstream>

namespace {
    constexpr std::string_view _loggerCat = "TextureVideoStream";

    constexpr std::string_view ReencodeHint =
        "Re-encode the video with support/texturevideo/convert_video.py";
} // namespace

namespace openspace {

TextureVideoStream::TextureVideoStream(std::filesystem::path file,
                                       basist::transcoder_texture_format format)
    : _format(format)
{
    _bytesPerBlock = basist::basis_get_bytes_per_block_or_pixel(_format);
    ghoul_assert(
        basist::basis_transcoder_format_is_uncompressed(_format) == false,
        "Transcode target must be a block-compressed format"
    );

    std::ifstream f = std::ifstream(file, std::ios::binary | std::ios::ate);
    if (!f.good()) {
        throw ghoul::RuntimeError(std::format(
            "Could not open texture video file '{}'", file
        ));
    }
    _fileData.resize(static_cast<size_t>(f.tellg()));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(_fileData.data()), _fileData.size());
    if (!f.good()) {
        throw ghoul::RuntimeError(std::format(
            "Error reading texture video file '{}'", file
        ));
    }

    if (!_transcoder.init(_fileData.data(), static_cast<uint32_t>(_fileData.size()))) {
        throw ghoul::RuntimeError(std::format(
            "'{}' is not a valid KTX2 file. {}", file, ReencodeHint
        ));
    }
    if (!_transcoder.is_etc1s()) {
        throw ghoul::RuntimeError(std::format(
            "'{}' is not BasisLZ/ETC1S encoded; UASTC and other formats are not "
            "supported. {}", file, ReencodeHint
        ));
    }
    if (_transcoder.get_faces() != 1) {
        throw ghoul::RuntimeError(std::format(
            "'{}' is a cubemap, which is not supported as a texture video", file
        ));
    }
    if (!_transcoder.start_transcoding()) {
        throw ghoul::RuntimeError(std::format(
            "Could not start transcoding '{}'. {}", file, ReencodeHint
        ));
    }

    _metadata.width = _transcoder.get_width();
    _metadata.height = _transcoder.get_height();
    _metadata.frameCount = std::max(1u, _transcoder.get_layers());
    _metadata.mipLevels = _transcoder.get_levels();
    _metadata.hasAlpha = _transcoder.get_has_alpha() != 0;

    // KTXanimData is three little-endian uint32s: duration, timescale, loopcount; the
    // frame rate is timescale ticks per second with each frame lasting duration ticks
    const basisu::uint8_vec* anim = _transcoder.find_key("KTXanimData");
    if (anim && anim->size() >= 8) {
        uint32_t duration = 0;
        uint32_t timescale = 0;
        std::memcpy(&duration, anim->data(), sizeof(uint32_t));
        std::memcpy(&timescale, anim->data() + sizeof(uint32_t), sizeof(uint32_t));
        if (duration > 0) {
            _metadata.fps = static_cast<double>(timescale) / duration;
        }
    }

    // Per-frame layout of the transcoded output, identical for every frame
    _levels.reserve(_metadata.mipLevels);
    for (uint32_t level = 0; level < _metadata.mipLevels; level++) {
        basist::ktx2_image_level_info info;
        if (!_transcoder.get_image_level_info(info, level, 0, 0)) {
            throw ghoul::RuntimeError(std::format(
                "Could not read mipmap level {} of '{}'", level, file
            ));
        }
        _levels.push_back(LevelInfo {
            .offset = _frameByteSize,
            .byteSize = static_cast<size_t>(info.m_total_blocks) * _bytesPerBlock,
            .width = info.m_orig_width,
            .height = info.m_orig_height,
            .totalBlocks = info.m_total_blocks
        });
        _frameByteSize += _levels.back().byteSize;
    }

    // I-frame indices; for '-tex_type video' files only frame 0, for 2D-array files
    // every frame
    for (uint32_t i = 0; i < _metadata.frameCount; i++) {
        basist::ktx2_image_level_info info;
        if (_transcoder.get_image_level_info(info, 0, i, 0) && info.m_iframe_flag) {
            _keyframes.push_back(i);
        }
    }
    if (_keyframes.empty() || _keyframes.front() != 0) {
        throw ghoul::RuntimeError(std::format(
            "'{}' does not start with an I-frame. {}", file, ReencodeHint
        ));
    }

    LDEBUG(std::format(
        "Loaded '{}': {}x{}, {} frames, {} mipmap levels, {} keyframes, {} fps, "
        "{:.1f} MiB file, {:.1f} KiB per transcoded frame",
        file, _metadata.width, _metadata.height, _metadata.frameCount,
        _metadata.mipLevels, _keyframes.size(), _metadata.fps,
        _fileData.size() / (1024.0 * 1024.0), _frameByteSize / 1024.0
    ));
}

const TextureVideoStream::Metadata& TextureVideoStream::metadata() const {
    return _metadata;
}

const std::vector<TextureVideoStream::LevelInfo>& TextureVideoStream::levels() const {
    return _levels;
}

size_t TextureVideoStream::frameByteSize() const {
    return _frameByteSize;
}

const std::vector<uint32_t>& TextureVideoStream::keyframes() const {
    return _keyframes;
}

std::optional<uint32_t> TextureVideoStream::currentPosition() const {
    if (_hasValidPosition) {
        return _nextFrame - 1;
    }
    return std::nullopt;
}

void TextureVideoStream::resetToKeyframe(uint32_t keyframe) {
    ghoul_assert(
        std::binary_search(_keyframes.begin(), _keyframes.end(), keyframe),
        "Frame must be a keyframe"
    );
    _state = basist::ktx2_transcoder_state();
    _nextFrame = keyframe;
    _hasValidPosition = false;
}

bool TextureVideoStream::transcodeNext(Frame& frame) {
    ghoul_assert(_nextFrame < _metadata.frameCount, "No more frames to transcode");

    frame.frameIndex = _nextFrame;
    frame.blocks.resize(_frameByteSize);
    for (uint32_t level = 0; level < _metadata.mipLevels; level++) {
        const LevelInfo& info = _levels[level];
        const bool success = _transcoder.transcode_image_level(
            level,
            _nextFrame,
            0,
            frame.blocks.data() + info.offset,
            info.totalBlocks,
            _format,
            0,
            0,
            0,
            -1,
            -1,
            &_state
        );
        if (!success) {
            LERROR(std::format(
                "Transcoding frame {} (mipmap level {}) failed", _nextFrame, level
            ));
            _hasValidPosition = false;
            return false;
        }
    }
    _nextFrame++;
    _hasValidPosition = true;
    return true;
}

} // namespace openspace
