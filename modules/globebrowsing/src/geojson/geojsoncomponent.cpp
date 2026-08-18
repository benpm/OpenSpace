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

#include <modules/globebrowsing/src/geojson/geojsoncomponent.h>

#include <modules/globebrowsing/src/geojson/geojsonparser.h>
#include <modules/globebrowsing/src/layergroup.h>
#include <modules/globebrowsing/src/layermanager.h>
#include <modules/globebrowsing/src/renderableglobe.h>
#include <openspace/documentation/documentation.h>
#include <openspace/engine/globals.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/scene/lightsource.h>
#include <openspace/scene/scene.h>
#include <openspace/scripting/scriptengine.h>
#include <openspace/util/ellipsoid.h>
#include <openspace/util/geodetic.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/filesystem/cachemanager.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/format.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/defer.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/misc/profiling.h>
#include <ghoul/opengl/openglstatecache.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/Point.h>
#include <geos/operation/valid/MakeValid.h>
#include <geos/util/GEOSException.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <utility>

namespace {
    using namespace openspace;

    constexpr std::string_view _loggerCat = "GeoJsonComponent";

    // Height refinement sweep pacing: minimum time between the starts of two full
    // sweeps, per-frame time budget for the reference-point change checks, and the
    // per-frame budget of vertices to re-sample when changes are found
    constexpr std::chrono::seconds MinSweepInterval(10);
    constexpr std::chrono::microseconds SweepCheckTimeBudget(1500);
    constexpr size_t SweepResampleVertexBudget = 30000;

    constexpr std::string_view KeyIdentifier = "Identifier";
    constexpr std::string_view KeyName = "Name";
    constexpr std::string_view KeyDesc = "Description";

    constexpr Property::PropertyInfo EnabledInfo = {
        "Enabled",
        "Enabled",
        "This setting determines whether this object will be visible or not.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo FileInfo = {
        "File",
        "File",
        "Path to the GeoJSON file to base the rendering on.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo HeightOffsetInfo = {
        "HeightOffset",
        "Height offset",
        "A height offset value, in meters. Useful for moving a feature closer to or "
        "farther away from the surface.",
        Property::Visibility::NoviceUser
    };

    constexpr Property::PropertyInfo CoordinateOffsetInfo = {
        "CoordinateOffset",
        "Geographic coordinate offset",
        "A latitude and longitude offset value, in decimal degrees. Can be used to move "
        "the object on the surface and correct potential mismatches with other "
        "renderings. Note that changing it during runtime leads to all positions being "
        "recomputed.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo DrawWireframeInfo = {
        "DrawWireframe",
        "Wireframe",
        "If true, draw the wire frame of the polygons. Used for testing and to "
        "investigate tessellation results.",
        Property::Visibility::Developer
    };

    constexpr Property::PropertyInfo PreventHeightUpdateInfo = {
        "PreventHeightUpdate",
        "Prevent update from heightmap",
        "If true, the polygon mesh will not be automatically updated based on the "
        "heightmap, even if the 'RelativeToGround' altitude option is set and the "
        "heightmap updates. The data can still be force updated.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo ForceUpdateHeightDataInfo = {
        "ForceUpdateHeightData",
        "Force update height data",
        "Triggering this leads to a recomputation of the heights based on the globe "
        "height map value at the geometry's positions. The recomputation is spread "
        "over the following frames.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo PointRenderModeInfo = {
        "PointRenderMode",
        "Points aligned to",
        "Decides how the billboards for the points should be rendered in terms of up "
        "direction and whether the plane should face the camera. See details on the "
        "different options in the wiki.",
        Property::Visibility::User
    };

    constexpr Property::PropertyInfo FlyToFeatureInfo = {
        "FlyToFeature",
        "Fly to feature",
        "Triggering this leads to the camera flying to a position that show the GeoJson "
        "feature. The flight will account for any lat, long or height offset.",
        Property::Visibility::NoviceUser
    };

    constexpr Property::PropertyInfo CentroidCoordinateInfo = {
        "CentroidCoordinate",
        "Centroid coordinate",
        "The lat long coordinate of the centroid position of the read geometry. Note "
        "that this value does not incude the offset.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo BoundingBoxInfo = {
        "BoundingBox",
        "Bounding box",
        "The lat long coordinates of the lower and upper corner of the bounding box of "
        "the read geometry. Note that this value does not incude the offset.",
        Property::Visibility::AdvancedUser
    };

    constexpr Property::PropertyInfo PointSizeScaleInfo = {
        "PointSizeScale",
        "Point size scale",
        "An extra scale value that can be used to increase or decrease the scale of any "
        "rendered points in the component, even if a value is set from the GeoJson file.",
        Property::Visibility::NoviceUser
    };

    constexpr Property::PropertyInfo LineWidthScaleInfo = {
        "LineWidthScale",
        "Line width scale",
        "An extra scale value that can be used to increase or decrease the width of any "
        "rendered lines in the component, even if a value is set from the GeoJson file. "
        "Note that there is a max limit for how wide lines can be.",
        Property::Visibility::NoviceUser
    };

    constexpr Property::PropertyInfo DeleteInfo = {
        "Delete",
        "Delete",
        "Triggering this will remove this GeoJson component from its globe. Note that "
        "the GUI may have to be reloaded for the change to be reflect in the user "
        "interface.",
        Property::Visibility::User
    };

    struct [[codegen::Dictionary(GeoJsonComponent)]] Parameters {
        // The unique identifier for this layer. May not contain '.' or spaces.
        std::string identifier;

        // A human-readable name for the user interface. If this is omitted, the
        // identifier is used instead.
        std::optional<std::string> name;

        // [[codegen::verbatim(EnabledInfo.description)]]
        std::optional<bool> enabled;

        // The opacity of the component
        std::optional<float> opacity [[codegen::inrange(0.0, 1.0)]];

        // A human-readable description of the layer to be used in informational texts
        // presented to the user.
        std::optional<std::string> description;

        // If true, ignore any height values that are given in the file. Coordinates with
        // three values will then be treated as coordinates with only two values.
        std::optional<bool> ignoreHeights;

        // [[codegen::verbatim(PreventHeightUpdateInfo.description)]]
        std::optional<bool> preventHeightUpdate;

        // [[codegen::verbatim(FileInfo.description)]]
        std::filesystem::path file;

        // [[codegen::verbatim(HeightOffsetInfo.description)]]
        std::optional<float> heightOffset;

        // [[codegen::verbatim(PointSizeScaleInfo.description)]]
        std::optional<float> pointSizeScale;

        // [[codegen::verbatim(LineWidthScaleInfo.description)]]
        std::optional<float> lineWidthScale;

        // [[codegen::verbatim(CoordinateOffsetInfo.description)]]
        std::optional<glm::vec2> coordinateOffset;

        enum class
        [[codegen::map(openspace::GlobeGeometryFeature::PointRenderMode)]]
        PointRenderMode
        {
            AlignToCameraDir [[codegen::key("Camera Direction")]],
            AlignToCameraPos [[codegen::key("Camera Position")]],
            AlignToGlobeNormal [[codegen::key("Globe Normal")]],
            AlignToGlobeSurface [[codegen::key("Globe Surface")]]
        };
        // [[codegen::verbatim(PointRenderModeInfo.description)]]
        std::optional<PointRenderMode> pointRenderMode;

        // [[codegen::verbatim(DrawWireframeInfo.description)]]
        std::optional<bool> drawWireframe;

        // The maximum number of features for which individual per-feature settings
        // (enabled, fade, fly-to) are exposed as properties. Files with more features
        // than this only provide the component-level controls, since hundreds of
        // thousands of per-feature properties use a lot of memory and make the user
        // interface unusably slow. The count refers to the number of features in the
        // file; multi-geometries may render as more features than that. Defaults to
        // 10000.
        std::optional<int> perFeaturePropertiesThreshold [[codegen::greaterequal(0)]];

        // These properties will be used as default values for the geoJson rendering,
        // meaning that they will be used when there is no value given for the
        // individual geoJson features.
        std::optional<ghoul::Dictionary> defaultProperties
            [[codegen::reference("globebrowsing_geojsonproperties")]];

        // A list of light sources that this object should accept light from.
        std::optional<std::vector<ghoul::Dictionary>> lightSources
            [[codegen::reference("core_lightsource")]];
    };
} // namespace
#include "geojsoncomponent_codegen.cpp"

namespace openspace {

Documentation GeoJsonComponent::Documentation() {
    return codegen::doc<Parameters>("globebrowsing_geojsoncomponent");
}

GeoJsonComponent::SubFeatureProps::SubFeatureProps(PropertyOwner::PropertyOwnerInfo info)
    : PropertyOwner(std::move(info))
    , enabled(EnabledInfo, true)
    , centroidLatLong(
        CentroidCoordinateInfo,
        glm::vec2(0.f),
        glm::vec2(-90.f, -180.f),
        glm::vec2(90.f, 180.f)
    )
    , boundingboxLatLong(
        BoundingBoxInfo,
        glm::vec4(0.f),
        glm::vec4(-90.f, -180.f, -90.f, -180.f),
        glm::vec4(90.f, 180.f, 90.f, 180.f)
    )
    , flyToFeature(FlyToFeatureInfo)
{
    _opacity.setVisibility(Property::Visibility::AdvancedUser);
    addProperty(Fadeable::_opacity);
    addProperty(Fadeable::_fade);

    addProperty(enabled);

    addProperty(flyToFeature);

    centroidLatLong.setReadOnly(true);
    addProperty(centroidLatLong);

    boundingboxLatLong.setReadOnly(true);
    addProperty(boundingboxLatLong);
}

void GeoJsonComponent::SubFeatureProps::onStyleChange(std::function<void()> callback) {
    enabled.onChange(callback);
    _opacity.onChange(callback);
    _fade.onChange(std::move(callback));
}

GeoJsonComponent::GeoJsonComponent(const ghoul::Dictionary& dictionary,
                                   RenderableGlobe& globe)
    : PropertyOwner({
        dictionary.value<std::string>(KeyIdentifier),
        dictionary.hasKey(KeyName) ? dictionary.value<std::string>(KeyName) : "",
        dictionary.hasKey(KeyDesc) ? dictionary.value<std::string>(KeyDesc) : ""
    })
    , _enabled(EnabledInfo, true)
    , _geoJsonFile(FileInfo)
    , _heightOffset(HeightOffsetInfo, 10.f, -1e12f, 1e12f)
    , _latLongOffset(
        CoordinateOffsetInfo,
        glm::vec2(0.f),
        glm::vec2(-90.0),
        glm::vec2(90.f)
    )
    , _pointSizeScale(PointSizeScaleInfo, 1.f, 0.01f, 100.f)
    , _lineWidthScale(LineWidthScaleInfo, 1.f, 0.01f, 10.f)
    , _pointRenderModeOption(PointRenderModeInfo)
    , _drawWireframe(DrawWireframeInfo, false)
    , _preventUpdatesFromHeightMap(PreventHeightUpdateInfo, false)
    , _forceUpdateHeightData(ForceUpdateHeightDataInfo)
    , _globeNode(globe)
    , _centerLatLong(
        CentroidCoordinateInfo,
        glm::vec2(0.f),
        glm::vec2(-90.f, -180.f),
        glm::vec2(90.f, 180.f)
    )
    , _flyToFeature(FlyToFeatureInfo)
    , _deletePropertyOwner({ "Deletion", "Deletion" })
    , _deleteThisComponent(DeleteInfo)
    , _lightSourcePropertyOwner({ "LightSources", "Light Sources" })
    , _featuresPropertyOwner({ "Features", "Features" })
{
    const Parameters p = codegen::bake<Parameters>(dictionary);

    _enabled = p.enabled.value_or(_enabled);
    addProperty(_enabled);

    _opacity = p.opacity.value_or(_opacity);
    addProperty(_opacity);
    addProperty(_fade);

    _geoJsonFile = p.file.string();
    _geoJsonFile.setReadOnly(true);
    addProperty(_geoJsonFile);

    _ignoreHeightsFromFile = p.ignoreHeights.value_or(_ignoreHeightsFromFile);

    const float minGlobeRadius = static_cast<float>(
        _globeNode.ellipsoid().minimumRadius()
    );

    _heightOffset = p.heightOffset.value_or(_heightOffset);
    _heightOffset.onChange([this]() { _heightOffsetIsDirty = true; });
    constexpr float MinRadiusFactor = -0.9f;
    constexpr float MaxRadiusFactor = 5.f;
    _heightOffset.setMinValue(MinRadiusFactor * minGlobeRadius);
    _heightOffset.setMaxValue(MaxRadiusFactor * minGlobeRadius);
    addProperty(_heightOffset);

    _latLongOffset = p.coordinateOffset.value_or(_latLongOffset);
    _latLongOffset.onChange([this]() { _dataIsDirty = true; });
    addProperty(_latLongOffset);

    _pointSizeScale = p.pointSizeScale.value_or(_pointSizeScale);
    addProperty(_pointSizeScale);

    _lineWidthScale = p.lineWidthScale.value_or(_lineWidthScale);
    addProperty(_lineWidthScale);

    if (p.defaultProperties.has_value()) {
        _defaultProperties.createFromDictionary(*p.defaultProperties, _globeNode);
    }
    addPropertySubOwner(_defaultProperties);

    _defaultProperties.pointTexture.onChange([this]() {
        const std::filesystem::path texturePath = _defaultProperties.pointTexture.value();
        // Note that an empty texture is also valid => use default texture from module
        if (std::filesystem::is_regular_file(texturePath) || texturePath.empty()) {
            _textureIsDirty = true;
        }
        else {
            LERROR(std::format(
                "Provided texture file does not exist: {}",
                _defaultProperties.pointTexture.value()
            ));
        }
    });

    _defaultProperties.tessellation.enabled.onChange([this]() { _dataIsDirty = true; });
    _defaultProperties.tessellation.useLevel.onChange([this]() { _dataIsDirty = true; });
    _defaultProperties.tessellation.level.onChange([this]() { _dataIsDirty = true; });
    _defaultProperties.tessellation.distance.onChange([this]() { _dataIsDirty = true; });

    // Extrusion wall geometry is only built when actually drawn, so a toggle of the
    // extrude property requires a geometry rebuild. The same applies to the per-vertex
    // height map data, which is only computed in RelativeToGround altitude mode
    _defaultProperties.extrude.onChange([this]() { _dataIsDirty = true; });
    _defaultProperties.altitudeModeOption.onChange([this]() { _dataIsDirty = true; });

    _forceUpdateHeightData.onChange([this]() {
        // Restart the refinement sweep in forced mode; the re-sampling is spread over
        // the next frames rather than done all at once
        _forceResampleAll = true;
        _heightSweepActive = true;
        _heightSweepCursor = 0;
        _lastSweepStart = std::chrono::steady_clock::now();
    });
    addProperty(_forceUpdateHeightData);

    _preventUpdatesFromHeightMap =
        p.preventHeightUpdate.value_or(_preventUpdatesFromHeightMap);
    addProperty(_preventUpdatesFromHeightMap);

    _drawWireframe = p.drawWireframe.value_or(_drawWireframe);
    addProperty(_drawWireframe);

    _perFeaturePropertiesThreshold =
        p.perFeaturePropertiesThreshold.value_or(_perFeaturePropertiesThreshold);

    using PointRenderMode = GlobeGeometryFeature::PointRenderMode;
    _pointRenderModeOption.addOptions({
        { static_cast<int>(PointRenderMode::AlignToCameraDir), "Camera Direction" },
        { static_cast<int>(PointRenderMode::AlignToCameraPos), "Camera Position" },
        { static_cast<int>(PointRenderMode::AlignToGlobeNormal), "Globe Normal" },
        { static_cast<int>(PointRenderMode::AlignToGlobeSurface), "Globe Surface" }
    });
    if (p.pointRenderMode.has_value()) {
        _pointRenderModeOption =
            static_cast<int>(codegen::map<PointRenderMode>(*p.pointRenderMode));
    }
    addProperty(_pointRenderModeOption);

    _centerLatLong.setReadOnly(true);
    addProperty(_centerLatLong);

    _flyToFeature.onChange([this]() { flyToFeature(); });
    addProperty(_flyToFeature);

    // Add delete trigger under its own property owner, to make it more difficult to
    // access by mistake
    _deleteThisComponent.onChange([this]() { triggerDeletion(); });
    _deletePropertyOwner.addProperty(_deleteThisComponent);
    addPropertySubOwner(_deletePropertyOwner);

    // Anything that affects how the batched points and lines are drawn marks the style
    // dirty, so that the GeoJsonManager knows to rebuild the shared draw lists. On
    // frames where nothing changed, the previous lists and per-draw data are reused
    auto markStyleDirty = [this]() { _styleIsDirty = true; };
    _enabled.onChange(markStyleDirty);
    _opacity.onChange(markStyleDirty);
    _fade.onChange(markStyleDirty);
    _heightOffset.onChange(markStyleDirty);
    _pointSizeScale.onChange(markStyleDirty);
    _lineWidthScale.onChange(markStyleDirty);
    _pointRenderModeOption.onChange(markStyleDirty);
    _drawWireframe.onChange(markStyleDirty);
    for (Property* prop : _defaultProperties.propertiesRecursive()) {
        prop->onChange(markStyleDirty);
    }

    readFile();

    if (p.lightSources.has_value()) {
        const std::vector<ghoul::Dictionary> lightsources = *p.lightSources;

        for (const ghoul::Dictionary& lsDictionary : lightsources) {
            std::unique_ptr<LightSource> lightSource =
                LightSource::createFromDictionary(lsDictionary);
            _lightSourcePropertyOwner.addPropertySubOwner(lightSource.get());
            _lightSources.push_back(std::move(lightSource));
        }
    }
    else {
        // If no light source provided, add a deafult light source from the camera
        using namespace std::string_literals;
        ghoul::Dictionary defaultLightSourceDict;
        defaultLightSourceDict.setValue("Identifier", "Camera"s);
        defaultLightSourceDict.setValue("Type", "CameraLightSource"s);
        defaultLightSourceDict.setValue("Intensity", 1.0);
        _lightSources.push_back(
            LightSource::createFromDictionary(defaultLightSourceDict)
        );
        _lightSourcePropertyOwner.addPropertySubOwner(_lightSources.back().get());
    }
    addPropertySubOwner(_lightSourcePropertyOwner);
    addPropertySubOwner(_featuresPropertyOwner);
}

GeoJsonComponent::~GeoJsonComponent() {}

bool GeoJsonComponent::enabled() const {
    return _enabled;
}

bool GeoJsonComponent::styleIsDirty() const {
    return _styleIsDirty;
}

void GeoJsonComponent::clearStyleDirty() {
    _styleIsDirty = false;
}

void GeoJsonComponent::initialize() {
    ZoneScoped;

    for (const std::unique_ptr<LightSource>& ls : _lightSources) {
        ls->initialize();
    }
}

void GeoJsonComponent::initializeGL(rendering::MultiDrawBatch* pointsBatch,
                                    rendering::MultiDrawBatch* linesBatch,
                                    rendering::MultiDrawBatch* polygonsBatch)
{
    ZoneScoped;

    _pointsBatch = pointsBatch;
    _linesBatch = linesBatch;
    _polygonsBatch = polygonsBatch;

    for (GlobeGeometryFeature& g : _geometryFeatures) {
        g.initializeGL(_pointsBatch, _linesBatch, _polygonsBatch);
    }
}

void GeoJsonComponent::deinitializeGL() {
    // Persist the refined height map heights so that the next load of this file
    // starts from the best known values instead of re-sampling from scratch. This
    // must happen before the features release their render data below
    if (_heightsDirtyForCache && !_heightsCacheFile.empty()) {
        geojson::GeoJsonHeightsCacheFile out;
        out.features.reserve(_geometryFeatures.size());
        for (const GlobeGeometryFeature& g : _geometryFeatures) {
            geojson::CachedFeatureHeights h;
            h.controlHeights = g.lastControlHeights();
            h.perRenderFeatureHeights = g.currentHeights();
            out.features.push_back(std::move(h));
        }
        geojson::saveGeoJsonHeightsCache(out, _heightsCacheFile);
        _heightsDirtyForCache = false;
    }

    for (GlobeGeometryFeature& g : _geometryFeatures) {
        g.deinitializeGL();
    }

    _pointsBatch = nullptr;
    _linesBatch = nullptr;
    _polygonsBatch = nullptr;
    _isReadyCached = false;
}

bool GeoJsonComponent::isReady() const {
    // Called every frame; scanning tens of thousands of features is not free, and once
    // everything is ready it stays ready until deinitializeGL or a file reload
    if (_isReadyCached) {
        return true;
    }
    const bool isReady = std::all_of(
        _geometryFeatures.cbegin(),
        _geometryFeatures.cend(),
        std::mem_fn(&GlobeGeometryFeature::isReady)
    );
    _isReadyCached = isReady;
    return isReady;
}

bool GeoJsonComponent::hasPolygonsToDraw() const {
    // Fill polygons only exist for Polygon geometry, and extrusion polygons only draw
    // when the feature's resolved extrude value is true. Extrude overrides are static,
    // so only the default property is read here
    return _nFillPolygonFeatures > 0 ||
        _nExtrudeTrueOverride > 0 ||
        (_nExtrudableNoOverride > 0 && _defaultProperties.extrude);
}

void GeoJsonComponent::updateLightSources(const RenderData& data) {
    // @TODO (2023-03-17, emmbr): Once the light source for the globe can be configured,
    // this code should use the same light source as the globe
    _lightsourceRenderData.updateBasedOnLightSources(data, _lightSources);
}

const rendering::LightSourceRenderData& GeoJsonComponent::lightSourceRenderData() const {
    return _lightsourceRenderData;
}

void GeoJsonComponent::emitBatchedDraws(
                              std::map<int64_t, ghoul::opengl::Texture*>& pointTextures,
                                       int componentIndex,
                                       const geojson::GeoJsonCullContext* cullContext)
{
    if (!_enabled || !isVisible()) {
        return;
    }

    // Slack that grows the bounding spheres in the cull tests: the draw-time height
    // displacements plus the camera motion the cull result must stay valid for
    double slackBase = 0.0;
    if (cullContext) {
        slackBase = std::abs(_heightOffset.value()) + _maxSampledHeight +
            geojson::HeightSlackPadding + cullContext->posThreshold;
        _heightSlackAtLastEmit = _maxSampledHeight +
            static_cast<float>(geojson::HeightSlackPadding);
    }
    else {
        // Without culling there is no height bound the sweep could invalidate
        _heightSlackAtLastEmit = std::numeric_limits<float>::max();
    }

    using PointRenderMode = GlobeGeometryFeature::PointRenderMode;
    PointRenderMode pointRenderMode =
        static_cast<PointRenderMode>(_pointRenderModeOption.value());

    const int64_t polygonGroupBase =
        (static_cast<int64_t>(componentIndex) <<
            GeoJsonDrawRecord::PolygonComponentShift) |
        (_drawWireframe ? GeoJsonDrawRecord::WireframeGroupBit : 0);

    const GlobeGeometryFeature::ExtraRenderData extraRenderdata = {
        _pointSizeScale,
        _lineWidthScale,
        pointRenderMode,
        _lightsourceRenderData,
        polygonGroupBase
    };

    for (size_t i = 0; i < _geometryFeatures.size(); i++) {
        if (_createPerFeatureProps &&
            (!_features[i]->enabled || !_features[i]->isVisible()))
        {
            continue;
        }

        if (cullContext && i < _featureCullSpheres.size()) {
            const geojson::FeatureCullSphere& sphere = _featureCullSpheres[i];
            const double slack = sphere.isPoints ?
                slackBase + _geometryFeatures[i].pointCullSlack(_pointSizeScale) :
                slackBase;

            if (cullContext->frustumEnabled &&
                geojson::isSphereOutsideFrustum(sphere, *cullContext, slack))
            {
                continue;
            }
            if (cullContext->horizonEnabled &&
                geojson::isSphereBeyondHorizon(sphere, *cullContext, slack))
            {
                continue;
            }
        }

        const float featureOpacity =
            _createPerFeatureProps ? _features[i]->opacity() : 1.f;
        _geometryFeatures[i].emitBatchedDraws(
            opacity() * featureOpacity,
            extraRenderdata,
            _drawWireframe,
            pointTextures
        );
    }
}

bool GeoJsonComponent::update() {
    if (!_enabled || !isVisible()) {
        return false;
    }

    // Heights may need re-sampling from the height map (RelativeToGround altitude
    // mode) as tiles stream in. Altitude mode overrides are static after load; the
    // default property is live. A globe without active height layers reports zero
    // heights everywhere, so there is nothing to refine
    using AltitudeMode = GeoJsonProperties::AltitudeMode;
    const bool anyRelativeToGround = _nRelativeToGroundOverride > 0 ||
        (_nAltitudeModeNoOverride > 0 &&
            _defaultProperties.altitudeMode() == AltitudeMode::RelativeToGround);
    const bool globeHasHeightLayers = !_globeNode.layerManager().layerGroup(
        layers::Group::ID::HeightLayers
    ).activeLayers().empty();
    const bool heightsEnabled = anyRelativeToGround && globeHasHeightLayers &&
        !_preventUpdatesFromHeightMap;

    const auto now = std::chrono::steady_clock::now();
    if (heightsEnabled && !_heightSweepActive &&
        now - _lastSweepStart >= MinSweepInterval)
    {
        _heightSweepActive = true;
        _heightSweepCursor = 0;
        _lastSweepStart = now;
    }

    // Skip all per-feature work on idle frames
    const bool featureLoopNeeded = _dataIsDirty || _heightOffsetIsDirty ||
        _textureIsDirty || _nPointTextureFeatures > 0;
    if (!featureLoopNeeded && !(heightsEnabled && _heightSweepActive)) {
        return false;
    }

    const bool dataWasDirty = _dataIsDirty;
    bool geometryChanged = false;

    if (featureLoopNeeded) {
        const glm::vec3 offsets =
            glm::vec3(_latLongOffset.value(), _heightOffset.value());
        const bool textureWasDirty = _textureIsDirty;

        // Note that geometry is updated even for disabled sub-features, so that their
        // draws exist in the shared batches and they can be enabled or faded in
        // without a rebuild
        for (GlobeGeometryFeature& g : _geometryFeatures) {
            if (_dataIsDirty || _heightOffsetIsDirty) [[unlikely]] {
                g.setOffsets(offsets);
            }

            if (_textureIsDirty) [[unlikely]] {
                g.updateTexture();
                // A changed texture changes the point draw group keys
                _styleIsDirty = true;
            }

            geometryChanged |= g.update(_dataIsDirty);
        }

        if (textureWasDirty) {
            _nPointTextureFeatures = 0;
            for (const GlobeGeometryFeature& g : _geometryFeatures) {
                if (g.hasPointTexture()) {
                    _nPointTextureFeatures++;
                }
            }
        }

        _textureIsDirty = false;
        _dataIsDirty = false;
        _heightOffsetIsDirty = false;
    }

    // Advance the height refinement sweep, budgeted per frame both in reference-point
    // check time and in re-sampled vertices. Skipped on rebuild frames, where the
    // fresh geometry establishes its own reference state
    if (heightsEnabled && _heightSweepActive && !dataWasDirty) {
        const auto sweepStart = std::chrono::steady_clock::now();
        size_t resampledVertices = 0;
        while (_heightSweepCursor < _geometryFeatures.size()) {
            GlobeGeometryFeature& g = _geometryFeatures[_heightSweepCursor];

            std::optional<std::vector<double>> newHeights =
                g.checkHeightMapChange(_forceResampleAll);
            if (newHeights.has_value()) {
                // Always allow at least one re-sample per frame, even a large one
                if (resampledVertices > 0 &&
                    resampledVertices + g.heightVertexCount() >
                        SweepResampleVertexBudget)
                {
                    break; // cursor stays; this feature is re-sampled next frame
                }
                g.applyHeightUpdate(std::move(*newHeights));
                _heightsDirtyForCache = true;
                resampledVertices += g.heightVertexCount();

                _maxSampledHeight =
                    std::max(_maxSampledHeight, g.maxAbsSampledHeight());
                if (_maxSampledHeight > _heightSlackAtLastEmit) {
                    // The grown heights may exceed what the last cull results
                    // covered, so force a re-emit with fresh culling
                    _styleIsDirty = true;
                }
            }
            _heightSweepCursor++;

            if (std::chrono::steady_clock::now() - sweepStart > SweepCheckTimeBudget) {
                break;
            }
        }
        if (_heightSweepCursor >= _geometryFeatures.size()) {
            _heightSweepActive = false;
            _forceResampleAll = false;
        }
    }

    if (geometryChanged) {
        rebuildCullSpheres();
    }

    return geometryChanged;
}

void GeoJsonComponent::rebuildCullSpheres() {
    _featureCullSpheres.clear();
    _featureCullSpheres.reserve(_geometryFeatures.size());

    float maxHeight = 0.f;
    for (const GlobeGeometryFeature& g : _geometryFeatures) {
        geojson::FeatureCullSphere sphere;
        if (g.hasCullSphere()) {
            sphere.center = g.cullSphereCenter();
            sphere.radius = g.cullSphereRadius() + geojson::SphereEpsilon;
            sphere.distSq = glm::dot(sphere.center, sphere.center);
            sphere.isPoints = g.isPoints();
        }
        _featureCullSpheres.push_back(sphere);
        maxHeight = std::max(maxHeight, g.maxAbsSampledHeight());
    }
    _maxSampledHeight = maxHeight;
}

void GeoJsonComponent::readFile() {
    ZoneScoped;

    const std::filesystem::path source = absPath(_geoJsonFile.value());
    if (!std::filesystem::is_regular_file(source)) {
        LERROR(std::format("Failed to open GeoJSON file: {}", _geoJsonFile.value()));
        return;
    }

    _geometryFeatures.clear();
    _features.clear();
    _featureMeta.clear();
    _usedFeatureIdentifiers.clear();
    _loadedHeightsCache.reset();
    _heightsCacheFile.clear();
    _heightsDirtyForCache = false;
    _isReadyCached = false;
    _styleIsDirty = true;
    _nFillPolygonFeatures = 0;
    _nExtrudeTrueOverride = 0;
    _nExtrudableNoOverride = 0;
    _nRelativeToGroundOverride = 0;
    _nAltitudeModeNoOverride = 0;
    _nPointTextureFeatures = 0;

    using Clock = std::chrono::steady_clock;
    const auto secs = [](Clock::duration d) {
        return std::chrono::duration<double>(d).count();
    };
    const Clock::time_point startTime = Clock::now();

    // Look for a load cache with the fully derived feature data, keyed on the cache
    // version, the ignore-heights flag (it changes the derived coordinates) and the
    // source file's last write time (providing an information string disables the
    // CacheManager's own modification-time keying)
    std::filesystem::path cacheFile;
    std::string cacheInfo;
    if (FileSys.cacheManager()) {
        const int64_t lastWrite =
            std::filesystem::last_write_time(source).time_since_epoch().count();
        cacheInfo = std::format(
            "geojson|v{}|ih{}|{}",
            geojson::CacheVersion, _ignoreHeightsFromFile ? 1 : 0, lastWrite
        );
        cacheFile = FileSys.cacheManager()->cachedFilename(source, cacheInfo);

        // The heights sidecar cache holds the height map heights refined during
        // earlier runs. It shares the source-modification-time component with the
        // geometry cache above, so editing the source file invalidates both. It is
        // additionally keyed on the globe (heights are per height map) and on the
        // lat/long offset (it moves the sample positions). Missing or mismatching
        // sidecar just means heights start at zero and refine at runtime
        const glm::vec2 latLongOffset = _latLongOffset.value();
        const std::string heightsCacheInfo = std::format(
            "geojsonheights|v{}|globe{}|ih{}|off{}_{}|{}",
            geojson::HeightsCacheVersion,
            _globeNode.owner() ? _globeNode.owner()->identifier() : "unknown",
            _ignoreHeightsFromFile ? 1 : 0,
            latLongOffset.x, latLongOffset.y,
            lastWrite
        );
        _heightsCacheFile =
            FileSys.cacheManager()->cachedFilename(source, heightsCacheInfo);
        if (std::filesystem::is_regular_file(_heightsCacheFile)) {
            std::optional<geojson::GeoJsonHeightsCacheFile> heights =
                geojson::loadGeoJsonHeightsCache(_heightsCacheFile);
            if (heights && heights->version == geojson::HeightsCacheVersion) {
                LDEBUG(std::format(
                    "Using cached height map heights for {} features of '{}'",
                    heights->features.size(), _geoJsonFile.value()
                ));
                _loadedHeightsCache = std::move(heights);
            }
        }

        if (std::filesystem::is_regular_file(cacheFile)) {
            std::optional<geojson::GeoJsonCacheFile> cached =
                geojson::loadGeoJsonCache(cacheFile);
            if (cached && cached->version == geojson::CacheVersion &&
                cached->ignoreHeights == _ignoreHeightsFromFile)
            {
                size_t nRendered = 0;
                for (const geojson::CachedSourceFeature& f : cached->features) {
                    nRendered += f.rendered.size();
                }
                decidePerFeatureProps(nRendered);

                loadFromCache(*cached);
                computeMainFeatureMetaPropeties();
                countPointTextureFeatures();
                _loadedHeightsCache.reset();
                LINFO(std::format(
                    "Loaded '{}' from cache: {} features in {:.2f} s",
                    _geoJsonFile.value(), _geometryFeatures.size(),
                    secs(Clock::now() - startTime)
                ));
                return;
            }
            FileSys.cacheManager()->removeCacheFile(source, cacheInfo);
        }
    }

    std::ifstream file = std::ifstream(source);
    if (!file.good()) {
        LERROR(std::format("Failed to open GeoJSON file: {}", _geoJsonFile.value()));
        return;
    }
    const std::string content = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );

    // For the loading, we want to assume that the current working directory is where the
    // GeoJSON file is located
    const std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path jsonDir =
        std::filesystem::path(_geoJsonFile.value()).parent_path();
    std::filesystem::current_path(jsonDir);
    defer { std::filesystem::current_path(cwd); };

    int nFeatures = 0;
    geojson::GeoJsonCacheFile cacheOut;
    cacheOut.ignoreHeights = _ignoreHeightsFromFile;

    // Parse GeoJSON string into GeoJSON objects
    try {
        const geojson::ParsedGeoJson parsed = geojson::parseGeoJson(content);

        decidePerFeatureProps(parsed.features.size());

        for (const geojson::ParsedFeature& feature : parsed.features) {
            nFeatures++;
            parseSingleFeature(feature, nFeatures, cacheOut);
        }

        if (_geometryFeatures.empty()) {
            LWARNING(std::format(
                "No GeoJson features could be successfully created for GeoJson layer "
                "with identifier '{}'. Disabling layer", identifier()
            ));
            _enabled = false;
        }
    }
    catch (const ghoul::RuntimeError& e) {
        LERROR(std::format(
            "Error creating GeoJson layer with identifier '{}'. Problem reading "
            "GeoJson file '{}'. Error: {}", identifier(), _geoJsonFile.value(), e.message
        ));
    }
    catch (const geos::util::GEOSException& e) {
        LERROR(std::format(
            "Error creating GeoJson layer with identifier '{}'. Problem reading "
            "GeoJson file '{}'. Error: {}", identifier(), _geoJsonFile.value(), e.what()
        ));
    }

    computeMainFeatureMetaPropeties();
    countPointTextureFeatures();
    _loadedHeightsCache.reset();

    if (FileSys.cacheManager() && !_geometryFeatures.empty()) {
        geojson::saveGeoJsonCache(cacheOut, cacheFile);
    }

    LINFO(std::format(
        "Loaded '{}': {} features ({} rendered) in {:.2f} s",
        _geoJsonFile.value(), nFeatures, _geometryFeatures.size(),
        secs(Clock::now() - startTime)
    ));
}

void GeoJsonComponent::decidePerFeatureProps(size_t nFeatures) {
    _createPerFeatureProps =
        nFeatures <= static_cast<size_t>(_perFeaturePropertiesThreshold);
    if (!_createPerFeatureProps) {
        LINFO(std::format(
            "GeoJson layer '{}' has {} features, more than the "
            "PerFeaturePropertiesThreshold of {}. Per-feature settings (enable, fade, "
            "fly-to) are not created; component-level settings still apply",
            identifier(), nFeatures, _perFeaturePropertiesThreshold
        ));
    }
}

void GeoJsonComponent::countPointTextureFeatures() {
    _nPointTextureFeatures = 0;
    for (const GlobeGeometryFeature& g : _geometryFeatures) {
        if (g.hasPointTexture()) {
            _nPointTextureFeatures++;
        }
    }
}

void GeoJsonComponent::installCachedHeights(int index) {
    if (!_loadedHeightsCache ||
        index >= static_cast<int>(_loadedHeightsCache->features.size()))
    {
        return;
    }
    geojson::CachedFeatureHeights& h = _loadedHeightsCache->features[index];
    _geometryFeatures.back().setPendingCachedHeights(
        std::move(h.perRenderFeatureHeights),
        std::move(h.controlHeights)
    );
}

void GeoJsonComponent::parseSingleFeature(const geojson::ParsedFeature& feature,
                                          int indexInFile,
                                          geojson::GeoJsonCacheFile& cacheOut)
{
    std::unique_ptr<geos::geom::Geometry> ownedGeom;
    if (feature.geometry.kind != geojson::GeometryKind::None) {
        ownedGeom = geojson::buildGeosGeometry(feature.geometry);
    }
    const geos::geom::Geometry* geom = ownedGeom.get();

    // Only repair geometry that is actually invalid; MakeValid is expensive and the
    // vast majority of features are valid
    if (geom && !geom->isValid()) {
        LWARNING(std::format(
            "Feature {} in GeoJson file '{}' has invalid geometry (for example due to "
            "self-intersections or other non-simple geometry). If possible, the feature "
            "will be split into separate features with valid geometry. However, note "
            "that this may introduce artifacts", indexInFile, _geoJsonFile.value()
        ));
        geos::operation::valid::MakeValid makeValid;
        ownedGeom = makeValid.build(geom);
        geom = ownedGeom.get();
    }

    // Read the properties
    GeoJsonOverrideProperties propsFromFile = propsFromGeoJson(
        feature.properties,
        std::format("feature {} in GeoJson file '{}'", indexInFile, _geoJsonFile.value())
    );

    std::vector<const geos::geom::Geometry*> geomsToAdd;
    if (!geom) {
        // Null geometry => no geometries to add
        LWARNING(std::format(
            "Feature {} in GeoJson file '{}' is a null geometry and will not be loaded",
            indexInFile, _geoJsonFile.value()
        ));
        // @TODO (emmbr26) We should eventually support features with null geometry
    }
    else if (geom->isPuntal()) {
        // If points, handle all point features as one feature, even multi-points
        geomsToAdd = { geom };
    }
    else {
        const size_t nGeom = geom->getNumGeometries();
        geomsToAdd.reserve(nGeom);
        for (size_t i = 0; i < nGeom; i++) {
            const geos::geom::Geometry* subGeometry = geom->getGeometryN(i);
            if (subGeometry) {
                geomsToAdd.push_back(subGeometry);
            }
        }
    }

    geojson::CachedSourceFeature cachedFeature;
    cachedFeature.overrides = geojson::toCached(propsFromFile);

    // Split other collection features into multiple individual rendered components

    for (const geos::geom::Geometry* geometry : geomsToAdd) {
        const int index = static_cast<int>(_geometryFeatures.size());
        try {
            GlobeGeometryFeature g(_globeNode, _defaultProperties, propsFromFile);
            g.createFromSingleGeosGeometry(geometry, index, _ignoreHeightsFromFile);
            g.initializeGL(_pointsBatch, _linesBatch, _polygonsBatch);
            _geometryFeatures.push_back(std::move(g));
            installCachedHeights(index);

            std::string name = _geometryFeatures.back().key();
            std::string identifier = makeIdentifier(name);

            // If there is already a feature with that identifier, make a unique one
            if (!_usedFeatureIdentifiers.insert(identifier).second) {
                identifier = std::format("Feature{}-{}", index, identifier);
                _usedFeatureIdentifiers.insert(identifier);
            }

            const FeatureMeta meta = computeFeatureMeta(geometry);
            _featureMeta.push_back(meta);

            if (_createPerFeatureProps) {
                const PropertyOwner::PropertyOwnerInfo info = {
                    identifier,
                    name
                    // @TODO: Use description from file, if any
                };
                _features.push_back(std::make_unique<SubFeatureProps>(info));
                _features.back()->onStyleChange([this]() { _styleIsDirty = true; });
                applyMetaToFeature(*_features.back(), meta, index);
                _featuresPropertyOwner.addPropertySubOwner(_features.back().get());
            }

            using GeometryType = GlobeGeometryFeature::GeometryType;
            const GeometryType featureType = _geometryFeatures.back().type();
            if (featureType == GeometryType::Polygon) {
                _nFillPolygonFeatures++;
            }
            else if (featureType == GeometryType::LineString) {
                if (propsFromFile.extrude.value_or(false)) {
                    _nExtrudeTrueOverride++;
                }
                else if (!propsFromFile.extrude.has_value()) {
                    _nExtrudableNoOverride++;
                }
            }
            using AltitudeMode = GeoJsonProperties::AltitudeMode;
            if (propsFromFile.altitudeMode.has_value()) {
                if (*propsFromFile.altitudeMode == AltitudeMode::RelativeToGround) {
                    _nRelativeToGroundOverride++;
                }
            }
            else {
                _nAltitudeModeNoOverride++;
            }

            // Record the derived data for the load cache
            const GlobeGeometryFeature& gf = _geometryFeatures.back();
            geojson::CachedRenderedFeature cached;
            cached.geometryType = static_cast<int32_t>(gf.type());
            cached.geoCoordinates.reserve(gf.geoCoordinates().size());
            for (const std::vector<Geodetic3>& ring : gf.geoCoordinates()) {
                cached.geoCoordinates.push_back(geojson::toCached(ring));
            }
            cached.triangleCoordinates = geojson::toCached(gf.triangleCoordinates());
            cached.heightUpdateReferencePoints =
                geojson::toCached(gf.heightUpdateReferencePoints());
            cached.key = gf.key();
            cached.centroidLatLong = { meta.centroidLatLong.x, meta.centroidLatLong.y };
            const glm::vec4 bbox = meta.boundingboxLatLong;
            cached.boundingboxLatLong = { bbox.x, bbox.y, bbox.z, bbox.w };
            cached.identifier = std::move(identifier);
            cached.name = std::move(name);
            cachedFeature.rendered.push_back(std::move(cached));
        }
        catch (const ghoul::RuntimeError& error) {
            LERROR(std::format(
                "Error creating GeoJson layer with identifier '{}'. Problem reading "
                "feature {} in GeoJson file '{}'",
                identifier(), indexInFile, _geoJsonFile.value()
            ));
            LERRORC(error.component, error.message);
            // Do nothing
        }
    }

    // Features that failed and were skipped are simply absent from the cache, so warm
    // loads reproduce the surviving set (without repeating the cold-path warnings)
    if (!cachedFeature.rendered.empty()) {
        cacheOut.features.push_back(std::move(cachedFeature));
    }
}

void GeoJsonComponent::loadFromCache(const geojson::GeoJsonCacheFile& cache) {
    for (const geojson::CachedSourceFeature& source : cache.features) {
        GeoJsonOverrideProperties props = geojson::fromCached(source.overrides);

        for (const geojson::CachedRenderedFeature& r : source.rendered) {
            const int index = static_cast<int>(_geometryFeatures.size());

            GlobeGeometryFeature g(_globeNode, _defaultProperties, props);
            std::vector<std::vector<Geodetic3>> geoCoords;
            geoCoords.reserve(r.geoCoordinates.size());
            for (const std::vector<geojson::CachedGeodetic3>& ring : r.geoCoordinates) {
                geoCoords.push_back(geojson::fromCached(ring));
            }
            g.setFromCachedData(
                static_cast<GlobeGeometryFeature::GeometryType>(r.geometryType),
                std::move(geoCoords),
                geojson::fromCached(r.triangleCoordinates),
                geojson::fromCached(r.heightUpdateReferencePoints),
                r.key
            );
            g.initializeGL(_pointsBatch, _linesBatch, _polygonsBatch);
            _geometryFeatures.push_back(std::move(g));
            installCachedHeights(index);

            FeatureMeta meta;
            meta.centroidLatLong =
                glm::vec2(r.centroidLatLong[0], r.centroidLatLong[1]);
            meta.boundingboxLatLong = glm::vec4(
                r.boundingboxLatLong[0],
                r.boundingboxLatLong[1],
                r.boundingboxLatLong[2],
                r.boundingboxLatLong[3]
            );
            computeFeatureDiagonal(meta);
            _featureMeta.push_back(meta);

            if (_createPerFeatureProps) {
                // Identifiers were de-duplicated on the cold path and are replayed
                // verbatim, in the same order, so the GUI listing is identical
                const PropertyOwner::PropertyOwnerInfo info = { r.identifier, r.name };
                _features.push_back(std::make_unique<SubFeatureProps>(info));
                _features.back()->onStyleChange([this]() { _styleIsDirty = true; });
                applyMetaToFeature(*_features.back(), meta, index);
                _featuresPropertyOwner.addPropertySubOwner(_features.back().get());
            }

            using GeometryType = GlobeGeometryFeature::GeometryType;
            const GeometryType featureType = _geometryFeatures.back().type();
            if (featureType == GeometryType::Polygon) {
                _nFillPolygonFeatures++;
            }
            else if (featureType == GeometryType::LineString) {
                if (props.extrude.value_or(false)) {
                    _nExtrudeTrueOverride++;
                }
                else if (!props.extrude.has_value()) {
                    _nExtrudableNoOverride++;
                }
            }
            using AltitudeMode = GeoJsonProperties::AltitudeMode;
            if (props.altitudeMode.has_value()) {
                if (*props.altitudeMode == AltitudeMode::RelativeToGround) {
                    _nRelativeToGroundOverride++;
                }
            }
            else {
                _nAltitudeModeNoOverride++;
            }
        }
    }
}

GeoJsonComponent::FeatureMeta GeoJsonComponent::computeFeatureMeta(
                                            const geos::geom::Geometry* geometry) const
{
    FeatureMeta meta;

    std::unique_ptr<geos::geom::Point> centroid = geometry->getCentroid();
    const geos::geom::CoordinateXY centroidCoord = *centroid->getCoordinate();
    meta.centroidLatLong = glm::vec2(centroidCoord.y, centroidCoord.x);

    std::unique_ptr<geos::geom::Geometry> boundingbox = geometry->getEnvelope();
    std::unique_ptr<geos::geom::CoordinateSequence> coords =
        boundingbox->getCoordinates();
    if (boundingbox->isRectangle()) {
        // A rectangle has 5 coordinates, where the first and third are two corners
        meta.boundingboxLatLong = glm::vec4(
            (*coords)[0].y,
            (*coords)[0].x,
            (*coords)[2].y,
            (*coords)[2].x
        );
    }
    else {
        // Invalid boundingbox. Can happen e.g. for single points. Just add a degree to
        // every direction from the centroid
        meta.boundingboxLatLong = glm::vec4(
            meta.centroidLatLong.x - 1.f,
            meta.centroidLatLong.y - 1.f,
            meta.centroidLatLong.x + 1.f,
            meta.centroidLatLong.y + 1.f
        );
    }

    computeFeatureDiagonal(meta);
    return meta;
}

void GeoJsonComponent::applyMetaToFeature(SubFeatureProps& feature,
                                          const FeatureMeta& meta, int index)
{
    feature.centroidLatLong = meta.centroidLatLong;
    feature.boundingboxLatLong = meta.boundingboxLatLong;
    feature.boundingBoxDiagonal = meta.boundingBoxDiagonal;
    feature.flyToFeature.onChange([this, index]() { flyToFeature(index); });
}

void GeoJsonComponent::computeFeatureDiagonal(FeatureMeta& meta) const {
    // Compute the diagonal distance of the bounding box
    const Geodetic2 pos0 = {
        glm::radians(meta.boundingboxLatLong.x),
        glm::radians(meta.boundingboxLatLong.y)
    };

    const Geodetic2 pos1 = {
        glm::radians(meta.boundingboxLatLong.z),
        glm::radians(meta.boundingboxLatLong.w)
    };
    meta.boundingBoxDiagonal = static_cast<float>(
        std::abs(_globeNode.ellipsoid().greatCircleDistance(pos0, pos1))
    );
}

void GeoJsonComponent::computeMainFeatureMetaPropeties() {
    if (_featureMeta.empty()) {
        return;
    }

    glm::vec2 bboxLowerCorner = glm::vec2(
        _featureMeta.front().boundingboxLatLong.x,
        _featureMeta.front().boundingboxLatLong.y
    );
    glm::vec2 bboxUpperCorner = glm::vec2(
        _featureMeta.front().boundingboxLatLong.z,
        _featureMeta.front().boundingboxLatLong.w
    );

    for (const FeatureMeta& m : _featureMeta) {
        // Update bbox corners
        bboxLowerCorner.x = std::min(bboxLowerCorner.x, m.boundingboxLatLong.x);
        bboxLowerCorner.y = std::min(bboxLowerCorner.y, m.boundingboxLatLong.y);
        bboxUpperCorner.x = std::max(bboxUpperCorner.x, m.boundingboxLatLong.z);
        bboxUpperCorner.y = std::max(bboxUpperCorner.y, m.boundingboxLatLong.w);
    }

    // Identify the bounding box midpoints
    _centerLatLong = 0.5f * (bboxLowerCorner + bboxUpperCorner);

    // Compute the diagonal distance (size) of the bounding box
    const Geodetic2 pos0 = {
        .lat = glm::radians(bboxLowerCorner.x),
        .lon = glm::radians(bboxLowerCorner.y)
    };

    const Geodetic2 pos1 = {
        .lat = glm::radians(bboxUpperCorner.x),
        .lon = glm::radians(bboxUpperCorner.y)
    };
    _bboxDiagonalSize = static_cast<float>(
        std::abs(_globeNode.ellipsoid().greatCircleDistance(pos0, pos1))
    );
}

void GeoJsonComponent::flyToFeature(std::optional<int> index) const {
    // General size properties
    float diagonal = _bboxDiagonalSize;
    float centroidLat = _centerLatLong.value().x;
    float centroidLon = _centerLatLong.value().y;

    if (index.has_value()) {
        const FeatureMeta& m = _featureMeta[*index];
        diagonal = m.boundingBoxDiagonal;
        centroidLat = m.centroidLatLong.x;
        centroidLon = m.centroidLatLong.y;
    }

    // Compute a good distance to travel to based on the feature's size. Assumes 80 degree
    // FOV
    constexpr float Angle = glm::radians(40.f);
    float d = diagonal / glm::tan(Angle);
    d += _heightOffset;

    float lat = centroidLat + _latLongOffset.value().x;
    float lon = centroidLon + _latLongOffset.value().y;

    const std::string script = std::format(
        "openspace.navigation.flyToGeo([[{}]], {}, {}, {})",
        _globeNode.owner()->identifier(), lat, lon, d
    );
    global::scriptEngine->queueScript(script);
}

void GeoJsonComponent::triggerDeletion() const {
    const std::string script = std::format(
        "openspace.globebrowsing.deleteGeoJson([[{}]], [[{}]])",
        _globeNode.owner()->identifier(), _identifier
    );
    global::scriptEngine->queueScript(script);
}

} // namespace openspace
