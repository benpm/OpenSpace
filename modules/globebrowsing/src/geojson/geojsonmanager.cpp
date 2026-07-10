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

#include <modules/globebrowsing/src/geojson/geojsonmanager.h>

#include <modules/globebrowsing/globebrowsingmodule.h>
#include <modules/globebrowsing/src/renderableglobe.h>
#include <openspace/documentation/documentation.h>
#include <openspace/engine/globals.h>
#include <openspace/rendering/helper.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/util/updatestructures.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/misc/assert.h>
#include <ghoul/misc/dictionary.h>
#include <ghoul/misc/exception.h>
#include <ghoul/misc/profiling.h>
#include <ghoul/opengl/openglstatecache.h>
#include <ghoul/opengl/programobject.h>
#include <ghoul/opengl/texture.h>
#include <ghoul/opengl/textureunit.h>
#include <algorithm>
#include <functional>
#include <utility>

namespace {
    constexpr std::string_view _loggerCat = "GeoJsonManager";
} // namespace

namespace openspace {

GeoJsonManager::GeoJsonManager()
    : PropertyOwner({ "GeographicOverlays", "Geographic Overlays" })
{}

void GeoJsonManager::initialize(RenderableGlobe* globe) {
    ghoul_assert(globe, "No globe provided");
    _parentGlobe = globe;
}

void GeoJsonManager::deinitializeGL() {
    for (const std::unique_ptr<GeoJsonComponent>& g : _geoJsonObjects) {
        g->deinitializeGL();
    }

    _pointsBatch.deinitialize();
    _linesBatch.deinitialize();
    _pointsSsboBinding = nullptr;
    _linesSsboBinding = nullptr;

    if (_polygonsProgram) {
        global::renderEngine->removeRenderProgram(_polygonsProgram.get());
        _polygonsProgram = nullptr;
    }
    if (_linesProgram) {
        global::renderEngine->removeRenderProgram(_linesProgram.get());
        _linesProgram = nullptr;
    }
    if (_pointsProgram) {
        global::renderEngine->removeRenderProgram(_pointsProgram.get());
        _pointsProgram = nullptr;
    }
}

bool GeoJsonManager::isReady() const {
    return std::all_of(
        _geoJsonObjects.cbegin(),
        _geoJsonObjects.cend(),
        std::mem_fn(&GeoJsonComponent::isReady)
    );
}

void GeoJsonManager::addGeoJsonLayer(const ghoul::Dictionary& layerDict) {
    ZoneScoped;

    try {
        const std::string identifier = layerDict.value<std::string>("Identifier");
        if (hasPropertySubOwner(identifier)) {
            LERROR(std::format(
                "GeoJson layer with identifier '{}' already exists", identifier
            ));
            return;
        }

        initializeGLResources();

        // TODO: use owner instead of explicit parent?
        std::unique_ptr<GeoJsonComponent> geo =
            std::make_unique<GeoJsonComponent>(layerDict, *_parentGlobe);

        geo->initializeGL(_polygonsProgram.get(), &_pointsBatch, &_linesBatch);

        GeoJsonComponent* ptr = geo.get();
        _geoJsonObjects.push_back(std::move(geo));
        addPropertySubOwner(ptr);
    }
    catch (const SpecificationError& e) {
        logError(e);
    }
    catch (const ghoul::RuntimeError& e) {
        LERRORC(e.component, e.message);
    }
}

void GeoJsonManager::deleteLayer(const std::string& layerIdentifier) {
    ZoneScoped;

    for (auto it = _geoJsonObjects.begin(); it != _geoJsonObjects.end(); it++) {
        if (it->get()->identifier() == layerIdentifier) {
            LINFO(std::format("Deleting GeoJson layer: {}", layerIdentifier));
            removePropertySubOwner(it->get());
            (*it)->deinitializeGL();
            _geoJsonObjects.erase(it);

            // The deleted component's draws were removed from the batches
            if (_pointsBatch.isInitialized()) {
                _pointsBatch.commit();
                _linesBatch.commit();
            }
            return;
        }
    }
    LWARNING(std::format("Could not find GeoJson layer {}", layerIdentifier));
}

void GeoJsonManager::update() {
    ZoneScoped;

    bool geometryChanged = false;
    for (std::unique_ptr<GeoJsonComponent>& obj : _geoJsonObjects) {
        if (obj->enabled()) {
            geometryChanged |= obj->update();
        }
    }

    if (geometryChanged) {
        _pointsBatch.commit();
        _linesBatch.commit();
    }
}

void GeoJsonManager::render(const RenderData& data) {
    ZoneScoped;

    if (_geoJsonObjects.empty()) {
        return;
    }

    // Collect this frame's points and lines draws from all components
    _pointsBatch.beginFrame();
    _linesBatch.beginFrame();
    _pointTextures.clear();
    for (std::unique_ptr<GeoJsonComponent>& obj : _geoJsonObjects) {
        if (obj->enabled()) {
            obj->emitBatchedDraws(_pointTextures);
        }
    }
    _pointsBatch.endFrame();
    _linesBatch.endFrame();

    if (_pointsBatch.nDrawsThisFrame() > 0 || _linesBatch.nDrawsThisFrame() > 0) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        renderBatchedLines(data);
        renderBatchedPoints(data);

        glBindVertexArray(0);
        global::renderEngine->openglStateCache().resetPolygonAndClippingState();
        global::renderEngine->openglStateCache().resetBlendState();
        global::renderEngine->openglStateCache().resetDepthState();
    }

    // Polygons are still rendered per component
    for (std::unique_ptr<GeoJsonComponent>& obj : _geoJsonObjects) {
        if (obj->enabled()) {
            obj->render(data);
        }
    }
}

void GeoJsonManager::initializeGLResources() {
    if (_pointsBatch.isInitialized()) {
        return;
    }

    _polygonsProgram = global::renderEngine->buildRenderProgram(
        "GeoPolygonsProgram",
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_vs.glsl"),
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_fs.glsl")
    );

    _linesProgram = global::renderEngine->buildRenderProgram(
        "GeoLinesProgram",
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_lines_vs.glsl"),
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_lines_fs.glsl"),
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_lines_gs.glsl")
    );

    _pointsProgram = global::renderEngine->buildRenderProgram(
        "GeoPointsProgram",
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_points_vs.glsl"),
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_points_fs.glsl"),
        absPath("${MODULE_GLOBEBROWSING}/shaders/geojson_points_gs.glsl")
    );

    // Both batches use the vertex layout of GlobeGeometryFeature::Vertex + a separate,
    // dynamically updated height stream
    using Vertex = GlobeGeometryFeature::Vertex;
    const std::vector<rendering::MultiDrawBatch::StreamSpec> streams = {
        { .stride = sizeof(Vertex), .usage = GL_STATIC_DRAW },
        { .stride = sizeof(float), .usage = GL_DYNAMIC_DRAW }
    };
    const std::vector<rendering::MultiDrawBatch::AttributeSpec> attributes = {
        { .location = 0, .nComponents = 3, .type = GL_FLOAT, .streamIndex = 0,
          .offsetInStream = 0 },
        { .location = 1, .nComponents = 3, .type = GL_FLOAT, .streamIndex = 0,
          .offsetInStream = 3 * sizeof(float) },
        { .location = 2, .nComponents = 1, .type = GL_FLOAT, .streamIndex = 1,
          .offsetInStream = 0 }
    };
    _pointsBatch.initialize(streams, attributes, sizeof(GeoJsonDrawRecord));
    _linesBatch.initialize(streams, attributes, sizeof(GeoJsonDrawRecord));

    _pointsSsboBinding = std::make_unique<SsboBinding>();
    _linesSsboBinding = std::make_unique<SsboBinding>();
}

void GeoJsonManager::renderBatchedLines(const RenderData& data) {
    const std::span<const rendering::MultiDrawBatch::Group> groups = _linesBatch.groups();
    if (groups.empty()) {
        return;
    }

    _linesProgram->activate();

    _linesProgram->setUniform("modelTransform", _parentGlobe->modelTransform());
    _linesProgram->setUniform("viewTransform", data.camera.combinedViewMatrix());
    _linesProgram->setUniform(
        "projectionTransform",
        glm::dmat4(data.camera.projectionMatrix())
    );
    _linesProgram->setUniform(
        "resolution",
        glm::vec2(global::renderEngine->renderingResolution())
    );

    // Re-applied every frame, since the block binding is lost on shader hot-reload
    glBindBufferBase(
        GL_SHADER_STORAGE_BUFFER,
        _linesSsboBinding->bindingNumber(),
        _linesBatch.ssboId()
    );
    _linesProgram->setSsboBinding("DrawData", _linesSsboBinding->bindingNumber());

    _linesBatch.bindVertexArray();
    for (const rendering::MultiDrawBatch::Group& g : groups) {
        const bool wireframe = (g.groupKey & GeoJsonDrawRecord::WireframeGroupBit) != 0;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

        _linesProgram->setUniform("baseDrawId", g.baseDrawId);
        _linesBatch.renderGroup(GL_LINE_STRIP, g);
    }

    _linesProgram->deactivate();
}

void GeoJsonManager::renderBatchedPoints(const RenderData& data) {
    const std::span<const rendering::MultiDrawBatch::Group> groups =
        _pointsBatch.groups();
    if (groups.empty()) {
        return;
    }

    _pointsProgram->activate();

    _pointsProgram->setUniform("modelTransform", _parentGlobe->modelTransform());
    _pointsProgram->setUniform("viewTransform", data.camera.combinedViewMatrix());
    _pointsProgram->setUniform(
        "projectionTransform",
        glm::dmat4(data.camera.projectionMatrix())
    );

    // Points are rendered as billboards
    const glm::dvec3 cameraViewDirWorld = -data.camera.viewDirectionWorldSpace();
    const glm::dvec3 cameraUpDirWorld = data.camera.lookUpVectorWorldSpace();
    glm::dvec3 orthoRight = glm::normalize(
        glm::cross(cameraUpDirWorld, cameraViewDirWorld)
    );
    if (orthoRight == glm::dvec3(0.0)) {
        // For some reason, the up vector and camera view vector were the same. Use a
        // slightly different vector
        const glm::dvec3 otherVector = glm::vec3(
            cameraUpDirWorld.y,
            cameraUpDirWorld.x,
            cameraUpDirWorld.z
        );
        orthoRight = glm::normalize(glm::cross(otherVector, cameraViewDirWorld));
    }
    const glm::dvec3 orthoUp = glm::normalize(glm::cross(cameraViewDirWorld, orthoRight));

    _pointsProgram->setUniform("cameraUp", glm::vec3(orthoUp));
    _pointsProgram->setUniform("cameraRight", glm::vec3(orthoRight));
    _pointsProgram->setUniform("cameraPosition", data.camera.position());
    _pointsProgram->setUniform("cameraLookUp", glm::vec3(cameraUpDirWorld));

    // Re-applied every frame, since the block binding is lost on shader hot-reload
    glBindBufferBase(
        GL_SHADER_STORAGE_BUFFER,
        _pointsSsboBinding->bindingNumber(),
        _pointsBatch.ssboId()
    );
    _pointsProgram->setSsboBinding("DrawData", _pointsSsboBinding->bindingNumber());

    _pointsBatch.bindVertexArray();
    for (const rendering::MultiDrawBatch::Group& g : groups) {
        const bool wireframe = (g.groupKey & GeoJsonDrawRecord::WireframeGroupBit) != 0;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

        ghoul::opengl::TextureUnit unit;
        ghoul::opengl::Texture* texture = _pointTextures[g.groupKey];
        if (texture) {
            unit.bind(*texture);
            _pointsProgram->setUniform("pointTexture", unit);
        }

        _pointsProgram->setUniform("baseDrawId", g.baseDrawId);
        _pointsBatch.renderGroup(GL_POINTS, g);
    }

    _pointsProgram->deactivate();
}

} // namespace openspace
