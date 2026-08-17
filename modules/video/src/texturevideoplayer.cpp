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

#include <modules/video/include/texturevideoplayer.h>

#include <modules/video/include/asyncvideoframedecoder.h>
#include <modules/video/include/texturevideostream.h>
#include <modules/video/include/videotiming.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/windowdelegate.h>
#include <openspace/util/timemanager.h>
#include <ghoul/format.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/exception.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/systemcapabilities/openglcapabilitiescomponent.h>
#include <algorithm>
#include <cmath>

namespace {
    constexpr std::string_view _loggerCat = "TextureVideoPlayer";

    // BC7, core since OpenGL 4.2
    constexpr GLenum CompressedRgbaBptc = GL_COMPRESSED_RGBA_BPTC_UNORM;
    // BC3, EXT_texture_compression_s3tc
    constexpr GLenum CompressedRgbaDxt5 = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;

    constexpr double FallbackFps = 24.0;

    constexpr openspace::Property::PropertyInfo PlayInfo = {
        "Play",
        "Play",
        "Starts playback of the video."
    };

    constexpr openspace::Property::PropertyInfo PauseInfo = {
        "Pause",
        "Pause",
        "Pauses playback of the video."
    };

    constexpr openspace::Property::PropertyInfo GoToStartInfo = {
        "GoToStart",
        "Go to Start",
        "Restarts the video from its first frame."
    };
} // namespace

namespace openspace {

TextureVideoPlayer::TextureVideoPlayer(Settings settings)
    : PropertyOwner({ "TextureVideoPlayer", "Texture Video Player" })
    , _settings(std::move(settings))
    , _play(PlayInfo)
    , _pause(PauseInfo)
    , _goToStart(GoToStartInfo)
{
    if (_settings.mode == PlaybackMode::RealTimeLoop) {
        _play.onChange([this]() {
            if (!_isPlaying && _fps > 0.0) {
                _playbackStartTime =
                    global::windowDelegate->applicationTime() -
                    _pausedFramePosition / _fps;
                _isPlaying = true;
            }
        });
        _pause.onChange([this]() {
            if (_isPlaying) {
                const double elapsed =
                    global::windowDelegate->applicationTime() - _playbackStartTime;
                _pausedFramePosition = std::max(elapsed, 0.0) * _fps;
                _isPlaying = false;
            }
        });
        _goToStart.onChange([this]() {
            _playbackStartTime = global::windowDelegate->applicationTime();
            _pausedFramePosition = 0.0;
        });
        addProperty(_play);
        addProperty(_pause);
        addProperty(_goToStart);
    }
}

TextureVideoPlayer::~TextureVideoPlayer() {}

void TextureVideoPlayer::initialize() {
    ghoul_assert(!_decoder, "Player is already initialized");

    // BC7 requires OpenGL 4.2 / ARB_texture_compression_bptc; BC3 is the fallback for
    // older hardware
    const bool hasBptc =
        OpenGLCap.isExtensionSupported("GL_ARB_texture_compression_bptc");
    if (!hasBptc) {
        LWARNING("BPTC (BC7) not supported, falling back to lower quality DXT5 (BC3)");
    }
    const basist::transcoder_texture_format transcodeFormat = hasBptc ?
        basist::transcoder_texture_format::cTFBC7_RGBA :
        basist::transcoder_texture_format::cTFBC3_RGBA;
    const GLenum internalFormat = hasBptc ? CompressedRgbaBptc : CompressedRgbaDxt5;

    auto stream = std::make_unique<TextureVideoStream>(_settings.file, transcodeFormat);
    const TextureVideoStream::Metadata& meta = stream->metadata();

    if (_settings.fpsOverride > 0.0) {
        _fps = _settings.fpsOverride;
    }
    else if (meta.fps > 0.0) {
        _fps = meta.fps;
    }
    else {
        LWARNING(std::format(
            "'{}' carries no frame rate metadata, assuming {} fps",
            _settings.file, FallbackFps
        ));
        _fps = FallbackFps;
    }

    // The number of levels ghoul will allocate for the immutable storage; the file may
    // contain more (down to 1x1), which are then not uploaded
    uint32_t allocatedLevels = 1;
    if (meta.mipLevels > 1) {
        const uint32_t maxX = static_cast<uint32_t>(std::floor(std::log2(meta.width)));
        const uint32_t maxY = static_cast<uint32_t>(std::floor(std::log2(meta.height)));
        const uint32_t maxLevels = meta.height > 1 ? std::min(maxX, maxY) : maxX;
        allocatedLevels = std::clamp(meta.mipLevels, 1u, std::max(maxLevels, 1u));
    }
    _nUploadLevels = std::min(meta.mipLevels, allocatedLevels);

    _frameTexture = std::make_unique<ghoul::opengl::Texture>(
        ghoul::opengl::Texture::FormatInit {
            .dimensions = glm::uvec3(meta.width, meta.height, 1),
            .type = GL_TEXTURE_2D,
            .format = ghoul::opengl::Texture::Format::RGBA,
            .dataType = GL_UNSIGNED_BYTE,
            .internalFormat = internalFormat
        },
        ghoul::opengl::Texture::SamplerInit {
            .filter = _nUploadLevels > 1 ?
                ghoul::opengl::Texture::FilterMode::AnisotropicMipMap :
                ghoul::opengl::Texture::FilterMode::Linear,
            .wrapping = ghoul::opengl::Texture::WrappingMode::ClampToBorder,
            .mipMapLevel = static_cast<int>(_nUploadLevels),
            .borderColor = glm::vec4(0.f)
        }
    );
    _frameTexture->setName(std::format("TextureVideo {}", _settings.file.filename()));

    _decoder = std::make_unique<AsyncVideoFrameDecoder>(std::move(stream));
    _playbackStartTime = global::windowDelegate->applicationTime();

    // Start out fully transparent so tile providers can hand out the texture before the
    // first frame arrives
    uploadTransparentFrame();
}

void TextureVideoPlayer::deinitialize() {
    _decoder = nullptr;
    _frameTexture = nullptr;
    _uploadedFrame = std::nullopt;
    _previousTarget = std::nullopt;
}

bool TextureVideoPlayer::isInitialized() const {
    return _decoder != nullptr;
}

void TextureVideoPlayer::update() {
    if (!_decoder) {
        return;
    }

    if (!isActive()) {
        // Outside the [StartTime, EndTime] interval with HideOutsideRange set; make the
        // overlay fully transparent instead of freezing on the first/last frame
        if (_uploadedFrame.has_value()) {
            uploadTransparentFrame();
        }
        return;
    }

    const uint32_t target = targetFrameForNow();
    const int direction =
        (_previousTarget.has_value() && target < *_previousTarget) ? -1 : 1;
    _previousTarget = target;
    _decoder->setTarget(target, direction);

    const std::shared_ptr<const TextureVideoStream::Frame> frame =
        _decoder->frameFor(target);
    if (!frame || (_uploadedFrame.has_value() && frame->frameIndex == *_uploadedFrame)) {
        return;
    }

    const std::vector<TextureVideoStream::LevelInfo>& levels = _decoder->stream().levels();
    for (uint32_t level = 0; level < _nUploadLevels; level++) {
        const TextureVideoStream::LevelInfo& info = levels[level];
        _frameTexture->setCompressedPixelData(
            frame->blocks.data() + info.offset,
            static_cast<GLsizei>(info.byteSize),
            static_cast<int>(level)
        );
    }
    _uploadedFrame = frame->frameIndex;
}

void TextureVideoPlayer::reload() {
    deinitialize();
    initialize();
}

ghoul::opengl::Texture* TextureVideoPlayer::frameTexture() const {
    return _frameTexture.get();
}

glm::ivec2 TextureVideoPlayer::resolution() const {
    ghoul_assert(_decoder, "Player must be initialized");
    const TextureVideoStream::Metadata& meta = _decoder->stream().metadata();
    return glm::ivec2(meta.width, meta.height);
}

bool TextureVideoPlayer::hasCurrentFrame() const {
    return _uploadedFrame.has_value();
}

bool TextureVideoPlayer::isActive() const {
    if (!_settings.hideOutsideRange ||
        _settings.mode != PlaybackMode::MapToSimulationTime)
    {
        return true;
    }
    const double now = global::timeManager->time().j2000Seconds();
    return now >= _settings.startJ2000 && now <= _settings.endJ2000;
}

void TextureVideoPlayer::uploadTransparentFrame() {
    // An all-zero BC7 or BC3 block decodes to transparent black
    for (uint32_t level = 0; level < _nUploadLevels; level++) {
        const TextureVideoStream::LevelInfo& info = _decoder->stream().levels()[level];
        const std::vector<std::byte> zeros =
            std::vector<std::byte>(info.byteSize, std::byte(0));
        _frameTexture->setCompressedPixelData(
            zeros.data(),
            static_cast<GLsizei>(info.byteSize),
            static_cast<int>(level)
        );
    }
    _uploadedFrame = std::nullopt;
}

uint32_t TextureVideoPlayer::targetFrameForNow() const {
    const uint32_t frameCount = _decoder->stream().metadata().frameCount;
    switch (_settings.mode) {
        case PlaybackMode::MapToSimulationTime:
            return texturevideo::frameIndexFromSimTime(
                global::timeManager->time().j2000Seconds(),
                _settings.startJ2000,
                _settings.endJ2000,
                frameCount
            );
        case PlaybackMode::RealTimeLoop:
            if (!_isPlaying) {
                const uint32_t frame = static_cast<uint32_t>(_pausedFramePosition);
                return std::min(frame, frameCount - 1);
            }
            return texturevideo::frameIndexRealTime(
                global::windowDelegate->applicationTime(),
                _playbackStartTime,
                _fps,
                frameCount,
                _settings.loop
            );
    }
    throw ghoul::MissingCaseException();
}

} // namespace openspace
