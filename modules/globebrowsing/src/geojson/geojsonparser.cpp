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

#include <modules/globebrowsing/src/geojson/geojsonparser.h>

#include <ghoul/misc/exception.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/LineString.h>
#include <geos/geom/Point.h>
#include <geos/geom/Polygon.h>
#include <glaze/glaze.hpp>
#include <format>
#include <map>
#include <optional>

// Raw glaze document types. The polymorphic parts ("coordinates" depends on the
// geometry type, property values can be anything) are deferred with glz::raw_json and
// parsed in a second, exactly-typed pass. In a named namespace because glaze's
// compile-time reflection requires types with linkage
namespace openspace::geojson::internal {

struct RawGeometry {
    std::string type;
    glz::raw_json coordinates;
    /// GeometryCollection only
    glz::raw_json geometries;
};

struct RawFeature {
    std::string type;
    /// JSON null -> nullopt
    std::optional<RawGeometry> geometry;
    std::optional<std::map<std::string, glz::raw_json>> properties;
};

struct RawFeatureCollection {
    std::string type;
    std::vector<RawFeature> features;
};

/**
 * Minimal struct to read only the top-level "type" member.
 */
struct TypeProbe {
    std::string type;
};

} // namespace openspace::geojson::internal

namespace {

    constexpr glz::opts ReadOpts = { .error_on_unknown_keys = false };

    using namespace openspace;
    using namespace openspace::geojson::internal;

    template <typename T>
    T readRaw(const std::string& buffer, std::string_view what) {
        T value{};
        const glz::error_ctx ec = glz::read<ReadOpts>(value, buffer);
        if (ec) {
            throw ghoul::RuntimeError(
                std::format("Error parsing {}: {}", what, glz::format_error(ec, buffer)),
                "GeoJsonParser"
            );
        }
        return value;
    }

    geojson::GeometryKind kindFromType(std::string_view type) {
        using enum geojson::GeometryKind;
        if (type == "Point") {
            return Point;
        }
        if (type == "MultiPoint") {
            return MultiPoint;
        }
        if (type == "LineString") {
            return LineString;
        }
        if (type == "MultiLineString") {
            return MultiLineString;
        }
        if (type == "Polygon") {
            return Polygon;
        }
        if (type == "MultiPolygon") {
            return MultiPolygon;
        }
        if (type == "GeometryCollection") {
            return GeometryCollection;
        }
        return None;
    }

    geojson::Position toPosition(const std::vector<double>& coords) {
        if (coords.size() < 2 || coords.size() > 3) {
            throw ghoul::RuntimeError(
                std::format("Expected two or three coordinates, found {}", coords.size()),
                "GeoJsonParser"
            );
        }
        geojson::Position p = { .x = coords[0], .y = coords[1] };
        if (coords.size() == 3) {
            p.z = coords[2];
        }
        return p;
    }

    std::vector<geojson::Position> toPositions(
                                             const std::vector<std::vector<double>>& list)
    {
        std::vector<geojson::Position> res;
        res.reserve(list.size());
        for (const std::vector<double>& c : list) {
            res.push_back(toPosition(c));
        }
        return res;
    }

    geojson::ParsedGeometry parseGeometry(const RawGeometry& raw) {
        using Coords1 = std::vector<double>;
        using Coords2 = std::vector<Coords1>;
        using Coords3 = std::vector<Coords2>;
        using Coords4 = std::vector<Coords3>;

        geojson::ParsedGeometry res;
        res.kind = kindFromType(raw.type);

        switch (res.kind) {
            case geojson::GeometryKind::Point: {
                const Coords1 c =
                    readRaw<Coords1>(raw.coordinates.str, "Point coordinates");
                if (!c.empty()) {
                    res.coords = { { { toPosition(c) } } };
                }
                break;
            }
            case geojson::GeometryKind::MultiPoint:
            case geojson::GeometryKind::LineString: {
                const Coords2 c = readRaw<Coords2>(raw.coordinates.str, "coordinates");
                res.coords = { { toPositions(c) } };
                break;
            }
            case geojson::GeometryKind::MultiLineString:
            case geojson::GeometryKind::Polygon: {
                const Coords3 c = readRaw<Coords3>(raw.coordinates.str, "coordinates");
                res.coords.emplace_back();
                res.coords.back().reserve(c.size());
                for (const Coords2& part : c) {
                    res.coords.back().push_back(toPositions(part));
                }
                break;
            }
            case geojson::GeometryKind::MultiPolygon: {
                const Coords4 c = readRaw<Coords4>(raw.coordinates.str, "coordinates");
                res.coords.reserve(c.size());
                for (const Coords3& polygon : c) {
                    std::vector<std::vector<geojson::Position>> rings;
                    rings.reserve(polygon.size());
                    for (const Coords2& ring : polygon) {
                        rings.push_back(toPositions(ring));
                    }
                    res.coords.push_back(std::move(rings));
                }
                break;
            }
            case geojson::GeometryKind::GeometryCollection: {
                const std::vector<RawGeometry> children =
                    readRaw<std::vector<RawGeometry>>(
                        raw.geometries.str,
                        "GeometryCollection geometries"
                    );
                res.children.reserve(children.size());
                for (const RawGeometry& child : children) {
                    res.children.push_back(parseGeometry(child));
                }
                break;
            }
            case geojson::GeometryKind::None:
                throw ghoul::RuntimeError(
                    std::format("Unknown geometry type '{}'", raw.type),
                    "GeoJsonParser"
                );
        }
        return res;
    }

    geojson::PropertyValue parsePropertyValue(const std::string& raw) {
        const size_t i = raw.find_first_not_of(" \t\r\n");
        if (i == std::string::npos) {
            return std::monostate();
        }

        switch (raw[i]) {
            case 'n': // null
                return std::monostate();
            case 't':
                return true;
            case 'f':
                return false;
            case '"': {
                std::string s;
                const glz::error_ctx ec = glz::read<ReadOpts>(s, raw);
                return ec ? geojson::PropertyValue(std::monostate()) : std::move(s);
            }
            case '[': {
                // Only arrays of numbers are meaningful downstream (color values)
                std::vector<double> v;
                const glz::error_ctx ec = glz::read<ReadOpts>(v, raw);
                return ec ? geojson::PropertyValue(std::monostate()) : std::move(v);
            }
            case '{':
                // Nested objects have no downstream consumer
                return std::monostate();
            default: {
                double d = 0.0;
                const glz::error_ctx ec = glz::read<ReadOpts>(d, raw);
                return ec ? geojson::PropertyValue(std::monostate()) : d;
            }
        }
    }

    geojson::ParsedFeature parseFeature(RawFeature& raw) {
        geojson::ParsedFeature res;
        if (raw.geometry.has_value()) {
            res.geometry = parseGeometry(*raw.geometry);
        }
        if (raw.properties.has_value()) {
            res.properties.reserve(raw.properties->size());
            for (auto& [key, value] : *raw.properties) {
                res.properties.emplace_back(key, parsePropertyValue(value.str));
            }
        }
        return res;
    }

    std::unique_ptr<geos::geom::CoordinateSequence> makeCoordinateSequence(
                                             const std::vector<geojson::Position>& points)
    {
        auto seq = std::make_unique<geos::geom::CoordinateSequence>();
        seq->reserve(points.size());
        for (const geojson::Position& p : points) {
            seq->add(geos::geom::Coordinate(p.x, p.y, p.z));
        }
        return seq;
    }

    std::unique_ptr<geos::geom::Polygon> makePolygon(
                        const std::vector<std::vector<geojson::Position>>& polygonRings,
                        const geos::geom::GeometryFactory& factory)
    {
        std::unique_ptr<geos::geom::LinearRing> shell;
        std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;
        holes.reserve(polygonRings.size());
        for (const std::vector<geojson::Position>& ring : polygonRings) {
            auto seq = makeCoordinateSequence(ring);
            if (!shell) {
                shell = factory.createLinearRing(std::move(seq));
            }
            else {
                holes.push_back(factory.createLinearRing(std::move(seq)));
            }
        }
        if (!shell) {
            return factory.createPolygon(2);
        }
        else if (holes.empty()) {
            return factory.createPolygon(std::move(shell));
        }
        else {
            return factory.createPolygon(std::move(shell), std::move(holes));
        }
    }

} // namespace

namespace openspace::geojson {

ParsedGeoJson parseGeoJson(const std::string& content) {
    // Cheap probe for the top-level type, to decide what to parse the document as
    TypeProbe probe;
    const glz::error_ctx ec = glz::read<ReadOpts>(probe, content);
    if (ec) {
        throw ghoul::RuntimeError(
            std::format("Error parsing GeoJSON: {}", glz::format_error(ec, content)),
            "GeoJsonParser"
        );
    }

    ParsedGeoJson res;
    if (probe.type == "FeatureCollection") {
        RawFeatureCollection fc = readRaw<RawFeatureCollection>(
            content,
            "FeatureCollection"
        );
        res.features.reserve(fc.features.size());
        for (RawFeature& f : fc.features) {
            res.features.push_back(parseFeature(f));
        }
    }
    else if (probe.type == "Feature") {
        RawFeature f = readRaw<RawFeature>(content, "Feature");
        res.features.push_back(parseFeature(f));
    }
    else if (kindFromType(probe.type) != GeometryKind::None) {
        const RawGeometry g = readRaw<RawGeometry>(content, "geometry");
        ParsedFeature f;
        f.geometry = parseGeometry(g);
        res.features.push_back(std::move(f));
    }
    else {
        throw ghoul::RuntimeError(
            std::format("Unknown GeoJSON type '{}'", probe.type),
            "GeoJsonParser"
        );
    }
    return res;
}

std::unique_ptr<geos::geom::Geometry> buildGeosGeometry(const ParsedGeometry& geometry) {
    using namespace geos::geom;
    const GeometryFactory& factory = *GeometryFactory::getDefaultInstance();

    // Mirrors geos::io::GeoJSONReader (ext/geos/src/io/GeoJSONReader.cpp), except that
    // z values are preserved for all geometry types (geos' own reader drops z for
    // MultiLineString)
    switch (geometry.kind) {
        case GeometryKind::Point: {
            if (geometry.coords.empty()) {
                return factory.createPoint(2);
            }
            const Position& p = geometry.coords[0][0][0];
            return std::unique_ptr<Point>(
                factory.createPoint(Coordinate(p.x, p.y, p.z))
            );
        }
        case GeometryKind::MultiPoint: {
            const std::vector<Position>& positions = geometry.coords[0][0];
            std::vector<std::unique_ptr<Point>> points;
            points.reserve(positions.size());
            for (const Position& p : positions) {
                points.push_back(std::unique_ptr<Point>(
                    factory.createPoint(Coordinate(p.x, p.y, p.z))
                ));
            }
            return factory.createMultiPoint(std::move(points));
        }
        case GeometryKind::LineString:
            return factory.createLineString(
                makeCoordinateSequence(geometry.coords[0][0])
            );
        case GeometryKind::MultiLineString: {
            std::vector<std::unique_ptr<LineString>> lines;
            lines.reserve(geometry.coords[0].size());
            for (const std::vector<Position>& line : geometry.coords[0]) {
                lines.push_back(factory.createLineString(makeCoordinateSequence(line)));
            }
            return factory.createMultiLineString(std::move(lines));
        }
        case GeometryKind::Polygon:
            return makePolygon(geometry.coords[0], factory);
        case GeometryKind::MultiPolygon: {
            std::vector<std::unique_ptr<Polygon>> polygons;
            polygons.reserve(geometry.coords.size());
            for (const std::vector<std::vector<Position>>& polygon : geometry.coords) {
                polygons.push_back(makePolygon(polygon, factory));
            }
            return factory.createMultiPolygon(std::move(polygons));
        }
        case GeometryKind::GeometryCollection: {
            std::vector<std::unique_ptr<Geometry>> children;
            children.reserve(geometry.children.size());
            for (const ParsedGeometry& child : geometry.children) {
                children.push_back(buildGeosGeometry(child));
            }
            return factory.createGeometryCollection(std::move(children));
        }
        case GeometryKind::None:
        default:
            return nullptr;
    }
}

} // namespace openspace::geojson
