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

#ifndef __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOPLAYER___H__
#define __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOPLAYER___H__

#include <openspace/properties/propertyowner.h>

#include <openspace/properties/misc/triggerproperty.h>
#include <ghoul/glm.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

namespace ghoul::opengl { class Texture; }

namespace openspace {

class AsyncVideoFrameDecoder;

/**
 * Plays a preprocessed Basis/KTX2 texture video into a single block-compressed OpenGL
 * texture. Frames are transcoded ahead of time on a worker thread and uploaded with
 * compressed sub-image updates when the current frame changes. In
 * `MapToSimulationTime` mode the displayed frame is a pure function of the (cluster
 * synchronized) simulation time, so no explicit synchronization is required; in
 * `RealTimeLoop` mode playback runs off the SGCT-synchronized application time.
 */
class TextureVideoPlayer : public PropertyOwner {
public:
    enum class PlaybackMode {
        MapToSimulationTime = 0,
        RealTimeLoop
    };

    struct Settings {
        std::filesystem::path file;
        PlaybackMode mode = PlaybackMode::MapToSimulationTime;
        /// Simulation time of the first frame; required for MapToSimulationTime
        double startJ2000 = 0.0;
        /// Simulation time of the last frame; required for MapToSimulationTime
        double endJ2000 = 0.0;
        bool loop = false;
        /// Overrides the frame rate stored in the file; 0 uses the file metadata
        double fpsOverride = 0.0;
        /// If `true`, `isActive` is `false` outside [startJ2000, endJ2000]
        bool hideOutsideRange = false;
    };

    explicit TextureVideoPlayer(Settings settings);
    ~TextureVideoPlayer() override;

    /// Loads the file and creates the frame texture; must be called on the GL thread
    void initialize();
    void deinitialize();
    bool isInitialized() const;

    /// Advances playback and uploads a new frame to the texture if one is available;
    /// must be called on the GL thread once per frame
    void update();

    /// Destroys and recreates the stream and texture
    void reload();

    /**
     * The frame texture. The pointer is stable between `initialize` and `deinitialize`
     * and the texture is never resized, so the GL name remains valid.
     */
    ghoul::opengl::Texture* frameTexture() const;

    /// The video resolution; valid after `initialize`
    glm::ivec2 resolution() const;

    /// `true` once at least one video frame has been uploaded to the texture
    bool hasCurrentFrame() const;

    /**
     * Returns `false` when the layer should be hidden, i.e. when `HideOutsideRange` is
     * set and the simulation time is outside [startJ2000, endJ2000].
     */
    bool isActive() const;

private:
    uint32_t targetFrameForNow() const;
    void uploadTransparentFrame();

    Settings _settings;
    double _fps = 24.0;

    std::unique_ptr<AsyncVideoFrameDecoder> _decoder;
    std::unique_ptr<ghoul::opengl::Texture> _frameTexture;
    /// The number of mipmap levels actually allocated for the texture, which may be
    /// smaller than the number of levels in the file
    uint32_t _nUploadLevels = 0;
    std::optional<uint32_t> _uploadedFrame;
    std::optional<uint32_t> _previousTarget;

    // RealTimeLoop state
    bool _isPlaying = true;
    double _playbackStartTime = 0.0;
    double _pausedFramePosition = 0.0;
    TriggerProperty _play;
    TriggerProperty _pause;
    TriggerProperty _goToStart;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_VIDEO___TEXTUREVIDEOPLAYER___H__
