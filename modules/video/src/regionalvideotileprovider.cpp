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

#include <modules/video/include/regionalvideotileprovider.h>

#include <openspace/documentation/documentation.h>
#include <openspace/util/time.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/misc/profiling.h>
#include <limits>

namespace {
    // Renders a preprocessed texture video over a limited geographic region of a globe.
    //
    // The video must be a Basis Universal ETC1S encoded KTX2 file, which can be created
    // from a regular video file with the `support/texturevideo/convert_video.py`
    // script. The area outside the given `Extent` is fully transparent, so the layers
    // below remain visible there. The extent must not cross the antimeridian (+/- 180
    // degrees longitude).
    //
    // The video can either be mapped to the simulation time (`PlaybackMode`
    // MapToSimulationTime), which requires `StartTime` and `EndTime`, or played back in
    // real time (`PlaybackMode` RealTimeLoop). If no playback mode is specified,
    // MapToSimulationTime is used when both `StartTime` and `EndTime` are provided and
    // RealTimeLoop otherwise.
    //
    // This layer type is intended for the `ColorLayers` group, whose blending respects
    // the transparency outside the extent. The `Overlays` group applies the normal
    // blending twice and is not recommended.
    struct [[codegen::Dictionary(RegionalVideoTileProvider)]] Parameters {
        // The preprocessed Basis/KTX2 texture video file
        std::filesystem::path video;

        // The geographic rectangle covered by the video, in degrees. The rectangle must
        // not cross the antimeridian
        struct Extent {
            // The westernmost longitude covered by the video, in degrees
            double minLon [[codegen::inrange(-180.0, 180.0)]];

            // The southernmost latitude covered by the video, in degrees
            double minLat [[codegen::inrange(-90.0, 90.0)]];

            // The easternmost longitude covered by the video, in degrees. Must be
            // greater than MinLon
            double maxLon [[codegen::inrange(-180.0, 180.0)]];

            // The northernmost latitude covered by the video, in degrees. Must be
            // greater than MinLat
            double maxLat [[codegen::inrange(-90.0, 90.0)]];
        };
        Extent extent;

        // The simulation time at which the video starts. Required for the
        // MapToSimulationTime playback mode
        std::optional<std::string> startTime [[codegen::datetime()]];

        // The simulation time at which the video ends. Required for the
        // MapToSimulationTime playback mode
        std::optional<std::string> endTime [[codegen::datetime()]];

        enum class PlaybackMode {
            MapToSimulationTime,
            RealTimeLoop
        };
        // Whether the video should be played back mapped to the simulation time or in
        // real time
        std::optional<PlaybackMode> playbackMode;

        // If true, the video loops. Only used in the RealTimeLoop playback mode
        std::optional<bool> loopVideo;

        // Overrides the frame rate stored in the video file. Only used in the
        // RealTimeLoop playback mode
        std::optional<double> framesPerSecond [[codegen::greater(0.0)]];

        // If true, the layer is fully transparent when the simulation time is outside
        // the [StartTime, EndTime] interval instead of freezing on the first/last frame
        std::optional<bool> hideOutsideRange;

        // Overrides the maximum tile level, which is otherwise derived from the video
        // resolution and the extent size
        std::optional<int> maxLevel [[codegen::greaterequal(2)]];
    };

    openspace::GeodeticPatch extentFromParameters(const Parameters::Extent& e) {
        if (e.minLon >= e.maxLon || e.minLat >= e.maxLat) {
            throw ghoul::RuntimeError(
                "Invalid Extent: MinLon/MinLat must be less than MaxLon/MaxLat. Note "
                "that extents crossing the antimeridian are not supported"
            );
        }
        const double minLat = glm::radians(e.minLat);
        const double maxLat = glm::radians(e.maxLat);
        const double minLon = glm::radians(e.minLon);
        const double maxLon = glm::radians(e.maxLon);
        return openspace::GeodeticPatch(
            (minLat + maxLat) / 2.0,
            (minLon + maxLon) / 2.0,
            (maxLat - minLat) / 2.0,
            (maxLon - minLon) / 2.0
        );
    }

    openspace::TextureVideoPlayer::Settings playerSettings(const Parameters& p) {
        using PlaybackMode = openspace::TextureVideoPlayer::PlaybackMode;

        const bool hasTimes = p.startTime.has_value() && p.endTime.has_value();
        PlaybackMode mode =
            hasTimes ? PlaybackMode::MapToSimulationTime : PlaybackMode::RealTimeLoop;
        if (p.playbackMode.has_value()) {
            switch (*p.playbackMode) {
                case Parameters::PlaybackMode::MapToSimulationTime:
                    mode = PlaybackMode::MapToSimulationTime;
                    break;
                case Parameters::PlaybackMode::RealTimeLoop:
                    mode = PlaybackMode::RealTimeLoop;
                    break;
            }
        }

        openspace::TextureVideoPlayer::Settings settings;
        settings.file = p.video;
        settings.mode = mode;
        settings.loop = p.loopVideo.value_or(false);
        settings.fpsOverride = p.framesPerSecond.value_or(0.0);
        settings.hideOutsideRange = p.hideOutsideRange.value_or(false);

        if (mode == PlaybackMode::MapToSimulationTime) {
            if (!hasTimes) {
                throw ghoul::RuntimeError(
                    "StartTime and EndTime are required when the playback mode is "
                    "MapToSimulationTime"
                );
            }
            settings.startJ2000 = openspace::Time::convertTime(*p.startTime);
            settings.endJ2000 = openspace::Time::convertTime(*p.endTime);
            if (settings.endJ2000 <= settings.startJ2000) {
                throw ghoul::RuntimeError("EndTime must be after StartTime");
            }
        }
        return settings;
    }
} // namespace
#include "regionalvideotileprovider_codegen.cpp"

namespace openspace {

Documentation RegionalVideoTileProvider::Documentation() {
    return codegen::doc<Parameters>("video_tileprovider_regionalvideo");
}

RegionalVideoTileProvider::RegionalVideoTileProvider(const ghoul::Dictionary& dictionary)
    : _extent(0.0, 0.0, 0.0, 0.0)
    , _player([&dictionary]() {
        const Parameters p = codegen::bake<Parameters>(dictionary);
        return playerSettings(p);
    }())
{
    ZoneScoped;

    const Parameters p = codegen::bake<Parameters>(dictionary);
    _extent = extentFromParameters(p.extent);
    _maxLevelOverride = p.maxLevel;
    addPropertySubOwner(_player);
}

Tile RegionalVideoTileProvider::tile(const TileIndex&) {
    ZoneScoped;

    if (!_player.isInitialized() || !_player.frameTexture()) {
        return Tile();
    }

    // Every tile shares the single frame texture; geographic confinement comes entirely
    // from the UV transform and the transparent clamp-to-border sampling. The texture
    // is transparent until the first frame is uploaded, so returning OK from the start
    // is safe and avoids the engine's fallback to an uninitialized default tile
    return Tile { _player.frameTexture(), std::nullopt, Tile::Status::OK };
}

Tile::Status RegionalVideoTileProvider::tileStatus(const TileIndex& tileIndex) {
    if (tileIndex.level > maxLevel()) {
        return Tile::Status::OutOfRange;
    }
    if (!_extent.overlaps(GeodeticPatch(tileIndex))) {
        // Keeps this layer from requesting chunk subdivision outside the extent
        return Tile::Status::OutOfRange;
    }
    return _player.hasCurrentFrame() ? Tile::Status::OK : Tile::Status::Unavailable;
}

TileDepthTransform RegionalVideoTileProvider::depthTransform() {
    return { 0.f, 1.f };
}

void RegionalVideoTileProvider::update() {
    _player.update();
}

void RegionalVideoTileProvider::reset() {
    _player.reload();
}

ChunkTile RegionalVideoTileProvider::chunkTile(TileIndex tileIndex, int parents,
                                               int maxParents)
{
    // The UV transform is a pure function of the tile index, so ascending recomputes it
    // from the parent's own patch instead of tracking incremental offsets
    auto ascendToParent = [this](TileIndex& ti, TileUvTransform& uv) {
        ti.x /= 2;
        ti.y /= 2;
        ti.level--;
        uv = tileUvTransformForExtent(_extent, ti);
    };

    TileUvTransform uvTransform = tileUvTransformForExtent(_extent, tileIndex);
    return traverseTree(tileIndex, parents, maxParents, ascendToParent, uvTransform);
}

int RegionalVideoTileProvider::minLevel() {
    return 1;
}

int RegionalVideoTileProvider::maxLevel() {
    return _maxLevelOverride.value_or(_maxLevel);
}

float RegionalVideoTileProvider::noDataValueAsFloat() {
    return std::numeric_limits<float>::min();
}

void RegionalVideoTileProvider::internalInitialize() {
    _player.initialize();
    _maxLevel = maximumLevelForResolution(_extent, _player.resolution());
}

void RegionalVideoTileProvider::internalDeinitialize() {
    _player.deinitialize();
}

} // namespace openspace
