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

#include <modules/globebrowsing/src/geojson/globegeometryfeature.h>

#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/src/geojson/globegeometryhelper.h>
#include <modules/globebrowsing/src/renderableglobe.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/moduleengine.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/geodetic.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/format.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/exception.h>
#include <ghoul/opengl/openglstatecache.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <geos/triangulate/polygon/ConstrainedDelaunayTriangulator.h>
#include <geos/triangulate/tri/Tri.h>
#include <geos/triangulate/tri/TriList.h>
#include <geos/util/GEOSException.h>
#include <geos/util/IllegalStateException.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {
    constexpr std::string_view _loggerCat = "GlobeGeometryFeature";
} // namespace

namespace openspace {

GlobeGeometryFeature::GlobeGeometryFeature(const RenderableGlobe& globe,
                                           GeoJsonProperties& defaultProperties,
                                           GeoJsonOverrideProperties& overrideProperties)
    : _globe(globe)
    , _properties({
        .defaultValues = defaultProperties,
        .overrideValues = overrideProperties
    })
{}

std::string GlobeGeometryFeature::key() const {
    return _key;
}

void GlobeGeometryFeature::setOffsets(glm::vec3 offsets) {
    _offsets = std::move(offsets);
}

void GlobeGeometryFeature::initializeGL(rendering::MultiDrawBatch* pointsBatch,
                                        rendering::MultiDrawBatch* linesBatch,
                                        rendering::MultiDrawBatch* polygonsBatch)
{
    _pointsBatch = pointsBatch;
    _linesBatch = linesBatch;
    _polygonsBatch = polygonsBatch;

    if (isPoints()) {
        updateTexture(true);
    }
}

void GlobeGeometryFeature::deinitializeGL() {
    clearRenderFeatures();
    _pointTexture = nullptr;
}

bool GlobeGeometryFeature::isReady() const {
    const bool resourcesAreReady = _pointsBatch && _linesBatch && _polygonsBatch;
    const bool textureIsReady = !_hasTexture || _pointTexture;
    return resourcesAreReady && textureIsReady;
}

bool GlobeGeometryFeature::isPoints() const {
    return _type == GeometryType::Point;
}

bool GlobeGeometryFeature::useHeightMap() const {
    return _properties.altitudeMode() ==
        GeoJsonProperties::AltitudeMode::RelativeToGround;
}

bool GlobeGeometryFeature::hasPointTexture() const {
    return _pointTexture != nullptr;
}

void GlobeGeometryFeature::updateTexture(bool isInitializeStep) {
    std::filesystem::path texture;
    GlobeBrowsingModule* m = global::moduleEngine->module<GlobeBrowsingModule>();

    if (!isInitializeStep && _properties.hasOverrideTexture()) {
        // Here we don't necessarily have to update, since it should have been created at
        // initialization. Do nothing
        return;
    }
    else if (!_properties.pointTexture().empty()) {
        texture = _properties.pointTexture();
    }
    else if (m->hasDefaultGeoPointTexture()) {
        texture = m->defaultGeoPointTexture();
    }
    else {
        // No texture => render without texture
        _hasTexture = false;
        _pointTexture = nullptr;
        return;
    }

    if (isInitializeStep || !_pointTexture) {
        _pointTexture = std::make_unique<TextureComponent>(2);
        _pointTexture->setFilterMode(
            ghoul::opengl::Texture::FilterMode::AnisotropicMipMap
        );
        _pointTexture->setWrapping(ghoul::opengl::Texture::WrappingMode::ClampToEdge);
    }

    if (std::filesystem::is_regular_file(texture)) {
        _hasTexture = true;
        _pointTexture->loadFromFile(texture);
    }
    else {
        LERROR(std::format(
            "Trying to use texture file that does not exist: {}", texture
        ));
    }
}

void GlobeGeometryFeature::createFromSingleGeosGeometry(const geos::geom::Geometry* geo,
                                                        int index, bool ignoreHeights)
{
    if (!geo) {
        throw std::logic_error("No geometry provided");
    }
    ghoul_assert(
        (geo && geo->isPuntal()) || (geo && !geo->isCollection()),
        "Non-point geometry can not be a collection"
    );

    switch (geo->getGeometryTypeId()) {
        case geos::geom::GEOS_POINT:
        case geos::geom::GEOS_MULTIPOINT: {
            _geoCoordinates.push_back(geometryCoordsAsGeoVector(geo));
            _type = GeometryType::Point;
            break;
        }
        case geos::geom::GEOS_LINESTRING: {
            _geoCoordinates.push_back(geometryCoordsAsGeoVector(geo));
            _type = GeometryType::LineString;
            break;
        }
        case geos::geom::GEOS_POLYGON: {
            try {
                using geos::triangulate::polygon::ConstrainedDelaunayTriangulator;

                const auto p = dynamic_cast<const geos::geom::Polygon*>(geo);

                // Triangles
                // Note that Constrained Delaunay triangulation supports polygons with
                // holes :)
                geos::triangulate::tri::TriList<geos::triangulate::tri::Tri> triangles;
                ConstrainedDelaunayTriangulator::triangulatePolygon(p, triangles);

                std::vector<geos::geom::Coordinate> triCoords;
                triCoords.reserve(3 * triangles.size());

                // Add three coordinates per triangle. Note flipped winding order (want
                // counter clockwise, but GEOS provides clockwise)
                for (const geos::triangulate::tri::Tri* t : triangles) {
                    triCoords.push_back(t->getCoordinate(0));
                    triCoords.push_back(t->getCoordinate(2));
                    triCoords.push_back(t->getCoordinate(1));
                }
                _triangleCoordinates = coordsToGeodetic(triCoords);

                // Boundaries / Lines

                // Normalize to make sure rings have correct orientation
                std::unique_ptr<geos::geom::Polygon> pNormalized = p->clone();
                pNormalized->normalize();

                const geos::geom::LinearRing* outerRing = pNormalized->getExteriorRing();
                const std::vector<Geodetic3> outerBoundsGeoCoords =
                    geometryCoordsAsGeoVector(outerRing);

                if (!outerBoundsGeoCoords.empty()) {
                    const int nHoles = static_cast<int>(
                        pNormalized->getNumInteriorRing()
                    );
                    _geoCoordinates.reserve(nHoles + 1);

                    // Outer bounds
                    _geoCoordinates.push_back(outerBoundsGeoCoords);

                    // Inner bounds (holes)
                    for (int i = 0; i < nHoles; i++) {
                        const geos::geom::LinearRing* hole =
                            pNormalized->getInteriorRingN(i);
                        std::vector<Geodetic3> ringGeoCoords =
                            geometryCoordsAsGeoVector(hole);
                        _geoCoordinates.push_back(std::move(ringGeoCoords));
                    }
                }

                _type = GeometryType::Polygon;
            }
            catch (geos::util::IllegalStateException& e) {
                throw ghoul::RuntimeError(std::format(
                    "GEOS illegal state error: {}", e.what()
                ));
            }
            catch (geos::util::GEOSException& e) {
                throw ghoul::RuntimeError(std::format(
                    "Unknown geos error: {}", e.what()
                ));
            }
            break;
        }
        default:
            throw ghoul::MissingCaseException();
    }

    // Reset height values if we don't care about them
    if (ignoreHeights) {
        for (std::vector<Geodetic3>& vec : _geoCoordinates) {
            for (Geodetic3& coord : vec) {
                coord.height = 0.0;
            }
        }
    }

    // Compute reference positions to use for checking if height map changes
    geos::geom::Coordinate centroid;
    geo->getCentroid(centroid);
    Geodetic3 geoCentroid = coordsToGeodetic({ centroid }).front();
    _heightUpdateReferencePoints.push_back(std::move(geoCentroid));

    std::vector<Geodetic3> envelopeGeoCoords =
        geometryCoordsAsGeoVector(geo->getEnvelope().get());

    _heightUpdateReferencePoints.insert(
        _heightUpdateReferencePoints.end(),
        envelopeGeoCoords.begin(),
        envelopeGeoCoords.end()
    );

    if (_properties.overrideValues.name.has_value()) {
        _key = *_properties.overrideValues.name;
    }
    else {
        _key = std::format("Feature {} - {}", index, geo->getGeometryType());
    }
}

GlobeGeometryFeature::GeometryType GlobeGeometryFeature::type() const {
    return _type;
}

const std::vector<std::vector<Geodetic3>>& GlobeGeometryFeature::geoCoordinates() const {
    return _geoCoordinates;
}

const std::vector<Geodetic3>& GlobeGeometryFeature::triangleCoordinates() const {
    return _triangleCoordinates;
}

const std::vector<Geodetic3>& GlobeGeometryFeature::heightUpdateReferencePoints() const {
    return _heightUpdateReferencePoints;
}

void GlobeGeometryFeature::setFromCachedData(GeometryType type,
                                       std::vector<std::vector<Geodetic3>> geoCoordinates,
                                       std::vector<Geodetic3> triangleCoordinates,
                                       std::vector<Geodetic3> heightUpdateReferencePoints,
                                       std::string key)
{
    _type = type;
    _geoCoordinates = std::move(geoCoordinates);
    _triangleCoordinates = std::move(triangleCoordinates);
    _heightUpdateReferencePoints = std::move(heightUpdateReferencePoints);
    _key = std::move(key);
}

void GlobeGeometryFeature::emitBatchedDraws(float mainOpacity,
                                        const ExtraRenderData& extraRenderData,
                                        bool wireframe,
                                        std::map<int64_t, ghoul::opengl::Texture*>& pointTextures)
{
    const float opacity = mainOpacity * _properties.opacity();
    const float fillOpacity = mainOpacity * _properties.fillOpacity();

    constexpr int64_t WireframeBit = GeoJsonDrawRecord::WireframeGroupBit;

    for (const RenderFeature& r : _renderFeatures) {
        if (r.batchHandle == rendering::MultiDrawBatch::InvalidHandle) {
            continue;
        }
        if (r.isExtrusionFeature && !_properties.extrude()) {
            continue;
        }

        GeoJsonDrawRecord record;
        record.heightOffset = _offsets.z;
        record.flags = useHeightMap() ? GeoJsonDrawRecord::FlagUseHeightMap : 0;

        if (r.type == RenderType::Lines) {
            const glm::vec3 color = r.isExtrusionFeature ?
                _properties.fillColor() : _properties.color();
            record.color =
                glm::vec4(color, r.isExtrusionFeature ? fillOpacity : opacity);
            record.sizeOrWidth =
                _properties.lineWidth() * extraRenderData.lineWidthScale;

            _linesBatch->emitDraw(r.batchHandle, &record, wireframe ? WireframeBit : 0);
        }
        else if (r.type == RenderType::Points) {
            record.color = glm::vec4(_properties.color(), opacity);

            const float bs = static_cast<float>(_globe.boundingSphere());
            record.sizeOrWidth =
                0.001f * extraRenderData.pointSizeScale * _properties.pointSize() * bs;

            ghoul::opengl::Texture* texture =
                _pointTexture ? _pointTexture->texture() : nullptr;
            record.textureWidthFactor = texture ?
                static_cast<float>(texture->dimensions().x) /
                static_cast<float>(texture->dimensions().y) :
                1.f;

            using TextureAnchor = GeoJsonProperties::PointTextureAnchor;
            if (_properties.pointTextureAnchor() != TextureAnchor::Center) {
                record.flags |= GeoJsonDrawRecord::FlagBottomAnchor;
            }
            record.flags |=
                static_cast<uint32_t>(extraRenderData.pointRenderMode) <<
                GeoJsonDrawRecord::FlagsRenderModeShift;

            int64_t key = texture ? static_cast<int64_t>(GLuint(*texture)) : 0;
            if (wireframe) {
                key |= WireframeBit;
            }
            pointTextures[key] = texture;

            _pointsBatch->emitDraw(r.batchHandle, &record, key);
        }
        else if (r.type == RenderType::Polygon) {
            // Fill triangles and extrusion walls both use the fill color
            record.color = glm::vec4(_properties.fillColor(), fillOpacity);
            if (_properties.performShading()) {
                record.flags |= GeoJsonDrawRecord::FlagPerformShading;
            }

            const int64_t base = extraRenderData.polygonGroupBase;
            if (fillOpacity < 1.f && _properties.extrude()) {
                // Transparent extruded features are drawn twice for correct opacity of
                // overlapping surfaces: first the back faces, then the front faces
                _polygonsBatch->emitDraw(
                    r.batchHandle,
                    &record,
                    base | GeoJsonDrawRecord::PolygonBackFacePass
                );
                _polygonsBatch->emitDraw(
                    r.batchHandle,
                    &record,
                    base | GeoJsonDrawRecord::PolygonFrontFacePass
                );
            }
            else {
                _polygonsBatch->emitDraw(r.batchHandle, &record, base);
            }
        }
    }
}

void GlobeGeometryFeature::setPendingCachedHeights(
                                                 std::vector<std::vector<float>> heights,
                                                   std::vector<double> controlHeights)
{
    _pendingCachedHeights = std::move(heights);
    _pendingControlHeights = std::move(controlHeights);
    _pendingHeightsCursor = 0;
    _pendingHeightsValid = !_pendingCachedHeights.empty();
}

std::vector<std::vector<float>> GlobeGeometryFeature::currentHeights() const {
    std::vector<std::vector<float>> res;
    res.reserve(_renderFeatures.size());
    for (const RenderFeature& f : _renderFeatures) {
        res.push_back(f.heights);
    }
    return res;
}

const std::vector<double>& GlobeGeometryFeature::lastControlHeights() const {
    return _lastControlHeights;
}

std::optional<std::vector<double>> GlobeGeometryFeature::checkHeightMapChange(
                                                                        bool force) const
{
    if (_properties.altitudeMode() != GeoJsonProperties::AltitudeMode::RelativeToGround) {
        return std::nullopt;
    }

    std::vector<double> newHeights = getCurrentReferencePointsHeights();
    if (force) {
        return newHeights;
    }

    // Check if the height values at the control positions have changed
    const bool isSame = std::equal(
        _lastControlHeights.cbegin(),
        _lastControlHeights.cend(),
        newHeights.cbegin(),
        newHeights.cend(),
        [](double a, double b) {
            return std::abs(a - b) < std::numeric_limits<double>::epsilon();
        }
    );
    if (isSame) {
        return std::nullopt;
    }
    return newHeights;
}

void GlobeGeometryFeature::applyHeightUpdate(std::vector<double> newControlHeights) {
    for (RenderFeature& f : _renderFeatures) {
        if (f.vertices.empty()) {
            // Height data was skipped at build time (not in RelativeToGround mode)
            continue;
        }
        f.heights = heightMapHeightsFromGeodetic2List(_globe, f.vertices);
        bufferDynamicHeightData(f);
    }

    // Store the reference state the heights were computed with, so the next check
    // compares against the applied heights instead of re-resampling forever
    _lastControlHeights = std::move(newControlHeights);
}

size_t GlobeGeometryFeature::heightVertexCount() const {
    size_t count = 0;
    for (const RenderFeature& f : _renderFeatures) {
        count += f.vertices.size();
    }
    return count;
}

bool GlobeGeometryFeature::update(bool dataIsDirty) {
    bool geometryChanged = false;
    if (dataIsDirty) {
        updateGeometry();
        geometryChanged = true;
    }

    if (_pointTexture) {
        _pointTexture->update();
    }
    return geometryChanged;
}

void GlobeGeometryFeature::updateGeometry() {
    // Update vertex data and compute model coordinates based on globe
    clearRenderFeatures();

    if (_type == GeometryType::Point) {
        createPointGeometry();
    }
    else {
        const std::vector<std::vector<glm::vec3>> edgeVertices = createLineGeometry();
        // The extrusion walls can be a large amount of geometry (two triangles per
        // boundary edge), so they are only built when actually drawn. The extrude
        // property marks the component's data dirty, so a toggle rebuilds the geometry
        if (_properties.extrude()) {
            createExtrudedGeometry(edgeVertices);
        }
        createPolygonGeometry();
    }

    // Reference heights that the current per-vertex heights were computed with. With
    // fully consumed cached heights the cached reference state applies; otherwise
    // start at zero, which is what an unstreamed height map reports, so the first
    // refinement sweep detects the difference once tiles stream in. No globe queries
    // happen here
    if (!useHeightMap()) {
        _lastControlHeights.clear();
    }
    else if (_pendingHeightsValid &&
             _pendingHeightsCursor == _pendingCachedHeights.size())
    {
        _lastControlHeights = std::move(_pendingControlHeights);
    }
    else {
        _lastControlHeights.assign(_heightUpdateReferencePoints.size(), 0.0);
    }

    // Cached heights are only valid for the first build after installation; later
    // rebuilds (changed tessellation, offsets, ...) sample fresh values
    _pendingCachedHeights.clear();
    _pendingControlHeights.clear();
    _pendingHeightsCursor = 0;
    _pendingHeightsValid = false;
}

std::vector<std::vector<glm::vec3>> GlobeGeometryFeature::createLineGeometry() {
    std::vector<std::vector<glm::vec3>> resultPositions;
    resultPositions.reserve(_geoCoordinates.size());
    for (const std::vector<Geodetic3>& coordinates : _geoCoordinates) {
        std::vector<Vertex> vertices;
        std::vector<glm::vec3> positions;
        // @TODO: this is not correct anymore
        vertices.reserve(coordinates.size() * 3);
        // @TODO: this is not correct anymore
        positions.reserve(coordinates.size() * 3);

        glm::dvec3 lastPos = glm::dvec3(0.0);
        double lastHeightValue = 0.0;

        bool isFirst = true;
        for (const Geodetic3& geodetic : coordinates) {
            const glm::dvec3 v = computeOffsetedModelCoordinate(
                geodetic,
                _globe,
                _offsets.x,
                _offsets.y
            );

            const auto addLinePos = [&vertices, &positions](const glm::vec3& pos) {
                vertices.push_back({ .position = pos, .normal = glm::vec3(0.f) });
                positions.push_back(pos);
            };

            if (isFirst) {
                lastPos = v;
                lastHeightValue = geodetic.height;
                isFirst = false;
                addLinePos(glm::vec3(v));
                continue;
            }

            if (_properties.tessellationEnabled()) {
                // Tessellate.
                // But first, determine the step size for the tessellation (larger
                // features will not be tessellated)
                const float stepSize = tessellationStepSize();

                std::vector<PosHeightPair> subdividedPositions =
                    subdivideLine(lastPos, v, lastHeightValue, geodetic.height, stepSize);

                // Don't add the first position. Has been added as last in previous step
                for (size_t si = 1; si < subdividedPositions.size(); si++) {
                    const PosHeightPair& pair = subdividedPositions[si];
                    addLinePos(glm::vec3(pair.position));
                }
            }
            else {
                // Just add the line point
                addLinePos(glm::vec3(v));
            }

            lastPos = v;
            lastHeightValue = geodetic.height;
        }

        vertices.shrink_to_fit();

        RenderFeature feature = {
            .type = RenderType::Lines,
            .nVertices = vertices.size()
        };
        initializeRenderFeature(feature, vertices);
        _renderFeatures.push_back(std::move(feature));

        positions.shrink_to_fit();
        resultPositions.push_back(std::move(positions));
    }

    resultPositions.shrink_to_fit();
    return resultPositions;
}

void GlobeGeometryFeature::createPointGeometry() {
    if (_type != GeometryType::Point) {
        return;
    }

    for (const std::vector<Geodetic3>& coordinates : _geoCoordinates) {
        std::vector<Vertex> vertices;
        vertices.reserve(coordinates.size());

        std::vector<Vertex> extrudedLineVertices;
        extrudedLineVertices.reserve(2 * coordinates.size());

        for (const Geodetic3& geodetic : coordinates) {
            const glm::dvec3 v = computeOffsetedModelCoordinate(
                geodetic,
                _globe,
                _offsets.x,
                _offsets.y
            );

            const glm::vec3 vf = static_cast<glm::vec3>(v);
            // Normal is the out direction
            const glm::vec3 normal = glm::normalize(vf);

            vertices.push_back({ .position = vf, .normal = normal });

            // Lines from center of the globe out to the point
            extrudedLineVertices.push_back({
                .position = glm::vec3(0.f),
                .normal = glm::vec3(0.f)
            });
            extrudedLineVertices.push_back({ .position = vf, .normal = glm::vec3(0.f) });
        }

        vertices.shrink_to_fit();
        extrudedLineVertices.shrink_to_fit();

        RenderFeature feature = {
            .type = RenderType::Points,
            .nVertices = vertices.size()
        };
        initializeRenderFeature(feature, vertices);
        _renderFeatures.push_back(std::move(feature));

        // Create extrusion feature
        RenderFeature extrudeFeature = {
            .type = RenderType::Lines,
            .nVertices = extrudedLineVertices.size(),
            .isExtrusionFeature = true
        };
        initializeRenderFeature(extrudeFeature, extrudedLineVertices);
        _renderFeatures.push_back(std::move(extrudeFeature));
    }
}

void GlobeGeometryFeature::createExtrudedGeometry(
                                  const std::vector<std::vector<glm::vec3>>& edgeVertices)
{
    if (edgeVertices.empty()) {
        return;
    }

    const std::vector<Vertex> vertices = createExtrudedGeometryVertices(edgeVertices);

    RenderFeature feature = {
        .type = RenderType::Polygon,
        .nVertices = vertices.size(),
        .isExtrusionFeature = true
    };
    initializeRenderFeature(feature, vertices);
    _renderFeatures.push_back(std::move(feature));
}

void GlobeGeometryFeature::createPolygonGeometry() {
    if (_triangleCoordinates.empty()) {
        return;
    }

    std::vector<Vertex> polyVertices;

    // Create polygon vertices from the triangle coordinates
    int triIndex = 0;
    std::array<glm::vec3, 3> triPositions;
    std::array<double, 3> triHeights;
    for (const Geodetic3& geodetic : _triangleCoordinates) {
        const glm::vec3 vert = computeOffsetedModelCoordinate(
            geodetic,
            _globe,
            _offsets.x,
            _offsets.y
        );
        triPositions[triIndex] = vert;
        triHeights[triIndex] = geodetic.height;
        triIndex++;

        // Once we have a triangle, start subdividing
        if (triIndex == 3) {
            triIndex = 0;

            const glm::vec3 v0 = triPositions[0];
            const glm::vec3 v1 = triPositions[1];
            const glm::vec3 v2 = triPositions[2];

            const double h0 = triHeights[0];
            const double h1 = triHeights[1];
            const double h2 = triHeights[2];

            if (_properties.tessellationEnabled()) {
                // First determine the step size for the tessellation (larger features
                // will not be tesselated)
                const float stepSize = tessellationStepSize();

                std::vector<Vertex> verts = subdivideTriangle(
                    v0, v1, v2,
                    h0, h1, h2,
                    stepSize,
                    _globe
                );
                polyVertices.insert(polyVertices.end(), verts.begin(), verts.end());
            }
            else {
                // Just add a triangle consisting of the three vertices
                const glm::vec3 n = -glm::normalize(glm::cross(v1 - v0, v2 - v0));
                polyVertices.push_back({ .position = v0, .normal = n });
                polyVertices.push_back({ .position = v1, .normal = n });
                polyVertices.push_back({ .position = v2, .normal = n });
            }
        }
    }

    RenderFeature triFeature = {
        .type = RenderType::Polygon,
        .nVertices = polyVertices.size()
    };
    initializeRenderFeature(triFeature, polyVertices);
    _renderFeatures.push_back(std::move(triFeature));
}

void GlobeGeometryFeature::initializeRenderFeature(RenderFeature& feature,
                                                   const std::vector<Vertex>& vertices)
{
    if (useHeightMap()) {
        feature.vertices = geodetic2FromVertexList(_globe, vertices);

        if (_pendingHeightsValid &&
            _pendingHeightsCursor < _pendingCachedHeights.size() &&
            _pendingCachedHeights[_pendingHeightsCursor].size() == vertices.size())
        {
            // Use the heights loaded from the heights cache
            feature.heights = std::move(_pendingCachedHeights[_pendingHeightsCursor]);
            _pendingHeightsCursor++;
        }
        else {
            // No cached heights, or the cached build configuration drifted. Start at
            // zero — what an unstreamed height map reports — and let the component's
            // refinement sweep fill in real values once tiles stream in. This avoids
            // one globe height query per vertex at load time. Alignment with the
            // cached vectors is positional, so a mismatch discards the rest
            _pendingHeightsValid = false;
            feature.heights.assign(vertices.size(), 0.f);
        }
    }
    else {
        // The heights are only consumed by the shaders in RelativeToGround mode, so
        // skip the retained geodetic copies. A change of the altitude mode marks the
        // component's data dirty, which rebuilds the geometry
        feature.heights.assign(vertices.size(), 0.f);
    }

    if (feature.type == RenderType::Polygon) {
        // Polygons are flat shaded with normals derived in the fragment shader, so
        // their batch's vertex stream holds positions only
        ghoul_assert(_polygonsBatch, "Batch must be initialized");
        std::vector<glm::vec3> positions;
        positions.reserve(vertices.size());
        for (const Vertex& v : vertices) {
            positions.push_back(v.position);
        }

        const std::array<std::span<const std::byte>, 2> streamData = {
            std::as_bytes(std::span(positions)),
            std::as_bytes(std::span(feature.heights))
        };
        feature.batchHandle = _polygonsBatch->addDraw(
            static_cast<GLsizei>(vertices.size()),
            streamData
        );
        return;
    }

    rendering::MultiDrawBatch* batch =
        (feature.type == RenderType::Points) ? _pointsBatch : _linesBatch;
    ghoul_assert(batch, "Batch must be initialized");

    const std::array<std::span<const std::byte>, 2> streamData = {
        std::as_bytes(std::span(vertices)),
        std::as_bytes(std::span(feature.heights))
    };
    feature.batchHandle = batch->addDraw(
        static_cast<GLsizei>(vertices.size()),
        streamData
    );
}

rendering::MultiDrawBatch* GlobeGeometryFeature::batchForRenderType(
                                                                   RenderType type) const
{
    switch (type) {
        case RenderType::Points:
            return _pointsBatch;
        case RenderType::Lines:
            return _linesBatch;
        default:
            return _polygonsBatch;
    }
}

void GlobeGeometryFeature::clearRenderFeatures() {
    for (const RenderFeature& r : _renderFeatures) {
        if (r.batchHandle != rendering::MultiDrawBatch::InvalidHandle) {
            batchForRenderType(r.type)->removeDraw(r.batchHandle);
        }
    }
    _renderFeatures.clear();
}

float GlobeGeometryFeature::tessellationStepSize() const {
    float distance = _properties.tessellationDistance();
    const bool shouldDivideDistance = _properties.useTessellationLevel() &&
        _properties.tessellationLevel() > 0;

    if (shouldDivideDistance) {
        distance /= static_cast<float>(_properties.tessellationLevel());
    }

    return distance;
}

std::vector<double> GlobeGeometryFeature::getCurrentReferencePointsHeights() const {
    std::vector<double> newHeights;
    newHeights.reserve(_heightUpdateReferencePoints.size());
    for (const Geodetic3& geo : _heightUpdateReferencePoints) {
        const glm::dvec3 p = computeOffsetedModelCoordinate(
            geo,
            _globe,
            _offsets.x,
            _offsets.y
        );
        const SurfacePositionHandle handle = _globe.calculateSurfacePositionHandle(p);
        newHeights.push_back(handle.heightToSurface);
    }
    return newHeights;
}

void GlobeGeometryFeature::bufferDynamicHeightData(const RenderFeature& feature) {
    if (feature.batchHandle != rendering::MultiDrawBatch::InvalidHandle) {
        batchForRenderType(feature.type)->updateStreamRange(
            feature.batchHandle,
            1,
            std::as_bytes(std::span(feature.heights))
        );
    }
}

} // namespace openspace
