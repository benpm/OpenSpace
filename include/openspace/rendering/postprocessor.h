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

#ifndef __OPENSPACE_CORE___POSTPROCESSOR___H__
#define __OPENSPACE_CORE___POSTPROCESSOR___H__

#include <openspace/properties/propertyowner.h>

#include <openspace/glm.h>
#include <openspace/opengl/gl.h>
#include <openspace/opengl/uniformcache.h>
#include <openspace/properties/misc/optionproperty.h>
#include <openspace/properties/scalar/boolproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <map>
#include <memory>

namespace openspace {

namespace opengl { class ProgramObject; }

/**
 * Screen-space effects that are applied to the finished frame of every window and eye
 * through SGCT's post-process callback (Engine::Callbacks::postProcess). SGCT hands over
 * the texture holding the rendered frame and has already bound the framebuffer that the
 * result has to be drawn into. The effects run in this order: bloom, tone mapping,
 * vignette, and sharpening. All of them are controlled through properties and are
 * disabled by default, in which case the frame is copied through unchanged.
 *
 * The input is the display-referred image that the FramebufferRenderer has already
 * tone mapped and gamma encoded, so the effects linearize it with the configured gamma
 * before operating on it and encode the result again.
 */
class PostProcessor final : public PropertyOwner {
public:
    PostProcessor();
    ~PostProcessor() override = default;

    /**
     * Applies the enabled effects to \p inputTexture and draws the result into the
     * framebuffer that is bound when this function is called. The framebuffer binding
     * and the viewport are restored before returning.
     *
     * \param windowId The SGCT id of the window that is being rendered. It selects the
     *        intermediate buffers, as windows can have different sizes
     * \param inputTexture The 2D texture holding the rendered frame
     * \param size The size of \p inputTexture in pixels
     * \param eyeLayout The number of eye images next to each other horizontally and
     *        vertically in \p inputTexture. Side-by-side stereo uses (2, 1), so that
     *        effects such as the vignette are applied to each eye separately
     */
    void apply(int windowId, GLuint inputTexture, glm::ivec2 size,
        glm::ivec2 eyeLayout = glm::ivec2(1, 1));

    /**
     * Releases all OpenGL resources. Has to be called with a valid context.
     */
    void deinitializeGL();

private:
    struct Target {
        GLuint framebuffer = 0;
        GLuint texture = 0;
        glm::ivec2 size = glm::ivec2(0);
        int levels = 1;
    };

    struct WindowResources {
        glm::ivec2 size = glm::ivec2(0);
        Target brightPass;  // Half resolution, mip-mapped, holds the thresholded image
        Target blur;        // Half resolution, the accumulated mip levels
        Target upsample;    // Full resolution, the smoothed bloom
        Target composite;   // Full resolution, only used when sharpening is enabled
    };

    void initializeGL();
    void updatePrograms();
    WindowResources& resources(int windowId, glm::ivec2 size);
    void createTarget(Target& target, glm::ivec2 size, GLenum internalFormat,
        bool mipmapped, const std::string& label) const;
    void destroyTarget(Target& target) const;
    void destroyResources(WindowResources& res) const;
    void drawQuad() const;

    void renderBloom(WindowResources& res, GLuint inputTexture);
    void renderComposite(const WindowResources& res, GLuint inputTexture,
        glm::ivec2 eyeLayout) const;
    void renderSharpening(const WindowResources& res) const;

    bool isBloomEnabled() const;
    bool isToneMappingEnabled() const;
    bool isVignetteEnabled() const;
    bool isSharpeningEnabled() const;

    BoolProperty _enabled;
    FloatProperty _gamma;

    struct Bloom {
        PropertyOwner owner;
        BoolProperty enabled;
        FloatProperty threshold;
        FloatProperty softThreshold;
        FloatProperty intensity;
        FloatProperty radius;
    };
    Bloom _bloom;

    struct ToneMapping {
        PropertyOwner owner;
        BoolProperty enabled;
        OptionProperty op;
        FloatProperty exposure;
        FloatProperty whitePoint;
    };
    ToneMapping _toneMapping;

    struct Vignette {
        PropertyOwner owner;
        BoolProperty enabled;
        FloatProperty intensity;
        FloatProperty falloff;
    };
    Vignette _vignette;

    struct Sharpening {
        PropertyOwner owner;
        BoolProperty enabled;
        FloatProperty amount;
    };
    Sharpening _sharpening;

    bool _isInitialized = false;
    GLuint _vao = 0;
    GLuint _vbo = 0;
    std::map<int, WindowResources> _resources;

    std::unique_ptr<opengl::ProgramObject> _brightPassProgram;
    UniformCache(inputTexture, threshold, softThreshold, gamma) _brightPassCache;

    std::unique_ptr<opengl::ProgramObject> _blurProgram;
    UniformCache(brightPassTexture, nLevels, sigma) _blurCache;

    std::unique_ptr<opengl::ProgramObject> _upsampleProgram;
    UniformCache(blurTexture) _upsampleCache;

    std::unique_ptr<opengl::ProgramObject> _compositeProgram;
    UniformCache(sceneTexture, bloomTexture, passthrough, gamma, applyBloom,
        bloomIntensity, applyToneMapping, toneMappingOperator, exposure, whitePoint,
        applyVignette, vignetteIntensity, vignetteFalloff, eyeLayout) _compositeCache;

    std::unique_ptr<opengl::ProgramObject> _sharpenProgram;
    UniformCache(inputTexture, amount) _sharpenCache;
};

} // namespace openspace

#endif // __OPENSPACE_CORE___POSTPROCESSOR___H__
