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
#include <ghoul/opengl/openglstatecache.h>
#include <ghoul/misc/defer.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/misc/profiling.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/Point.h>
#include <geos/util/GEOSException.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <utility>

#include <modules/globebrowsing/src/geojson/geojsoncache.h>
#include <modules/globebrowsing/src/geojson/geojsonparser.h>
#include <geos/geom/Geometry.h>
#include <geos/operation/valid/MakeValid.h>

namespace {
    using namespace openspace;

    constexpr std::string_view _loggerCat = "GeoJsonComponent";

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
        "height map value at the geometry's positions.",
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

    _forceUpdateHeightData.onChange([this]() {
        for (GlobeGeometryFeature& f : _geometryFeatures) {
            f.updateHeightsFromHeightMap();
        }
    });
    addProperty(_forceUpdateHeightData);

    _preventUpdatesFromHeightMap =
        p.preventHeightUpdate.value_or(_preventUpdatesFromHeightMap);
    addProperty(_preventUpdatesFromHeightMap);

    _drawWireframe = p.drawWireframe.value_or(_drawWireframe);
    addProperty(_drawWireframe);

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
    for (Property* p : _defaultProperties.propertiesRecursive()) {
        p->onChange(markStyleDirty);
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

void GeoJsonComponent::initializeGL(ghoul::opengl::ProgramObject* polygonsProgram,
                                    rendering::MultiDrawBatch* pointsBatch,
                                    rendering::MultiDrawBatch* linesBatch)
{
    ZoneScoped;

    _polygonsProgram = polygonsProgram;
    _pointsBatch = pointsBatch;
    _linesBatch = linesBatch;

    for (GlobeGeometryFeature& g : _geometryFeatures) {
        g.initializeGL(_polygonsProgram, _pointsBatch, _linesBatch);
    }
}

void GeoJsonComponent::deinitializeGL() {
    for (GlobeGeometryFeature& g : _geometryFeatures) {
        g.deinitializeGL();
    }

    _polygonsProgram = nullptr;
    _pointsBatch = nullptr;
    _linesBatch = nullptr;
    _isReadyCached = false;
}

bool GeoJsonComponent::isReady() const {
    // Called every frame; scanning tens of thousands of features is not free, and once
    // everything is ready it stays ready until deinitializeGL or a file reload
    if (_isReadyCached) {
        return true;
    }
    const bool isReady = _polygonsProgram && std::all_of(
        _geometryFeatures.cbegin(),
        _geometryFeatures.cend(),
        std::mem_fn(&GlobeGeometryFeature::isReady)
    );
    _isReadyCached = isReady;
    return isReady;
}

void GeoJsonComponent::render(const RenderData& data) {
    if (!_enabled || !isVisible()) {
        return;
    }

    // This pass only draws polygon render features (points and lines are batched by
    // the manager). Skip the whole per-feature loop when no feature can draw polygons:
    // fill polygons only exist for Polygon geometry, and extrusion polygons only draw
    // when the feature's resolved extrude value is true. Extrude overrides are static,
    // so only the default property is read here
    const bool anyPolygonsToDraw =
        _nFillPolygonFeatures > 0 ||
        _nExtrudeTrueOverride > 0 ||
        (_nExtrudableNoOverride > 0 && _defaultProperties.extrude);
    if (!anyPolygonsToDraw) {
        return;
    }

    // @TODO (2023-03-17, emmbr): Once the light source for the globe can be configured,
    // this code should use the same light source as the globe
    _lightsourceRenderData.updateBasedOnLightSources(data, _lightSources);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);

    if (_drawWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    using PointRenderMode = GlobeGeometryFeature::PointRenderMode;
    PointRenderMode pointRenderMode =
        static_cast<PointRenderMode>(_pointRenderModeOption.value());

    // Compose extra data from relevant properties to pass to the individual features
    const GlobeGeometryFeature::ExtraRenderData extraRenderdata = {
        _pointSizeScale,
        _lineWidthScale,
        pointRenderMode,
        _lightsourceRenderData
    };

    // Do two render passes, to properly render opacity of overlaying objects
    for (int renderPass = 0; renderPass < 2; renderPass++) {
        for (size_t i = 0; i < _geometryFeatures.size(); i++) {
            if (_features[i]->enabled && _features[i]->isVisible()) {
                _geometryFeatures[i].render(
                    data,
                    renderPass,
                    opacity() * _features[i]->opacity(),
                    extraRenderdata
                );
            }
        }
    }

    if (_drawWireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    glBindVertexArray(0);

    global::renderEngine->openglStateCache().resetPolygonAndClippingState();
    global::renderEngine->openglStateCache().resetBlendState();
    global::renderEngine->openglStateCache().resetDepthState();
    global::renderEngine->openglStateCache().resetLineState();
}

void GeoJsonComponent::emitBatchedDraws(
                                 std::map<int64_t, ghoul::opengl::Texture*>& pointTextures)
{
    if (!_enabled || !isVisible()) {
        return;
    }

    using PointRenderMode = GlobeGeometryFeature::PointRenderMode;
    PointRenderMode pointRenderMode =
        static_cast<PointRenderMode>(_pointRenderModeOption.value());

    const GlobeGeometryFeature::ExtraRenderData extraRenderdata = {
        _pointSizeScale,
        _lineWidthScale,
        pointRenderMode,
        _lightsourceRenderData
    };

    for (size_t i = 0; i < _geometryFeatures.size(); i++) {
        if (_features[i]->enabled && _features[i]->isVisible()) {
            _geometryFeatures[i].emitBatchedDraws(
                opacity() * _features[i]->opacity(),
                extraRenderdata,
                _drawWireframe,
                pointTextures
            );
        }
    }
}

bool GeoJsonComponent::update() {
    if (!_enabled || !isVisible()) {
        return false;
    }

    const glm::vec3 offsets = glm::vec3(_latLongOffset.value(), _heightOffset.value());

    bool geometryChanged = false;

    // Note that geometry is updated even for disabled sub-features, so that their draws
    // exist in the shared batches and they can be enabled or faded in without a rebuild
    for (GlobeGeometryFeature& g : _geometryFeatures) {
        if (_dataIsDirty || _heightOffsetIsDirty) [[unlikely]] {
            g.setOffsets(offsets);
        }

        if (_textureIsDirty) [[unlikely]] {
            g.updateTexture();
            // A changed texture changes the point draw group keys
            _styleIsDirty = true;
        }

        geometryChanged |= g.update(_dataIsDirty, _preventUpdatesFromHeightMap);
    }

    _textureIsDirty = false;
    _dataIsDirty = false;
    return geometryChanged;
}

void GeoJsonComponent::readFile() {
    ZoneScoped;

    const std::filesystem::path source = absPath(_geoJsonFile.value());
    if (!std::filesystem::is_regular_file(source)) {
        LERROR(std::format("Failed to open GeoJSON file: {}", _geoJsonFile.value()));
        return;
    }

    _geometryFeatures.clear();
    _isReadyCached = false;
    _styleIsDirty = true;
    _nFillPolygonFeatures = 0;
    _nExtrudeTrueOverride = 0;
    _nExtrudableNoOverride = 0;

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
        const auto lastWrite =
            std::filesystem::last_write_time(source).time_since_epoch().count();
        cacheInfo = std::format(
            "geojson|v{}|ih{}|{}",
            geojson::CacheVersion, _ignoreHeightsFromFile ? 1 : 0, lastWrite
        );
        cacheFile = FileSys.cacheManager()->cachedFilename(source, cacheInfo);

        if (std::filesystem::is_regular_file(cacheFile)) {
            std::optional<geojson::GeoJsonCacheFile> cached =
                geojson::loadGeoJsonCache(cacheFile);
            if (cached && cached->version == geojson::CacheVersion &&
                cached->ignoreHeights == _ignoreHeightsFromFile)
            {
                loadFromCache(*cached);
                computeMainFeatureMetaPropeties();
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

    LoadStats stats;
    int count = 0;
    geojson::GeoJsonCacheFile cacheOut;
    cacheOut.ignoreHeights = _ignoreHeightsFromFile;

    // Parse GeoJSON string into GeoJSON objects
    try {
        const Clock::time_point parseStart = Clock::now();
        const geojson::ParsedGeoJson parsed = geojson::parseGeoJson(content);
        stats.parse = Clock::now() - parseStart;

        for (const geojson::ParsedFeature& feature : parsed.features) {
            count++;
            parseSingleFeature(feature, count, stats, cacheOut);
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

    Clock::duration cacheWrite{};
    if (FileSys.cacheManager() && !_geometryFeatures.empty()) {
        const Clock::time_point cacheStart = Clock::now();
        geojson::saveGeoJsonCache(cacheOut, cacheFile);
        cacheWrite = Clock::now() - cacheStart;
    }

    LINFO(std::format(
        "Loaded '{}': {} features ({} rendered) in {:.2f} s (parse {:.2f} s, validate "
        "{:.2f} s, derive {:.2f} s, register {:.2f} s, cache write {:.2f} s)",
        _geoJsonFile.value(), count, _geometryFeatures.size(),
        secs(Clock::now() - startTime), secs(stats.parse), secs(stats.validate),
        secs(stats.derive), secs(stats.registration), secs(cacheWrite)
    ));
}

void GeoJsonComponent::parseSingleFeature(const geojson::ParsedFeature& feature,
                                          int indexInFile, LoadStats& stats,
                                          geojson::GeoJsonCacheFile& cacheOut)
{
    using Clock = std::chrono::steady_clock;

    const Clock::time_point validateStart = Clock::now();
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
    stats.validate += Clock::now() - validateStart;

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
            const Clock::time_point deriveStart = Clock::now();
            GlobeGeometryFeature g(_globeNode, _defaultProperties, propsFromFile);
            g.createFromSingleGeosGeometry(geometry, index, _ignoreHeightsFromFile);
            g.initializeGL(_polygonsProgram, _pointsBatch, _linesBatch);
            _geometryFeatures.push_back(std::move(g));
            stats.derive += Clock::now() - deriveStart;

            std::string name = _geometryFeatures.back().key();
            std::string identifier = makeIdentifier(name);

            const Clock::time_point registerStart = Clock::now();

            // If there is already an owner with that name as an identifier, make a unique
            // one
            if (_featuresPropertyOwner.hasPropertySubOwner(identifier)) {
                identifier = std::format("Feature{}-{}", index, identifier);
            }

            const PropertyOwner::PropertyOwnerInfo info = {
                std::move(identifier),
                std::move(name)
                // @TODO: Use description from file, if any
            };
            _features.push_back(std::make_unique<SubFeatureProps>(info));
            _features.back()->onStyleChange([this]() { _styleIsDirty = true; });

            addMetaPropertiesToFeature(*_features.back(), index, geometry);

            _featuresPropertyOwner.addPropertySubOwner(_features.back().get());
            stats.registration += Clock::now() - registerStart;

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

            // Record the derived data for the load cache
            const GlobeGeometryFeature& gf = _geometryFeatures.back();
            const SubFeatureProps& props = *_features.back();
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
            const glm::vec2 centroid = props.centroidLatLong;
            cached.centroidLatLong = { centroid.x, centroid.y };
            const glm::vec4 bbox = props.boundingboxLatLong;
            cached.boundingboxLatLong = { bbox.x, bbox.y, bbox.z, bbox.w };
            cached.identifier = props.identifier();
            cached.name = props.guiName();
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
            g.initializeGL(_polygonsProgram, _pointsBatch, _linesBatch);
            _geometryFeatures.push_back(std::move(g));

            // Identifiers were de-duplicated on the cold path and are replayed
            // verbatim, in the same order, so the GUI listing is identical
            const PropertyOwner::PropertyOwnerInfo info = { r.identifier, r.name };
            _features.push_back(std::make_unique<SubFeatureProps>(info));
            SubFeatureProps& f = *_features.back();
            f.onStyleChange([this]() { _styleIsDirty = true; });
            f.centroidLatLong =
                glm::vec2(r.centroidLatLong[0], r.centroidLatLong[1]);
            f.boundingboxLatLong = glm::vec4(
                r.boundingboxLatLong[0],
                r.boundingboxLatLong[1],
                r.boundingboxLatLong[2],
                r.boundingboxLatLong[3]
            );
            computeFeatureDiagonal(f);
            f.flyToFeature.onChange([this, index]() { flyToFeature(index); });

            _featuresPropertyOwner.addPropertySubOwner(_features.back().get());

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
        }
    }
}

void GeoJsonComponent::addMetaPropertiesToFeature(SubFeatureProps& feature, int index,
                                                  const geos::geom::Geometry* geometry)
{
    std::unique_ptr<geos::geom::Point> centroid = geometry->getCentroid();
    const geos::geom::CoordinateXY centroidCoord = *centroid->getCoordinate();
    const glm::vec2 centroidLatLong = glm::vec2(centroidCoord.y, centroidCoord.x);
    feature.centroidLatLong = centroidLatLong;

    std::unique_ptr<geos::geom::Geometry> boundingbox = geometry->getEnvelope();
    std::unique_ptr<geos::geom::CoordinateSequence> coords =
        boundingbox->getCoordinates();
    glm::vec4 boundingboxLatLong;
    if (boundingbox->isRectangle()) {
        // A rectangle has 5 coordinates, where the first and third are two corners
        boundingboxLatLong = glm::vec4(
            (*coords)[0].y,
            (*coords)[0].x,
            (*coords)[2].y,
            (*coords)[2].x
        );
    }
    else {
        // Invalid boundingbox. Can happen e.g. for single points. Just add a degree to
        // every direction from the centroid
        boundingboxLatLong = glm::vec4(
            centroidLatLong.x - 1.f,
            centroidLatLong.y - 1.f,
            centroidLatLong.x + 1.f,
            centroidLatLong.y + 1.f
        );
    }

    feature.boundingboxLatLong = boundingboxLatLong;
    computeFeatureDiagonal(feature);

    feature.flyToFeature.onChange([this, index]() { flyToFeature(index); });
}

void GeoJsonComponent::computeFeatureDiagonal(SubFeatureProps& feature) const {
    const glm::vec4 boundingboxLatLong = feature.boundingboxLatLong;

    // Compute the diagonal distance of the bounding box
    const Geodetic2 pos0 = {
        glm::radians(boundingboxLatLong.x),
        glm::radians(boundingboxLatLong.y)
    };

    const Geodetic2 pos1 = {
        glm::radians(boundingboxLatLong.z),
        glm::radians(boundingboxLatLong.w)
    };
    feature.boundingBoxDiagonal = static_cast<float>(
        std::abs(_globeNode.ellipsoid().greatCircleDistance(pos0, pos1))
    );
}

void GeoJsonComponent::computeMainFeatureMetaPropeties() {
    if (_features.empty()) {
        return;
    }

    glm::vec2 bboxLowerCorner = glm::vec2(
        _features.front()->boundingboxLatLong.value().x,
        _features.front()->boundingboxLatLong.value().y
    );
    glm::vec2 bboxUpperCorner = glm::vec2(
        _features.front()->boundingboxLatLong.value().z,
        _features.front()->boundingboxLatLong.value().w
    );

    for (const std::unique_ptr<SubFeatureProps>& f : _features) {
        // Update bbox corners
        if (f->boundingboxLatLong.value().x < bboxLowerCorner.x) {
            bboxLowerCorner.x = f->boundingboxLatLong.value().x;
        }
        if (f->boundingboxLatLong.value().y < bboxLowerCorner.y) {
            bboxLowerCorner.y = f->boundingboxLatLong.value().y;
        }
        if (f->boundingboxLatLong.value().z > bboxUpperCorner.x) {
            bboxUpperCorner.x = f->boundingboxLatLong.value().z;
        }
        if (f->boundingboxLatLong.value().w > bboxUpperCorner.y) {
            bboxUpperCorner.y = f->boundingboxLatLong.value().w;
        }
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
        const SubFeatureProps* f = _features[*index].get();
        diagonal = f->boundingBoxDiagonal;
        centroidLat = f->centroidLatLong.value().x;
        centroidLon = f->centroidLatLong.value().y;
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
