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

#include <modules/globebrowsing/src/basictypes.h>
#include <modules/globebrowsing/src/geodeticpatch.h>
#include <modules/globebrowsing/src/tileindex.h>
#include <openspace/util/geodetic.h>
#include <ghoul/glm.h>
#include <cmath>

using namespace openspace;

namespace {
    constexpr float Epsilon = 1e-5f;

    bool approx(float lhs, float rhs) {
        return std::abs(lhs - rhs) < Epsilon;
    }

    bool approx(const glm::vec2& lhs, const glm::vec2& rhs) {
        return approx(lhs.x, rhs.x) && approx(lhs.y, rhs.y);
    }

    GeodeticPatch fullGlobe() {
        return GeodeticPatch(0.0, 0.0, glm::half_pi<double>(), glm::pi<double>());
    }
} // namespace

TEST_CASE("RegionalTileUv: Full-globe extent matches whole-globe formula",
          "[regionaltileuv]")
{
    // For the full-globe extent the transform must reduce to the equirectangular
    // mapping used by VideoTileProvider: uvScale = (2^-L, 2^-(L-1)),
    // uvOffset = (x / 2^L, ratioY * (2^(L-1) - y - 1))
    const GeodeticPatch extent = fullGlobe();

    struct { uint32_t x; uint32_t y; uint8_t level; } cases[] = {
        { 0, 0, 2 },
        { 3, 1, 2 },
        { 17, 9, 5 }
    };

    for (const auto& c : cases) {
        const TileUvTransform uv = tileUvTransformForExtent(
            extent,
            TileIndex(c.x, c.y, c.level)
        );
        const float nTilesX = std::pow(2.f, c.level);
        const float nTilesY = std::pow(2.f, c.level - 1);
        const glm::vec2 ratios = glm::vec2(1.f / nTilesX, 1.f / nTilesY);

        CHECK(approx(uv.uvScale, ratios));
        CHECK(approx(uv.uvOffset.x, ratios.x * c.x));
        CHECK(approx(uv.uvOffset.y, ratios.y * (nTilesY - c.y - 1.f)));
    }
}

TEST_CASE("RegionalTileUv: Extent equal to tile is identity", "[regionaltileuv]") {
    const TileIndex index = TileIndex(1, 1, 2);
    const GeodeticPatch extent = GeodeticPatch(index);

    const TileUvTransform uv = tileUvTransformForExtent(extent, index);
    CHECK(approx(uv.uvOffset, glm::vec2(0.f, 0.f)));
    CHECK(approx(uv.uvScale, glm::vec2(1.f, 1.f)));
}

TEST_CASE("RegionalTileUv: Children map to quadrants with northward v",
          "[regionaltileuv]")
{
    // The extent is a level-2 tile; its four level-3 children each cover one quadrant.
    // Tile y grows southwards while v grows northwards, so the north children (smaller
    // y) must land at v = 0.5
    const GeodeticPatch extent = GeodeticPatch(TileIndex(2, 1, 2));

    const TileUvTransform nw = tileUvTransformForExtent(extent, TileIndex(4, 2, 3));
    const TileUvTransform ne = tileUvTransformForExtent(extent, TileIndex(5, 2, 3));
    const TileUvTransform sw = tileUvTransformForExtent(extent, TileIndex(4, 3, 3));
    const TileUvTransform se = tileUvTransformForExtent(extent, TileIndex(5, 3, 3));

    CHECK(approx(nw.uvOffset, glm::vec2(0.f, 0.5f)));
    CHECK(approx(ne.uvOffset, glm::vec2(0.5f, 0.5f)));
    CHECK(approx(sw.uvOffset, glm::vec2(0.f, 0.f)));
    CHECK(approx(se.uvOffset, glm::vec2(0.5f, 0.f)));

    for (const TileUvTransform& uv : { nw, ne, sw, se }) {
        CHECK(approx(uv.uvScale, glm::vec2(0.5f, 0.5f)));
    }
}

TEST_CASE("RegionalTileUv: Tiles outside the extent map outside [0,1]",
          "[regionaltileuv]")
{
    // Contiguous-US-like extent
    const double minLat = glm::radians(24.0);
    const double maxLat = glm::radians(49.5);
    const double minLon = glm::radians(-125.0);
    const double maxLon = glm::radians(-66.5);
    const GeodeticPatch extent = GeodeticPatch(
        (minLat + maxLat) / 2.0,
        (minLon + maxLon) / 2.0,
        (maxLat - minLat) / 2.0,
        (maxLon - minLon) / 2.0
    );

    // Level 4: tile spans 2pi/16 ~= 22.5 degrees
    // x = 0 -> lon [-180, -157.5], entirely west of the extent
    const TileUvTransform west = tileUvTransformForExtent(extent, TileIndex(0, 3, 4));
    CHECK(west.uvOffset.x + west.uvScale.x <= 0.f);

    // x = 15 -> lon [157.5, 180], entirely east
    const TileUvTransform east = tileUvTransformForExtent(extent, TileIndex(15, 3, 4));
    CHECK(east.uvOffset.x >= 1.f);

    // y = 7 -> lat [-90, -67.5], entirely south
    const TileUvTransform south = tileUvTransformForExtent(extent, TileIndex(6, 7, 4));
    CHECK(south.uvOffset.y + south.uvScale.y <= 0.f);

    // y = 0 -> lat [67.5, 90], entirely north
    const TileUvTransform north = tileUvTransformForExtent(extent, TileIndex(6, 0, 4));
    CHECK(north.uvOffset.y >= 1.f);
}

TEST_CASE("RegionalTileUv: Parent and child transforms are consistent",
          "[regionaltileuv]")
{
    // Sampling the child through its own transform must equal sampling the parent at
    // the child's sub-rectangle: T_c(uv) == T_p(childOrigin + 0.5 * uv)
    const GeodeticPatch extent = GeodeticPatch(0.3, 0.8, 0.4, 0.9);

    const TileIndex parent = TileIndex(5, 2, 3);
    struct { TileIndex index; glm::vec2 origin; } children[] = {
        // y even -> north child -> upper half of the parent in v
        { TileIndex(10, 4, 4), glm::vec2(0.f, 0.5f) },
        { TileIndex(11, 4, 4), glm::vec2(0.5f, 0.5f) },
        { TileIndex(10, 5, 4), glm::vec2(0.f, 0.f) },
        { TileIndex(11, 5, 4), glm::vec2(0.5f, 0.f) }
    };

    const TileUvTransform parentUv = tileUvTransformForExtent(extent, parent);
    for (const auto& c : children) {
        const TileUvTransform childUv = tileUvTransformForExtent(extent, c.index);
        for (const glm::vec2& uv : { glm::vec2(0.f, 0.f), glm::vec2(0.3f, 0.7f),
                                     glm::vec2(1.f, 1.f) })
        {
            const glm::vec2 viaChild = childUv.uvOffset + childUv.uvScale * uv;
            const glm::vec2 viaParent =
                parentUv.uvOffset + parentUv.uvScale * (c.origin + 0.5f * uv);
            CHECK(approx(viaChild, viaParent));
        }
    }
}

TEST_CASE("RegionalTileUv: GeodeticPatch overlaps", "[regionaltileuv]") {
    const GeodeticPatch a = GeodeticPatch(0.0, 0.0, 0.1, 0.1);

    // Disjoint
    const GeodeticPatch far = GeodeticPatch(0.5, 0.5, 0.1, 0.1);
    CHECK_FALSE(a.overlaps(far));
    CHECK_FALSE(far.overlaps(a));

    // Edge-touching patches share zero area
    const GeodeticPatch touching = GeodeticPatch(0.2, 0.0, 0.1, 0.1);
    CHECK_FALSE(a.overlaps(touching));
    CHECK_FALSE(touching.overlaps(a));

    // Containment, both directions
    const GeodeticPatch inner = GeodeticPatch(0.0, 0.0, 0.05, 0.05);
    CHECK(a.overlaps(inner));
    CHECK(inner.overlaps(a));

    // Partial overlap
    const GeodeticPatch partial = GeodeticPatch(0.15, 0.15, 0.1, 0.1);
    CHECK(a.overlaps(partial));
    CHECK(partial.overlaps(a));

    // Both crossing the equator and prime meridian
    const GeodeticPatch wide = GeodeticPatch(0.05, -0.05, 0.2, 0.2);
    CHECK(a.overlaps(wide));
    CHECK(wide.overlaps(a));
}

TEST_CASE("RegionalTileUv: maximumLevelForResolution", "[regionaltileuv]") {
    // Full-globe 1024x512 texture with 512px tiles: level 1 tiles already cover 512
    // source pixels, +1 margin and the MinSplitDepth clamp both give 2
    CHECK(maximumLevelForResolution(fullGlobe(), glm::ivec2(1024, 512)) == 2);

    // Doubling the resolution adds exactly one level
    CHECK(maximumLevelForResolution(fullGlobe(), glm::ivec2(2048, 1024)) == 3);
    CHECK(maximumLevelForResolution(fullGlobe(), glm::ivec2(4096, 2048)) == 4);

    // A small extent at high resolution needs a deeper level than the same resolution
    // over the whole globe
    const GeodeticPatch small = GeodeticPatch(
        0.0, 0.0,
        glm::radians(5.0), glm::radians(5.0)
    );
    const int smallLevel = maximumLevelForResolution(small, glm::ivec2(2048, 2048));
    const int globeLevel = maximumLevelForResolution(fullGlobe(), glm::ivec2(2048, 2048));
    CHECK(smallLevel > globeLevel);

    // Degenerate tiny texture still clamps to the minimum split depth
    CHECK(maximumLevelForResolution(fullGlobe(), glm::ivec2(16, 8)) == 2);
}
