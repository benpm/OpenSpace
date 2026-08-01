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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCOMPONENT___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCOMPONENT___H__

#include <openspace/properties/propertyowner.h>
#include <openspace/rendering/fadeable.h>

#include <modules/globebrowsing/src/geojson/geojsoncache.h>
#include <modules/globebrowsing/src/geojson/geojsonproperties.h>
#include <modules/globebrowsing/src/geojson/globegeometryfeature.h>
#include <openspace/properties/misc/optionproperty.h>
#include <openspace/properties/misc/stringproperty.h>
#include <openspace/properties/misc/triggerproperty.h>
#include <openspace/properties/scalar/boolproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <openspace/properties/vector/vec2property.h>
#include <openspace/properties/vector/vec4property.h>
#include <openspace/rendering/helper.h>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>

namespace geos::geom { class Geometry; }

namespace ghoul {
    namespace opengl {
        class ProgramObject;
        class Texture;
    } // namespace opengl
    class Dictionary;
} // namespace ghoul

namespace openspace {

struct Documentation;
class LightSource;
struct RenderData;
class RenderableGlobe;

/**
 * A component representing a collection of globe geometry features, whose details are
 * read from a GeoJson file.
 */
class GeoJsonComponent : public PropertyOwner, public Fadeable {
public:
    GeoJsonComponent(const ghoul::Dictionary& dictionary, RenderableGlobe& globe);
    ~GeoJsonComponent() override;

    void initialize();
    void initializeGL(rendering::MultiDrawBatch* pointsBatch,
        rendering::MultiDrawBatch* linesBatch, rendering::MultiDrawBatch* polygonsBatch);
    void deinitializeGL();

    bool isReady() const;
    bool enabled() const;

    /**
     * Returns true when something that affects the batched draws changed since the
     * last call to clearStyleDirty (property edits, fades, feature toggles, ...). The
     * owning GeoJsonManager uses this to skip re-emitting the batch draw lists on
     * frames where nothing changed.
     */
    bool styleIsDirty() const;
    void clearStyleDirty();

    /**
     * Returns true if any feature of this component can currently draw polygon
     * geometry (fill triangles or extrusion walls).
     */
    bool hasPolygonsToDraw() const;

    /**
     * Update the view-dependent light source data used by the polygon shading. Called
     * by the owning GeoJsonManager once per frame when polygons are drawn.
     */
    void updateLightSources(const RenderData& data);
    const rendering::LightSourceRenderData& lightSourceRenderData() const;

    /**
     * Queue this component's draws into the shared batches for the current frame (see
     * GlobeGeometryFeature::emitBatchedDraws). \p componentIndex is this component's
     * index in the manager's list and becomes part of the polygon draw group keys
     */
    void emitBatchedDraws(std::map<int64_t, ghoul::opengl::Texture*>& pointTextures,
        int componentIndex);

    /**
     * Returns true if any feature's geometry was rebuilt, so that the owner knows to
     * re-commit the shared batches.
     */
    bool update();

    static openspace::Documentation Documentation();

private:
    /**
     * Small helper class whose purpose is to encapsulate properties related to a specific
     * geomoetry feature, and allow things like flying to or fadin out individual
     * subfeatures.
     */
    class SubFeatureProps : public PropertyOwner, public Fadeable {
    public:
        SubFeatureProps(PropertyOwner::PropertyOwnerInfo info);

        /**
         * Register \p callback on every property that affects how the feature is
         * drawn.
         */
        void onStyleChange(std::function<void()> callback);

        BoolProperty enabled;
        Vec2Property centroidLatLong;
        Vec4Property boundingboxLatLong;
        TriggerProperty flyToFeature;
        float boundingBoxDiagonal = 0.f;
    };

    /**
     * Accumulated load phase durations, for the load-time summary log.
     */
    struct LoadStats {
        std::chrono::steady_clock::duration parse{};
        std::chrono::steady_clock::duration validate{};
        std::chrono::steady_clock::duration derive{};
        std::chrono::steady_clock::duration registration{};
    };

    /**
     * Per-feature meta data, always kept 1:1 with _geometryFeatures. The
     * SubFeatureProps property owners are only created when the number of features is
     * at most the per-feature-properties threshold; everything else reads from here.
     */
    struct FeatureMeta {
        glm::vec2 centroidLatLong = glm::vec2(0.f);
        glm::vec4 boundingboxLatLong = glm::vec4(0.f);
        float boundingBoxDiagonal = 0.f;
    };

    void readFile();
    void parseSingleFeature(const geojson::ParsedFeature& feature, int indexInFile,
        LoadStats& stats, geojson::GeoJsonCacheFile& cacheOut);

    /**
     * Decide whether per-feature property owners are created, based on the feature
     * count and the configured threshold.
     */
    void decidePerFeatureProps(size_t nFeatures);

    /**
     * Recount how many features own a point texture (needing per-frame updates).
     */
    void countPointTextureFeatures();

    /**
     * Hand the most recently created geometry feature its cached heights (if the
     * heights sidecar cache was loaded and has an entry for \p index).
     */
    void installCachedHeights(int index);

    /**
     * Construct all features from a load cache, skipping JSON parsing and GEOS work.
     */
    void loadFromCache(const geojson::GeoJsonCacheFile& cache);

    /**
     * Compute the bounding box diagonal from the meta's boundingboxLatLong. Depends
     * on the globe's ellipsoid, which is why it is not part of the load cache.
     */
    void computeFeatureDiagonal(FeatureMeta& meta) const;

    /**
     * Compute the centroid, bounding box and diagonal of the geometry.
     */
    FeatureMeta computeFeatureMeta(const geos::geom::Geometry* geometry) const;

    /**
     * Copy the meta values into the feature's properties and hook up its fly-to.
     */
    void applyMetaToFeature(SubFeatureProps& feature, const FeatureMeta& meta,
        int index);

    void computeMainFeatureMetaPropeties();

    /**
     * Trigger a flight to a feature in the collection. No index means to fly to an
     * overview of all features in the collection.
     */
    void flyToFeature(std::optional<int> index = std::nullopt) const;

    void triggerDeletion() const;

    std::vector<GlobeGeometryFeature> _geometryFeatures;

    BoolProperty _enabled;
    StringProperty _geoJsonFile;
    FloatProperty _heightOffset;
    Vec2Property _latLongOffset;

    FloatProperty _pointSizeScale;
    FloatProperty _lineWidthScale;

    GeoJsonProperties _defaultProperties;

    OptionProperty _pointRenderModeOption;

    BoolProperty _drawWireframe;
    BoolProperty _preventUpdatesFromHeightMap;
    TriggerProperty _forceUpdateHeightData;

    RenderableGlobe& _globeNode;

    bool _ignoreHeightsFromFile = false;

    bool _dataIsDirty = true;
    bool _heightOffsetIsDirty = false;
    bool _textureIsDirty = false;
    bool _styleIsDirty = true;
    mutable bool _isReadyCached = false;

    /// Height refinement sweep: a budgeted round-robin over the features that checks
    /// whether streamed height map data changed and re-samples where it did. Replaces
    /// per-feature timers, which all fired in the same frame
    size_t _heightSweepCursor = 0;
    bool _heightSweepActive = false;
    std::chrono::steady_clock::time_point _lastSweepStart;
    bool _forceResampleAll = false;

    /// Sidecar cache holding the refined heights across runs; written on
    /// deinitialization when any heights changed
    std::filesystem::path _heightsCacheFile;
    bool _heightsDirtyForCache = false;
    std::optional<geojson::GeoJsonHeightsCacheFile> _loadedHeightsCache;

    /// Cached facts about the loaded features, used to skip per-feature work when no
    /// feature can possibly need it. Property overrides are static after load; only the
    /// default properties are live
    int _nFillPolygonFeatures = 0;
    int _nExtrudeTrueOverride = 0;
    int _nExtrudableNoOverride = 0;
    int _nRelativeToGroundOverride = 0;
    int _nAltitudeModeNoOverride = 0;
    int _nPointTextureFeatures = 0;

    Vec2Property _centerLatLong;
    float _bboxDiagonalSize = 0.f;
    TriggerProperty _flyToFeature;

    PropertyOwner _deletePropertyOwner;
    TriggerProperty _deleteThisComponent;

    std::vector<std::unique_ptr<LightSource>> _lightSources;
    std::unique_ptr<LightSource> _defaultLightSource;

    rendering::LightSourceRenderData _lightsourceRenderData;

    PropertyOwner _lightSourcePropertyOwner;
    PropertyOwner _featuresPropertyOwner;

    /// Only filled when _createPerFeatureProps; otherwise features are always enabled
    /// with full opacity and meta data is read from _featureMeta
    std::vector<std::unique_ptr<SubFeatureProps>> _features;
    std::vector<FeatureMeta> _featureMeta;
    std::unordered_set<std::string> _usedFeatureIdentifiers;
    bool _createPerFeatureProps = true;
    int _perFeaturePropertiesThreshold = 10000;

    /// Owned by the GeoJsonManager and shared between all components on the globe
    rendering::MultiDrawBatch* _pointsBatch = nullptr;
    rendering::MultiDrawBatch* _linesBatch = nullptr;
    rendering::MultiDrawBatch* _polygonsBatch = nullptr;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCOMPONENT___H__
