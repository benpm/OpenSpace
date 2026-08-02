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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEODETICPATCH___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEODETICPATCH___H__

#include <modules/globebrowsing/src/basictypes.h>
#include <openspace/util/geodetic.h>

namespace openspace {

struct TileIndex;

class GeodeticPatch {
public:
    GeodeticPatch(double centerLat, double centerLon, double halfSizeLat,
        double halfSizeLon);

    GeodeticPatch(const Geodetic2& center, const Geodetic2& halfSize);

    GeodeticPatch(const GeodeticPatch& patch) = default;

    explicit GeodeticPatch(const TileIndex& tileIndex);

    void setCenter(Geodetic2 center);
    void setHalfSize(Geodetic2 halfSize);

    /**
     * Returns the latitude boundary which is closest to the equator.
     */
    double edgeLatitudeNearestEquator() const;

    /**
     * Returns `true` if the center above the equator.
     */
    bool isNorthern() const;

    Geodetic2 corner(Quad q) const;

    double minLat() const;
    double maxLat() const;
    double minLon() const;
    double maxLon() const;

    /**
     * Returns `true` if the specified coordinate is contained within the patch.
     */
    bool contains(const Geodetic2& p) const;

    /**
     * Returns `true` if this patch and \p other overlap with nonzero area. The test is
     * not longitude-wrap aware; both patches are assumed to lie within [-pi, pi]
     * longitude without crossing the antimeridian.
     */
    bool overlaps(const GeodeticPatch& other) const;

    /**
     * Clamps a point to the patch region.
     */
    Geodetic2 clamp(const Geodetic2& p) const;

    /**
     * Returns the corner of the patch that is closest to the given point p.
     */
    Geodetic2 closestCorner(const Geodetic2& p) const;

    /**
     * Returns a point on the patch that minimizes the great-circle distance to the given
     * point p.
     */
    Geodetic2 closestPoint(const Geodetic2& p) const;

    /**
     * Returns the minimum tile level of the patch (based on largest side).
     */
    double minimumTileLevel() const;

    /**
     * Returns the maximum level of the patch (based on smallest side).
     */
    double maximumTileLevel() const;

    const Geodetic2& center() const;
    const Geodetic2& halfSize() const;
    Geodetic2 size() const;

private:
    Geodetic2 _center;
    Geodetic2 _halfSize;
};

/**
 * Computes the transform mapping a tile's local UV coordinates in [0,1]^2 into the UV
 * space of a texture covering exactly the geodetic \p extent, such that
 * `textureUV = uvOffset + uvScale * tileUV`. u=0 corresponds to `extent.minLon()` and
 * v=0 to `extent.minLat()`, with v growing northwards. For tiles (partially) outside
 * the extent the resulting UVs fall outside [0,1].
 */
TileUvTransform tileUvTransformForExtent(const GeodeticPatch& extent,
    const TileIndex& tileIndex);

/**
 * Returns the highest tile level at which a tile still covers at least \p tileSize
 * pixels of a texture with \p resolution pixels spanning \p extent. Subdividing beyond
 * the returned level gains no additional detail from the texture.
 */
int maximumLevelForResolution(const GeodeticPatch& extent, const glm::ivec2& resolution,
    int tileSize = 512);

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEODETICPATCH___H__
