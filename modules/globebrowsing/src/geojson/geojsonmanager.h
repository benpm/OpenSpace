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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONMANAGER___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONMANAGER___H__

#include <openspace/properties/propertyowner.h>

#include <modules/globebrowsing/src/geojson/geojsoncomponent.h>
#include <openspace/rendering/multidrawbatch.h>
#include <ghoul/opengl/bufferbinding.h>
#include <chrono>
#include <map>
#include <memory>

namespace ghoul { class Dictionary; }
namespace ghoul::opengl {
    class ProgramObject;
    class Texture;
} // namespace ghoul::opengl

namespace openspace {

struct Documentation;
struct RenderData;
class RenderableGlobe;

/**
 * Manages multiple GeoJSON objects of a globe.
 */
class GeoJsonManager : public PropertyOwner {
public:
    GeoJsonManager();

    void initialize(RenderableGlobe* globe);
    void deinitializeGL();

    bool isReady() const;

    void addGeoJsonLayer(const ghoul::Dictionary& layerDict);
    void deleteLayer(const std::string& layerIdentifier);

    void update();
    void render(const RenderData& data);

private:
    using SsboBinding = ghoul::opengl::BufferBinding<
        ghoul::opengl::bufferbinding::Buffer::ShaderStorage
    >;

    /// Create the shared shader programs and batches. Called lazily when the first
    /// layer is added, since that is when a GL context is guaranteed
    void initializeGLResources();

    void renderBatchedLines(const RenderData& data);
    void renderBatchedPoints(const RenderData& data);

    std::vector<std::unique_ptr<GeoJsonComponent>> _geoJsonObjects;
    RenderableGlobe* _parentGlobe = nullptr;

    // All points and lines of all components on the globe are batched into these and
    // rendered with a few multidraw calls, with per-draw data in an SSBO. Polygons are
    // still rendered per feature, by the components
    rendering::MultiDrawBatch _pointsBatch;
    rendering::MultiDrawBatch _linesBatch;
    std::unique_ptr<SsboBinding> _pointsSsboBinding;
    std::unique_ptr<SsboBinding> _linesSsboBinding;

    std::unique_ptr<ghoul::opengl::ProgramObject> _polygonsProgram;
    std::unique_ptr<ghoul::opengl::ProgramObject> _linesProgram;
    std::unique_ptr<ghoul::opengl::ProgramObject> _pointsProgram;

    /// Textures used by this frame's point draws, keyed by draw group key
    std::map<int64_t, ghoul::opengl::Texture*> _pointTextures;

    /// Forces a rebuild of the emitted draw lists (set when batch geometry changes)
    bool _emitIsDirty = true;

    // Rolling performance counters, logged periodically at debug level
    std::chrono::steady_clock::duration _perfFrameInterval{};
    std::chrono::steady_clock::duration _perfEmitTime{};
    std::chrono::steady_clock::duration _perfRenderTime{};
    std::chrono::steady_clock::duration _perfUpdateTime{};
    std::chrono::steady_clock::time_point _perfLastRender{};
    std::chrono::steady_clock::time_point _perfLastLog{};
    int _perfFrameCount = 0;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONMANAGER___H__
