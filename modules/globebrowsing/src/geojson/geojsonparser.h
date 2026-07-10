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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONPARSER___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONPARSER___H__

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace geos::geom { class Geometry; }

/**
 * A fast GeoJSON parser built on the glaze library, replacing the nlohmann-DOM-based
 * geos::io::GeoJSONReader for loading. Glaze usage is confined to the .cpp; this header
 * only exposes standard types.
 */
namespace openspace::geojson {

/// One value from a feature's `properties` object. monostate represents null or a value
/// type that no consumer cares about (nested objects, non-numeric arrays)
using PropertyValue = std::variant<
    std::monostate, bool, double, std::string, std::vector<double>
>;
using PropertyMap = std::vector<std::pair<std::string, PropertyValue>>;

enum class GeometryKind {
    None = 0,
    Point,
    MultiPoint,
    LineString,
    MultiLineString,
    Polygon,
    MultiPolygon,
    GeometryCollection
};

/// One GeoJSON position: [lon, lat] or [lon, lat, alt]. z is NaN when absent, matching
/// geos Coordinate semantics (toGeodetic maps NaN heights to 0)
struct Position {
    double x = 0.0;
    double y = 0.0;
    double z = std::numeric_limits<double>::quiet_NaN();
};

struct ParsedGeometry {
    GeometryKind kind = GeometryKind::None;

    /// Nesting normalized to "polygons of rings of positions":
    ///   Point                     -> coords[0][0][0]
    ///   MultiPoint / LineString   -> coords[0][0]
    ///   MultiLineString / Polygon -> coords[0][i]
    ///   MultiPolygon              -> coords[i][j]
    std::vector<std::vector<std::vector<Position>>> coords;

    /// Only used for GeometryCollection
    std::vector<ParsedGeometry> children;
};

struct ParsedFeature {
    /// kind == None represents a null geometry
    ParsedGeometry geometry;
    PropertyMap properties;
};

struct ParsedGeoJson {
    std::vector<ParsedFeature> features;
};

/**
 * Parse GeoJSON text into features. Accepts a FeatureCollection, a single Feature, or a
 * bare geometry (parity with geos::io::GeoJSONReader::readFeatures).
 *
 * Throws ghoul::RuntimeError with a formatted parse error (including location context)
 * on malformed input.
 */
ParsedGeoJson parseGeoJson(const std::string& content);

/**
 * Build a geos geometry from a parsed one, for validation and triangulation. Unlike
 * geos' own GeoJSON reader, z values are preserved for all geometry types.
 */
std::unique_ptr<geos::geom::Geometry> buildGeosGeometry(const ParsedGeometry& geometry);

} // namespace openspace::geojson

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONPARSER___H__
