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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCACHE___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCACHE___H__

#include <modules/globebrowsing/src/geojson/geojsonproperties.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace openspace { struct Geodetic3; }

/**
 * A binary (BEVE) load cache for GeoJSON files, storing the fully derived per-feature
 * data so that warm loads skip JSON parsing, GEOS geometry construction, validation,
 * and triangulation entirely. Serialized with glaze (confined to the .cpp); the structs
 * here are plain aggregates so they stay reflectable and stable.
 */
namespace openspace::geojson {

/// Bump to invalidate all existing GeoJSON caches (part of the cache key)
inline constexpr int32_t CacheVersion = 1;

/// Radians / meters, decoupled from the engine's Geodetic3 layout
struct CachedGeodetic3 {
    double lat = 0.0;
    double lon = 0.0;
    double height = 0.0;
};

/**
 * Mirrors GeoJsonOverrideProperties. glm types are not glaze-reflectable, so colors are
 * arrays; enums are stored as int32_t; the texture path is a generic string (already
 * absPath-resolved on the cold path, so warm loads need no working-directory swap).
 */
struct CachedOverrideProperties {
    std::optional<std::string> name;
    std::optional<float> opacity;
    std::optional<std::array<float, 3>> color;
    std::optional<float> fillOpacity;
    std::optional<std::array<float, 3>> fillColor;
    std::optional<float> lineWidth;
    std::optional<float> pointSize;
    std::optional<std::string> pointTexture;
    std::optional<int32_t> pointTextureAnchor; // GeoJsonProperties::PointTextureAnchor
    std::optional<bool> extrude;
    std::optional<bool> performShading;
    std::optional<int32_t> altitudeMode;       // GeoJsonProperties::AltitudeMode
    std::optional<bool> tessellationEnabled;
    std::optional<bool> useTessellationLevel;
    std::optional<int32_t> tessellationLevel;
    std::optional<float> tessellationDistance;
};

/// One rendered GlobeGeometryFeature plus its SubFeatureProps metadata
struct CachedRenderedFeature {
    int32_t geometryType = 0; // GlobeGeometryFeature::GeometryType
    std::vector<std::vector<CachedGeodetic3>> geoCoordinates;
    std::vector<CachedGeodetic3> triangleCoordinates;
    std::vector<CachedGeodetic3> heightUpdateReferencePoints;
    std::string key;
    // SubFeatureProps metadata, in degrees as stored in the properties today.
    // boundingBoxDiagonal is ellipsoid-dependent and recomputed on load
    std::array<float, 2> centroidLatLong = { 0.f, 0.f };
    std::array<float, 4> boundingboxLatLong = { 0.f, 0.f, 0.f, 0.f };
    std::string identifier; // final, de-duplicated identifier
    std::string name;       // gui name
};

/// One source GeoJSON feature; its override properties are shared by all the rendered
/// features it was split into
struct CachedSourceFeature {
    CachedOverrideProperties overrides;
    std::vector<CachedRenderedFeature> rendered;
};

struct GeoJsonCacheFile {
    int32_t version = CacheVersion;
    /// Belt-and-braces: the flag is also part of the cache key
    bool ignoreHeights = false;
    std::vector<CachedSourceFeature> features;
};

/// Returns std::nullopt when the file is missing, corrupt, or truncated, in which case
/// the caller falls back to the cold path
std::optional<GeoJsonCacheFile> loadGeoJsonCache(const std::filesystem::path& cacheFile);

bool saveGeoJsonCache(const GeoJsonCacheFile& data,
    const std::filesystem::path& cacheFile);

CachedOverrideProperties toCached(const GeoJsonOverrideProperties& props);
GeoJsonOverrideProperties fromCached(const CachedOverrideProperties& props);

std::vector<CachedGeodetic3> toCached(const std::vector<Geodetic3>& coords);
std::vector<Geodetic3> fromCached(const std::vector<CachedGeodetic3>& coords);

} // namespace openspace::geojson

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCACHE___H__
