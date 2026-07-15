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

#include <modules/globebrowsing/src/geojson/geojsoncache.h>

#include <openspace/util/geodetic.h>
#include <ghoul/logging/logmanager.h>
#include <glaze/glaze.hpp>
#include <fstream>
#include <iterator>

namespace {
    constexpr std::string_view _loggerCat = "GeoJsonCache";
} // namespace

namespace openspace::geojson {

std::optional<GeoJsonCacheFile> loadGeoJsonCache(const std::filesystem::path& cacheFile)
{
    std::ifstream file = std::ifstream(cacheFile, std::ios::binary);
    if (!file.good()) {
        return std::nullopt;
    }

    const std::string buffer = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    GeoJsonCacheFile res;
    const glz::error_ctx ec = glz::read_beve(res, buffer);
    if (ec) {
        LWARNING(std::format(
            "Failed reading GeoJson cache file '{}', falling back to parsing the "
            "source file", cacheFile
        ));
        return std::nullopt;
    }
    return res;
}

bool saveGeoJsonCache(const GeoJsonCacheFile& data,
                      const std::filesystem::path& cacheFile)
{
    std::string buffer;
    const glz::error_ctx ec = glz::write_beve(data, buffer);
    if (ec) {
        LWARNING(std::format("Failed serializing GeoJson cache for '{}'", cacheFile));
        return false;
    }

    std::ofstream file = std::ofstream(cacheFile, std::ios::binary);
    if (!file.good()) {
        LWARNING(std::format("Failed opening GeoJson cache file '{}'", cacheFile));
        return false;
    }
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return file.good();
}

std::optional<GeoJsonHeightsCacheFile> loadGeoJsonHeightsCache(
                                                  const std::filesystem::path& cacheFile)
{
    std::ifstream file = std::ifstream(cacheFile, std::ios::binary);
    if (!file.good()) {
        return std::nullopt;
    }

    const std::string buffer = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    GeoJsonHeightsCacheFile res;
    const glz::error_ctx ec = glz::read_beve(res, buffer);
    if (ec) {
        LWARNING(std::format(
            "Failed reading GeoJson heights cache file '{}'; heights will be "
            "re-sampled from the height map", cacheFile
        ));
        return std::nullopt;
    }
    return res;
}

bool saveGeoJsonHeightsCache(const GeoJsonHeightsCacheFile& data,
                             const std::filesystem::path& cacheFile)
{
    std::string buffer;
    const glz::error_ctx ec = glz::write_beve(data, buffer);
    if (ec) {
        LWARNING(std::format(
            "Failed serializing GeoJson heights cache for '{}'", cacheFile
        ));
        return false;
    }

    std::ofstream file = std::ofstream(cacheFile, std::ios::binary);
    if (!file.good()) {
        LWARNING(std::format(
            "Failed opening GeoJson heights cache file '{}'", cacheFile
        ));
        return false;
    }
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return file.good();
}

CachedOverrideProperties toCached(const GeoJsonOverrideProperties& props) {
    const auto color = [](const std::optional<glm::vec3>& c) {
        return c.has_value() ?
            std::optional(std::array<float, 3>{ c->x, c->y, c->z }) :
            std::nullopt;
    };

    CachedOverrideProperties res;
    res.name = props.name;
    res.opacity = props.opacity;
    res.color = color(props.color);
    res.fillOpacity = props.fillOpacity;
    res.fillColor = color(props.fillColor);
    res.lineWidth = props.lineWidth;
    res.pointSize = props.pointSize;
    if (props.pointTexture.has_value()) {
        res.pointTexture = props.pointTexture->generic_string();
    }
    if (props.pointTextureAnchor.has_value()) {
        res.pointTextureAnchor = static_cast<int32_t>(*props.pointTextureAnchor);
    }
    res.extrude = props.extrude;
    res.performShading = props.performShading;
    if (props.altitudeMode.has_value()) {
        res.altitudeMode = static_cast<int32_t>(*props.altitudeMode);
    }
    res.tessellationEnabled = props.tessellationEnabled;
    res.useTessellationLevel = props.useTessellationLevel;
    res.tessellationLevel = props.tessellationLevel;
    res.tessellationDistance = props.tessellationDistance;
    return res;
}

GeoJsonOverrideProperties fromCached(const CachedOverrideProperties& props) {
    const auto color = [](const std::optional<std::array<float, 3>>& c) {
        return c.has_value() ?
            std::optional(glm::vec3((*c)[0], (*c)[1], (*c)[2])) :
            std::nullopt;
    };

    GeoJsonOverrideProperties res;
    res.name = props.name;
    res.opacity = props.opacity;
    res.color = color(props.color);
    res.fillOpacity = props.fillOpacity;
    res.fillColor = color(props.fillColor);
    res.lineWidth = props.lineWidth;
    res.pointSize = props.pointSize;
    if (props.pointTexture.has_value()) {
        res.pointTexture = std::filesystem::path(*props.pointTexture);
    }
    if (props.pointTextureAnchor.has_value()) {
        res.pointTextureAnchor = static_cast<GeoJsonProperties::PointTextureAnchor>(
            *props.pointTextureAnchor
        );
    }
    res.extrude = props.extrude;
    res.performShading = props.performShading;
    if (props.altitudeMode.has_value()) {
        res.altitudeMode = static_cast<GeoJsonProperties::AltitudeMode>(
            *props.altitudeMode
        );
    }
    res.tessellationEnabled = props.tessellationEnabled;
    res.useTessellationLevel = props.useTessellationLevel;
    res.tessellationLevel = props.tessellationLevel;
    res.tessellationDistance = props.tessellationDistance;
    return res;
}

std::vector<CachedGeodetic3> toCached(const std::vector<Geodetic3>& coords) {
    std::vector<CachedGeodetic3> res;
    res.reserve(coords.size());
    for (const Geodetic3& c : coords) {
        res.push_back({
            .lat = c.geodetic2.lat,
            .lon = c.geodetic2.lon,
            .height = c.height
        });
    }
    return res;
}

std::vector<Geodetic3> fromCached(const std::vector<CachedGeodetic3>& coords) {
    std::vector<Geodetic3> res;
    res.reserve(coords.size());
    for (const CachedGeodetic3& c : coords) {
        res.push_back({ .geodetic2 = { .lat = c.lat, .lon = c.lon }, .height = c.height });
    }
    return res;
}

} // namespace openspace::geojson
