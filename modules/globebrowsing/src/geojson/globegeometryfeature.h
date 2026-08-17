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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GLOBEGEOMETRYFEATURE___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GLOBEGEOMETRYFEATURE___H__

#include <modules/globebrowsing/src/geojson/geojsonproperties.h>
#include <openspace/rendering/helper.h>
#include <openspace/rendering/multidrawbatch.h>
#include <openspace/rendering/texturecomponent.h>
#include <ghoul/glm.h>
#include <ghoul/opengl/ghoul_gl.h>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace geos::geom { class Geometry; }
namespace ghoul::opengl {
    class ProgramObject;
    class Texture;
} // namespace ghoul::opengl

namespace openspace {

namespace rendering {
    struct LightSourceRenderData;
    struct VertexXYZNormal;
} // namespace rendering
struct Documentation;
struct Geodetic2;
struct Geodetic3;
struct RenderData;
class RenderableGlobe;

/**
 * Per-draw data for batched points and lines, stored in an SSBO and indexed with
 * `baseDrawId + gl_DrawID` in the shaders. Must be kept in sync with the DrawElement
 * struct in geojson_drawdata.glsl (std430 layout).
 */
struct GeoJsonDrawRecord {
    static constexpr uint32_t FlagUseHeightMap = 1;
    static constexpr uint32_t FlagBottomAnchor = 2;
    static constexpr uint32_t FlagsRenderModeShift = 2;
    static constexpr uint32_t FlagPerformShading = 16;

    /// Draw group key bit marking wireframe groups. Group keys otherwise hold the GL
    /// texture name for points, so this bit is kept well clear of that range
    static constexpr int64_t WireframeGroupBit = static_cast<int64_t>(1) << 40;

    /// Polygon group keys: bits 0-1 hold the cull pass, bit 40 wireframe and the bits
    /// from PolygonComponentShift up the owning component's index. Component-major key
    /// ordering keeps the current per-component draw order: single-pass draws, then the
    /// two culled passes of transparent extruded features, then the next component
    static constexpr int64_t PolygonCullPassMask = 3;
    /// Cull pass value for the glCullFace(GL_FRONT) pass
    static constexpr int64_t PolygonBackFacePass = 1;
    /// Cull pass value for the glCullFace(GL_BACK) pass
    static constexpr int64_t PolygonFrontFacePass = 2;
    static constexpr int PolygonComponentShift = 41;

    /// rgb + final opacity, fades folded in
    glm::vec4 color = glm::vec4(1.f);
    /// In meters
    float heightOffset = 0.f;
    /// Point size (world units) or line width (pixels)
    float sizeOrWidth = 0.f;
    /// Points only
    float textureWidthFactor = 1.f;
    uint32_t flags = 0;
};
static_assert(sizeof(GeoJsonDrawRecord) == 32);

/**
 * This class is responsible for rendering the geomoetry features of globes, created e.g.
 * from GeoJson files.
 */
class GlobeGeometryFeature {
public:
    using Vertex = rendering::VertexXYZNormal;

    GlobeGeometryFeature(const RenderableGlobe& globe,
        GeoJsonProperties& defaultProperties,
        GeoJsonOverrideProperties& overrideProperties);


    // TODO: Use instead of numbers
    //enum class RenderPass {
    //    GeometryOnly,
    //    GeometryAndShading
    //};

    enum class PointRenderMode {
        AlignToCameraDir = 0,
        AlignToCameraPos,
        AlignToGlobeNormal,
        AlignToGlobeSurface
    };

    enum class GeometryType {
        LineString = 0,
        Point,
        Polygon,
        Error
    };

    enum class RenderType {
        Lines = 0,
        Points,
        Polygon,
        Uninitialized
    };

    /**
     * Each geometry feature might translate into several render features. Every render
     * feature lives as one draw in the shared batch matching its type.
     */
    struct RenderFeature {
        RenderType type = RenderType::Uninitialized;
        rendering::MultiDrawBatch::DrawHandle batchHandle =
            rendering::MultiDrawBatch::InvalidHandle;
        size_t nVertices = 0;
        bool isExtrusionFeature = false;

        /// Store the geodetic lat long coordinates of each vertex, so we can quickly
        /// recompute the height values for these points
        std::vector<Geodetic2> vertices;

        /// Keep the heights around
        std::vector<float> heights;
    };

    /**
     * Some extra data that we need for doing the rendering.
     */
    struct ExtraRenderData {
        float pointSizeScale;
        float lineWidthScale;
        PointRenderMode& pointRenderMode;
        rendering::LightSourceRenderData& lightSourceData;
        /// Base group key for polygon draws: component index and wireframe bit.
        int64_t polygonGroupBase;
    };

    std::string key() const;

    void setOffsets(glm::vec3 offsets);

    void initializeGL(rendering::MultiDrawBatch* pointsBatch,
        rendering::MultiDrawBatch* linesBatch, rendering::MultiDrawBatch* polygonsBatch);
    void deinitializeGL();
    bool isReady() const;
    bool isPoints() const;
    bool useHeightMap() const;
    bool hasPointTexture() const;

    void updateTexture(bool isInitializeStep = false);

    void createFromSingleGeosGeometry(const geos::geom::Geometry* geo, int index,
        bool ignoreHeights);

    // Accessors for the derived geometry data, used to serialize the load cache
    GeometryType type() const;
    const std::vector<std::vector<Geodetic3>>& geoCoordinates() const;
    const std::vector<Geodetic3>& triangleCoordinates() const;
    const std::vector<Geodetic3>& heightUpdateReferencePoints() const;

    /**
     * Install pre-derived geometry data from the load cache, skipping all GEOS work.
     * Produces the same state as createFromSingleGeosGeometry (any height zeroing from
     * ignoreHeights is already baked into the cached coordinates).
     */
    void setFromCachedData(GeometryType type,
        std::vector<std::vector<Geodetic3>> geoCoordinates,
        std::vector<Geodetic3> triangleCoordinates,
        std::vector<Geodetic3> heightUpdateReferencePoints,
        std::string key);

    /**
     * Queue this feature's draws into the shared batches for the current frame.
     * Textures used by point draws are recorded in \p pointTextures, keyed by the same
     * group key as the emitted draws, so that the batch owner can bind the right
     * texture per group.
     */
    void emitBatchedDraws(float mainOpacity, const ExtraRenderData& extraRenderData,
        bool wireframe, std::map<int64_t, ghoul::opengl::Texture*>& pointTextures);

    /**
     * Install per-render-feature heights loaded from the heights cache, along with
     * the reference heights they were sampled against. They are consumed, in render
     * feature build order, by the next geometry build; each vector is validated by
     * vertex count and any mismatch discards the rest (alignment is positional).
     * Later rebuilds are unaffected.
     */
    void setPendingCachedHeights(std::vector<std::vector<float>> heights,
        std::vector<double> controlHeights);

    /**
     * Returns copies of each render feature's heights, in build order (for the
     * heights cache).
     */
    std::vector<std::vector<float>> currentHeights() const;

    /**
     * Returns the reference heights the current per-vertex heights were computed
     * with.
     */
    const std::vector<double>& lastControlHeights() const;

    /**
     * Check whether the height map heights at the feature's reference points differ
     * from the values the current per-vertex heights were computed with. The polling
     * cadence is owned by the GeoJsonComponent's refinement sweep.
     *
     * \param force If true, the new heights are returned without comparing
     * \return The new reference heights when they differ (pass them to
     *         applyHeightUpdate), or nullopt when nothing changed or the feature does
     *         not use the height map
     */
    std::optional<std::vector<double>> checkHeightMapChange(bool force = false) const;

    /**
     * Re-sample the height map for all of the feature's vertices, upload the new
     * heights and store \p newControlHeights as the reference state that
     * checkHeightMapChange compares against.
     */
    void applyHeightUpdate(std::vector<double> newControlHeights);

    /**
     * Returns the number of vertices that a height re-sample has to query the globe
     * for. Used by the component to budget its refinement sweep.
     */
    size_t heightVertexCount() const;

    /**
     * Returns true when bounds over the built render features are available, i.e.
     * cullSphereCenter and cullSphereRadius may be used.
     */
    bool hasCullSphere() const;

    /**
     * Center of a bounding sphere over all built render feature vertices (including
     * extrusion walls when built), in globe model space.
     */
    glm::dvec3 cullSphereCenter() const;

    /**
     * Radius of the bounding sphere around cullSphereCenter, in meters. Excludes the
     * dynamic displacements applied at draw time (height offset and height map
     * samples); those are covered by the cull slack.
     */
    double cullSphereRadius() const;

    /**
     * The largest absolute height map sample applied to any of the feature's
     * vertices, in meters. Only ever grows between geometry rebuilds.
     */
    float maxAbsSampledHeight() const;

    /**
     * Conservative bound on how far a point feature's billboard geometry can extend
     * from its anchor vertex, in meters. Zero for non-point features.
     *
     * \param pointSizeScale The owning component's point size scale factor
     */
    double pointCullSlack(float pointSizeScale) const;

    /**
     * Returns true if the geometry was rebuilt, i.e. draws were added to or removed
     * from the shared batches, so that the owner knows to re-commit them.
     */
    bool update(bool dataIsDirty);
    void updateGeometry();

private:
    rendering::MultiDrawBatch* batchForRenderType(RenderType type) const;

    /**
     * Remove all render features, releasing their batch draws.
     */
    void clearRenderFeatures();

    /**
     * Create the vertex information for any line parts of the feature. Returns the
     * resulting vertex positions, so we can use them for extrusion.
     */
    std::vector<std::vector<glm::vec3>> createLineGeometry();

    /**
     * Create the vertex information for any point parts of the feature. Also creates the
     * features for extruded lines for the points.
     */
    void createPointGeometry();

    /**
     * Create the triangle geometry for the extruded edges of lines/polygons.
     */
    void createExtrudedGeometry(const std::vector<std::vector<glm::vec3>>& edgeVertices);

    /**
     * Create the triangle geometry for the polygon part of the feature (the area
     * contained by the shape).
     */
    void createPolygonGeometry();

    void initializeRenderFeature(RenderFeature& feature,
        const std::vector<Vertex>& vertices);

    /**
     * Get the distance that shall be used for tessellation, based on the properties.
     */
    float tessellationStepSize() const;

    /**
     * Compute the heights to the surface at the reference points.
     */
    std::vector<double> getCurrentReferencePointsHeights() const;

    /**
     * Buffer the dynamic height data for the vertices, based on the height map.
     */
    void bufferDynamicHeightData(const RenderFeature& feature);

    GeometryType _type = GeometryType::Error;
    const RenderableGlobe& _globe;

    /// Coordinates for geometry. For polygons, the first is always the outer ring and any
    /// following are the inner rings (holes)
    std::vector<std::vector<Geodetic3>> _geoCoordinates;

    /// Coordinates for any triangles representing the geometry (only relevant for
    /// polygons)
    std::vector<Geodetic3> _triangleCoordinates;

    std::vector<RenderFeature> _renderFeatures;

    /// lat, long, distance (meters). Passed from parent on property change
    glm::vec3 _offsets = glm::vec3(0.f);

    std::string _key;
    const PropertySet _properties;

    std::vector<Geodetic3> _heightUpdateReferencePoints;
    std::vector<double> _lastControlHeights;

    /// Axis-aligned bounds over all built render feature vertices in globe model
    /// space, accumulated during geometry builds to derive the feature's cull sphere
    glm::vec3 _cullAabbMin = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 _cullAabbMax = glm::vec3(std::numeric_limits<float>::lowest());
    float _maxAbsSampledHeight = 0.f;

    // Heights installed from the heights cache, consumed by the next geometry build
    std::vector<std::vector<float>> _pendingCachedHeights;
    std::vector<double> _pendingControlHeights;
    size_t _pendingHeightsCursor = 0;
    bool _pendingHeightsValid = false;

    bool _hasTexture = false;
    std::unique_ptr<TextureComponent> _pointTexture;

    rendering::MultiDrawBatch* _pointsBatch = nullptr;
    rendering::MultiDrawBatch* _linesBatch = nullptr;
    rendering::MultiDrawBatch* _polygonsBatch = nullptr;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GLOBEGEOMETRYFEATURE___H__
