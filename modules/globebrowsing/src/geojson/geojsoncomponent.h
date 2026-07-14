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
#include <functional>
#include <memory>
#include <optional>

namespace geos::geom { class Geometry; }

namespace openspace::geojson { struct GeoJsonCacheFile; }
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
    void initializeGL(ghoul::opengl::ProgramObject* polygonsProgram,
        rendering::MultiDrawBatch* pointsBatch, rendering::MultiDrawBatch* linesBatch);
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

    /// Renders the polygon features. Points and lines are rendered batched by the
    /// owning GeoJsonManager, fed through emitBatchedDraws
    void render(const RenderData& data);

    /// Queue this component's points and lines draws into the shared batches for the
    /// current frame (see GlobeGeometryFeature::emitBatchedDraws)
    void emitBatchedDraws(std::map<int64_t, ghoul::opengl::Texture*>& pointTextures);

    /// Returns true if any feature's geometry was rebuilt, so that the owner knows to
    /// re-commit the shared batches
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

        /// Register \p callback on every property that affects how the feature is drawn
        void onStyleChange(std::function<void()> callback);

        BoolProperty enabled;
        Vec2Property centroidLatLong;
        Vec4Property boundingboxLatLong;
        TriggerProperty flyToFeature;
        float boundingBoxDiagonal = 0.f;
    };

    /// Accumulated load phase durations, for the load-time summary log
    struct LoadStats {
        std::chrono::steady_clock::duration parse{};
        std::chrono::steady_clock::duration validate{};
        std::chrono::steady_clock::duration derive{};
        std::chrono::steady_clock::duration registration{};
    };

    void readFile();
    void parseSingleFeature(const geojson::ParsedFeature& feature, int indexInFile,
        LoadStats& stats, geojson::GeoJsonCacheFile& cacheOut);

    /// Construct all features from a load cache, skipping JSON parsing and GEOS work
    void loadFromCache(const geojson::GeoJsonCacheFile& cache);

    /// Compute the bounding box diagonal from the feature's boundingboxLatLong. Depends
    /// on the globe's ellipsoid, which is why it is not part of the load cache
    void computeFeatureDiagonal(SubFeatureProps& feature) const;

    /**
     * Add meta properties to the feature, to allow things like flying to it, identifying
     * its location, etc.
     */
    void addMetaPropertiesToFeature(SubFeatureProps& feature, int index,
        const geos::geom::Geometry* geometry);

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

    // Cached facts about the loaded features, used to skip the polygon render pass
    // when no feature can possibly draw polygons. Extrude overrides are static after
    // load; only the default extrude property is live
    int _nFillPolygonFeatures = 0;
    int _nExtrudeTrueOverride = 0;
    int _nExtrudableNoOverride = 0;

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
    std::vector<std::unique_ptr<SubFeatureProps>> _features;

    // Owned by the GeoJsonManager and shared between all components on the globe
    ghoul::opengl::ProgramObject* _polygonsProgram = nullptr;
    rendering::MultiDrawBatch* _pointsBatch = nullptr;
    rendering::MultiDrawBatch* _linesBatch = nullptr;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCOMPONENT___H__
