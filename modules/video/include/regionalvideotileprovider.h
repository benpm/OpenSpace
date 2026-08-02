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

#ifndef __OPENSPACE_MODULE_VIDEO___REGIONALVIDEOTILEPROVIDER___H__
#define __OPENSPACE_MODULE_VIDEO___REGIONALVIDEOTILEPROVIDER___H__

#include <modules/globebrowsing/src/tileprovider/tileprovider.h>

#include <modules/globebrowsing/src/basictypes.h>
#include <modules/globebrowsing/src/geodeticpatch.h>
#include <modules/globebrowsing/src/tileindex.h>
#include <modules/video/include/texturevideoplayer.h>
#include <optional>

namespace openspace {

struct ChunkTile;

/**
 * Renders a preprocessed Basis/KTX2 texture video over a limited longitude/latitude
 * region of a globe. The frame texture uses clamp-to-border sampling with a transparent
 * border, so every tile can share the single frame texture; tiles outside the extent
 * simply sample the transparent border through UVs outside [0,1].
 */
class RegionalVideoTileProvider : public TileProvider {
public:
    explicit RegionalVideoTileProvider(const ghoul::Dictionary& dictionary);

    void update() override final;
    void reset() override final;
    int minLevel() override final;
    int maxLevel() override final;
    float noDataValueAsFloat() override final;
    ChunkTile chunkTile(TileIndex tileIndex, int parents, int maxParents = 1337) override;
    Tile tile(const TileIndex& tileIndex) override final;
    Tile::Status tileStatus(const TileIndex& tileIndex) override final;
    TileDepthTransform depthTransform() override final;

    static openspace::Documentation Documentation();

private:
    void internalInitialize() override final;
    void internalDeinitialize() override final;

    /// The geographic region covered by the video, in radians
    GeodeticPatch _extent;
    std::optional<int> _maxLevelOverride;
    /// Derived from the video resolution and extent size at initialization
    int _maxLevel = 2;

    TextureVideoPlayer _player;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_VIDEO___REGIONALVIDEOTILEPROVIDER___H__
