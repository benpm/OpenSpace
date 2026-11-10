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

#include <openspace/rendering/postprocessor.h>

#include <openspace/filesystem/filesystem.h>
#include <openspace/logging/logmanager.h>
#include <openspace/misc/profiling.h>
#include <openspace/opengl/programobject.h>
#include <openspace/opengl/textureunit.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <string>

namespace {
    using namespace openspace;

    constexpr std::string_view _loggerCat = "PostProcessor";

    // Mip levels of the half-resolution bright-pass texture that the blur can accumulate
    constexpr int MaxBlurLevels = 8;

    constexpr BoolPropertyInfo EnabledInfo = {
        .identifier = "Enabled",
        .name = "Enabled",
        .description = "Enables the post-processing effects that are applied to the "
            "finished frame of every window. The individual effects have to be enabled "
            "separately as well.",
        .visibility = Visibility::User,
        .defaults = {
            .value = false
        }
    };

    constexpr FloatPropertyInfo GammaInfo = {
        .identifier = "Gamma",
        .name = "Gamma",
        .description = "The gamma that the rendered frame is encoded with. The effects "
            "operate on linear color values, so the frame is linearized with this gamma "
            "first and encoded with it again at the end.",
        .visibility = Visibility::AdvancedUser,
        .defaults = {
            .value = 2.2f,
            .minMax = { 1.f, 3.f }
        }
    };

    constexpr BoolPropertyInfo BloomEnabledInfo = {
        .identifier = "Enabled",
        .name = "Enabled",
        .description = "Enables the bloom effect, which makes bright areas of the frame "
            "glow into their surroundings.",
        .visibility = Visibility::User,
        .defaults = {
            .value = false
        }
    };

    constexpr FloatPropertyInfo BloomThresholdInfo = {
        .identifier = "Threshold",
        .name = "Threshold",
        .description = "The luminance (of the linearized frame) above which a pixel "
            "contributes to the bloom.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.8f,
            .minMax = { 0.f, 2.f }
        }
    };

    constexpr FloatPropertyInfo BloomSoftThresholdInfo = {
        .identifier = "SoftThreshold",
        .name = "Soft threshold",
        .description = "The width of the transition around the threshold. A larger value "
            "lets pixels slightly below the threshold contribute partially, which avoids "
            "flickering.",
        .visibility = Visibility::AdvancedUser,
        .defaults = {
            .value = 0.3f,
            .minMax = { 0.f, 1.f }
        }
    };

    constexpr FloatPropertyInfo BloomIntensityInfo = {
        .identifier = "Intensity",
        .name = "Intensity",
        .description = "The strength with which the bloom is added to the frame.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.3f,
            .minMax = { 0.f, 3.f }
        }
    };

    constexpr FloatPropertyInfo BloomRadiusInfo = {
        .identifier = "Radius",
        .name = "Radius",
        .description = "Controls how far the glow spreads. The bloom accumulates "
            "successively smaller mip levels of the thresholded frame; this value sets "
            "the width of the Gaussian weights over those levels.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.5f,
            .minMax = { 0.f, 1.f }
        }
    };

    constexpr BoolPropertyInfo ToneMappingEnabledInfo = {
        .identifier = "Enabled",
        .name = "Enabled",
        .description = "Enables the tone mapping, which applies an exposure and a tone "
            "curve to the linearized frame. The frame has already been tone mapped by "
            "the renderer, so this acts as an additional look.",
        .visibility = Visibility::User,
        .defaults = {
            .value = false
        }
    };

    constexpr OptionPropertyInfo ToneMappingOperatorInfo = {
        .identifier = "Operator",
        .name = "Operator",
        .description = "The tone curve that is applied after the exposure.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 3 // ACES, see ToneMappingOperator
        }
    };

    constexpr FloatPropertyInfo ToneMappingExposureInfo = {
        .identifier = "Exposure",
        .name = "Exposure",
        .description = "The factor that the linearized frame is multiplied with before "
            "the tone curve.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 1.f,
            .minMax = { 0.f, 8.f }
        }
    };

    constexpr FloatPropertyInfo ToneMappingWhitePointInfo = {
        .identifier = "WhitePoint",
        .name = "White point",
        .description = "The linear value that is mapped to white by the Reinhard "
            "(extended) operator. It is ignored by the other operators.",
        .visibility = Visibility::AdvancedUser,
        .defaults = {
            .value = 4.f,
            .minMax = { 1.f, 16.f }
        }
    };

    constexpr BoolPropertyInfo VignetteEnabledInfo = {
        .identifier = "Enabled",
        .name = "Enabled",
        .description = "Enables the vignette, which darkens the frame towards its "
            "corners.",
        .visibility = Visibility::User,
        .defaults = {
            .value = false
        }
    };

    constexpr FloatPropertyInfo VignetteIntensityInfo = {
        .identifier = "Intensity",
        .name = "Intensity",
        .description = "How dark the corners of the frame become. 0 leaves the frame "
            "unchanged and 1 darkens the corners completely.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.5f,
            .minMax = { 0.f, 1.f }
        }
    };

    constexpr FloatPropertyInfo VignetteFalloffInfo = {
        .identifier = "Falloff",
        .name = "Falloff",
        .description = "How quickly the darkening increases towards the corners. Larger "
            "values keep more of the center untouched.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.5f,
            .minMax = { 0.05f, 2.f }
        }
    };

    constexpr BoolPropertyInfo SharpeningEnabledInfo = {
        .identifier = "Enabled",
        .name = "Enabled",
        .description = "Enables the contrast-adaptive sharpening, which enhances edges "
            "without amplifying noise in flat areas or creating halos around already "
            "sharp edges.",
        .visibility = Visibility::User,
        .defaults = {
            .value = false
        }
    };

    constexpr FloatPropertyInfo SharpeningAmountInfo = {
        .identifier = "Amount",
        .name = "Amount",
        .description = "The strength of the sharpening, where 0 is the mildest and 1 the "
            "strongest setting.",
        .visibility = Visibility::User,
        .defaults = {
            .value = 0.5f,
            .minMax = { 0.f, 1.f }
        }
    };

    // The values have to match the switch in composite_fs.glsl
    enum class ToneMappingOperator {
        Clamp = 0,
        Reinhard,
        ReinhardExtended,
        Aces,
        Filmic
    };

    void checkFramebuffer(GLuint framebuffer, const std::string& label) {
        const GLenum status = glCheckNamedFramebufferStatus(framebuffer, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LERROR(std::format(
                "Framebuffer '{}' is incomplete: {:#x}",
                label, static_cast<unsigned int>(status)
            ));
        }
    }
} // namespace

namespace openspace {

PostProcessor::PostProcessor()
    : PropertyOwner({
        "PostProcessing",
        "Post-Processing",
        "Screen-space effects applied to the finished frame of every window"
    })
    , _enabled(EnabledInfo)
    , _gamma(GammaInfo)
    , _bloom {
        .owner = PropertyOwner({ "Bloom", "Bloom", "Glow around bright areas" }),
        .enabled = BoolProperty(BloomEnabledInfo),
        .threshold = FloatProperty(BloomThresholdInfo),
        .softThreshold = FloatProperty(BloomSoftThresholdInfo),
        .intensity = FloatProperty(BloomIntensityInfo),
        .radius = FloatProperty(BloomRadiusInfo)
    }
    , _toneMapping {
        .owner = PropertyOwner(
            { "ToneMapping", "Tone Mapping", "Exposure and tone curve" }
        ),
        .enabled = BoolProperty(ToneMappingEnabledInfo),
        .op = OptionProperty(ToneMappingOperatorInfo),
        .exposure = FloatProperty(ToneMappingExposureInfo),
        .whitePoint = FloatProperty(ToneMappingWhitePointInfo)
    }
    , _vignette {
        .owner = PropertyOwner({ "Vignette", "Vignette", "Darkened corners" }),
        .enabled = BoolProperty(VignetteEnabledInfo),
        .intensity = FloatProperty(VignetteIntensityInfo),
        .falloff = FloatProperty(VignetteFalloffInfo)
    }
    , _sharpening {
        .owner = PropertyOwner({ "Sharpening", "Sharpening", "Edge sharpening" }),
        .enabled = BoolProperty(SharpeningEnabledInfo),
        .amount = FloatProperty(SharpeningAmountInfo)
    }
{
    addProperty(_enabled);
    addProperty(_gamma);

    _bloom.owner.addProperty(_bloom.enabled);
    _bloom.owner.addProperty(_bloom.threshold);
    _bloom.owner.addProperty(_bloom.softThreshold);
    _bloom.owner.addProperty(_bloom.intensity);
    _bloom.owner.addProperty(_bloom.radius);
    addPropertySubOwner(_bloom.owner);

    _toneMapping.owner.addProperty(_toneMapping.enabled);
    _toneMapping.op.addOptions({
        { static_cast<int>(ToneMappingOperator::Clamp), "Clamp" },
        { static_cast<int>(ToneMappingOperator::Reinhard), "Reinhard" },
        {
            static_cast<int>(ToneMappingOperator::ReinhardExtended),
            "Reinhard (extended)"
        },
        { static_cast<int>(ToneMappingOperator::Aces), "ACES" },
        { static_cast<int>(ToneMappingOperator::Filmic), "Filmic (Uncharted 2)" }
    });
    // addOptions selects the first option, so the default has to be applied afterwards
    _toneMapping.op = static_cast<int>(ToneMappingOperator::Aces);
    _toneMapping.owner.addProperty(_toneMapping.op);
    _toneMapping.owner.addProperty(_toneMapping.exposure);
    _toneMapping.owner.addProperty(_toneMapping.whitePoint);
    addPropertySubOwner(_toneMapping.owner);

    _vignette.owner.addProperty(_vignette.enabled);
    _vignette.owner.addProperty(_vignette.intensity);
    _vignette.owner.addProperty(_vignette.falloff);
    addPropertySubOwner(_vignette.owner);

    _sharpening.owner.addProperty(_sharpening.enabled);
    _sharpening.owner.addProperty(_sharpening.amount);
    addPropertySubOwner(_sharpening.owner);
}

bool PostProcessor::isBloomEnabled() const {
    return _enabled && _bloom.enabled;
}

bool PostProcessor::isToneMappingEnabled() const {
    return _enabled && _toneMapping.enabled;
}

bool PostProcessor::isVignetteEnabled() const {
    return _enabled && _vignette.enabled;
}

bool PostProcessor::isSharpeningEnabled() const {
    return _enabled && _sharpening.enabled;
}

void PostProcessor::initializeGL() {
    ZoneScoped;

    // A full-screen triangle pair; the vertex shader derives the texture coordinates
    constexpr std::array<glm::vec2, 6> VertexData = {
        glm::vec2(-1.f, -1.f), glm::vec2(1.f, 1.f), glm::vec2(-1.f, 1.f),
        glm::vec2(-1.f, -1.f), glm::vec2(1.f, -1.f), glm::vec2(1.f, 1.f)
    };
    glCreateBuffers(1, &_vbo);
    glNamedBufferStorage(
        _vbo,
        VertexData.size() * sizeof(glm::vec2),
        VertexData.data(),
        GL_NONE_BIT
    );
    glCreateVertexArrays(1, &_vao);
    glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(glm::vec2));
    glEnableVertexArrayAttrib(_vao, 0);
    glVertexArrayAttribFormat(_vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(_vao, 0, 0);
    glObjectLabel(GL_VERTEX_ARRAY, _vao, -1, "PostProcessor quad");

    const std::filesystem::path vs =
        absPath("${SHADERS}/postprocess/postprocess_vs.glsl");
    _brightPassProgram = opengl::ProgramObject::Build(
        "PostProcess Bloom Bright Pass",
        vs,
        absPath("${SHADERS}/postprocess/bloom_brightpass_fs.glsl")
    );
    _blurProgram = opengl::ProgramObject::Build(
        "PostProcess Bloom Blur",
        vs,
        absPath("${SHADERS}/postprocess/bloom_blur_fs.glsl")
    );
    _upsampleProgram = opengl::ProgramObject::Build(
        "PostProcess Bloom Upsample",
        vs,
        absPath("${SHADERS}/postprocess/bloom_upsample_fs.glsl")
    );
    _compositeProgram = opengl::ProgramObject::Build(
        "PostProcess Composite",
        vs,
        absPath("${SHADERS}/postprocess/composite_fs.glsl")
    );
    _sharpenProgram = opengl::ProgramObject::Build(
        "PostProcess Sharpen",
        vs,
        absPath("${SHADERS}/postprocess/sharpen_fs.glsl")
    );
    opengl::updateUniformLocations(*_brightPassProgram, _brightPassCache);
    opengl::updateUniformLocations(*_blurProgram, _blurCache);
    opengl::updateUniformLocations(*_upsampleProgram, _upsampleCache);
    opengl::updateUniformLocations(*_compositeProgram, _compositeCache);
    opengl::updateUniformLocations(*_sharpenProgram, _sharpenCache);

    _isInitialized = true;
    LDEBUG("Initialized post-processing");
}

void PostProcessor::deinitializeGL() {
    if (!_isInitialized) {
        return;
    }

    for (std::pair<const int, WindowResources>& r : _resources) {
        destroyResources(r.second);
    }
    _resources.clear();

    glDeleteVertexArrays(1, &_vao);
    _vao = 0;
    glDeleteBuffers(1, &_vbo);
    _vbo = 0;

    _brightPassProgram = nullptr;
    _blurProgram = nullptr;
    _upsampleProgram = nullptr;
    _compositeProgram = nullptr;
    _sharpenProgram = nullptr;
    _isInitialized = false;
}

void PostProcessor::updatePrograms() {
    // Rebuilding on change allows editing the shader files while running
    if (_brightPassProgram->isDirty()) {
        _brightPassProgram->rebuildFromFile();
        opengl::updateUniformLocations(*_brightPassProgram, _brightPassCache);
    }
    if (_blurProgram->isDirty()) {
        _blurProgram->rebuildFromFile();
        opengl::updateUniformLocations(*_blurProgram, _blurCache);
    }
    if (_upsampleProgram->isDirty()) {
        _upsampleProgram->rebuildFromFile();
        opengl::updateUniformLocations(*_upsampleProgram, _upsampleCache);
    }
    if (_compositeProgram->isDirty()) {
        _compositeProgram->rebuildFromFile();
        opengl::updateUniformLocations(*_compositeProgram, _compositeCache);
    }
    if (_sharpenProgram->isDirty()) {
        _sharpenProgram->rebuildFromFile();
        opengl::updateUniformLocations(*_sharpenProgram, _sharpenCache);
    }
}

void PostProcessor::createTarget(Target& target, glm::ivec2 size, GLenum internalFormat,
                                 bool mipmapped, const std::string& label) const
{
    target.size = glm::max(size, glm::ivec2(1));
    target.levels = 1;
    if (mipmapped) {
        const int maxDim = std::max(target.size.x, target.size.y);
        target.levels = std::min(
            static_cast<int>(std::floor(std::log2(maxDim))) + 1,
            MaxBlurLevels
        );
    }

    glCreateTextures(GL_TEXTURE_2D, 1, &target.texture);
    glTextureStorage2D(
        target.texture,
        target.levels,
        internalFormat,
        target.size.x,
        target.size.y
    );
    glTextureParameteri(
        target.texture,
        GL_TEXTURE_MIN_FILTER,
        mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR
    );
    glTextureParameteri(target.texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(target.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(target.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glObjectLabel(GL_TEXTURE, target.texture, -1, label.c_str());

    glCreateFramebuffers(1, &target.framebuffer);
    glNamedFramebufferTexture(
        target.framebuffer,
        GL_COLOR_ATTACHMENT0,
        target.texture,
        0
    );
    glNamedFramebufferDrawBuffer(target.framebuffer, GL_COLOR_ATTACHMENT0);
    glObjectLabel(GL_FRAMEBUFFER, target.framebuffer, -1, label.c_str());

    checkFramebuffer(target.framebuffer, label);
}

void PostProcessor::destroyTarget(Target& target) const {
    glDeleteFramebuffers(1, &target.framebuffer);
    glDeleteTextures(1, &target.texture);
    target = Target();
}

void PostProcessor::destroyResources(WindowResources& res) const {
    destroyTarget(res.brightPass);
    destroyTarget(res.blur);
    destroyTarget(res.upsample);
    destroyTarget(res.composite);
    res.size = glm::ivec2(0);
}

PostProcessor::WindowResources& PostProcessor::resources(int windowId,
                                                         glm::ivec2 size)
{
    WindowResources& res = _resources[windowId];
    if (res.size != size) {
        destroyResources(res);
        res.size = size;
        const glm::ivec2 half = glm::max(size / 2, glm::ivec2(1));
        createTarget(
            res.brightPass,
            half,
            GL_RGBA16F,
            true,
            std::format("PostProcessor bright pass (window {})", windowId)
        );
        createTarget(
            res.blur,
            half,
            GL_RGBA16F,
            false,
            std::format("PostProcessor blur (window {})", windowId)
        );
        createTarget(
            res.upsample,
            size,
            GL_RGBA16F,
            false,
            std::format("PostProcessor upsample (window {})", windowId)
        );
        createTarget(
            res.composite,
            size,
            GL_RGBA8,
            false,
            std::format("PostProcessor composite (window {})", windowId)
        );
        LDEBUG(std::format(
            "Created post-processing buffers for window {} ({}x{})",
            windowId, size.x, size.y
        ));
    }
    return res;
}

void PostProcessor::drawQuad() const {
    glBindVertexArray(_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PostProcessor::apply(int windowId, GLuint inputTexture, glm::ivec2 size) {
    ZoneScoped;
    TracyGpuZone("PostProcessor");

    if (!_isInitialized) {
        initializeGL();
    }
    updatePrograms();

    // SGCT expects the result in the framebuffer and viewport that are bound now; the
    // bloom passes render into their own targets, so both are saved and restored
    GLint outputFramebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &outputFramebuffer);
    std::array<GLint, 4> outputViewport = {};
    glGetIntegerv(GL_VIEWPORT, outputViewport.data());

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    const bool sharpen = isSharpeningEnabled();
    if (sharpen || isBloomEnabled()) {
        WindowResources& res = resources(windowId, size);

        if (isBloomEnabled()) {
            renderBloom(res, inputTexture);
        }

        if (sharpen) {
            // The sharpening reads the composite, so that one goes into a texture first
            glBindFramebuffer(GL_FRAMEBUFFER, res.composite.framebuffer);
            glViewport(0, 0, size.x, size.y);
            renderComposite(res, inputTexture);

            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(outputFramebuffer));
            glViewport(
                outputViewport[0],
                outputViewport[1],
                outputViewport[2],
                outputViewport[3]
            );
            renderSharpening(res);
        }
        else {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(outputFramebuffer));
            glViewport(
                outputViewport[0],
                outputViewport[1],
                outputViewport[2],
                outputViewport[3]
            );
            renderComposite(res, inputTexture);
        }
    }
    else {
        // Tone mapping and vignette (or nothing at all) only need the composite pass
        const WindowResources empty;
        renderComposite(empty, inputTexture);
    }

    // SGCT runs this callback with depth testing and blending disabled and restores
    // blending itself afterwards, so only the depth mask that was changed here is reset.
    // The OpenGLStateCache is not used as its defaults describe the scene rendering
    glDepthMask(GL_TRUE);
}

void PostProcessor::renderBloom(WindowResources& res, GLuint inputTexture) {
    ZoneScoped;
    TracyGpuZone("Bloom");

    // Pass 1: Extract the bright parts into the half-resolution texture
    glBindFramebuffer(GL_FRAMEBUFFER, res.brightPass.framebuffer);
    glViewport(0, 0, res.brightPass.size.x, res.brightPass.size.y);
    _brightPassProgram->activate();
    opengl::TextureUnit inputUnit;
    inputUnit.bind(inputTexture);
    _brightPassProgram->setUniform(_brightPassCache.inputTexture, inputUnit);
    _brightPassProgram->setUniform(_brightPassCache.threshold, _bloom.threshold.value());
    _brightPassProgram->setUniform(
        _brightPassCache.softThreshold,
        _bloom.softThreshold.value()
    );
    _brightPassProgram->setUniform(_brightPassCache.gamma, _gamma.value());
    drawQuad();
    _brightPassProgram->deactivate();

    // Pass 2: The mip chain provides increasingly blurred versions of the bright parts
    glGenerateTextureMipmap(res.brightPass.texture);

    // Pass 3: Accumulate the mip levels with Gaussian weights
    glBindFramebuffer(GL_FRAMEBUFFER, res.blur.framebuffer);
    glViewport(0, 0, res.blur.size.x, res.blur.size.y);
    _blurProgram->activate();
    opengl::TextureUnit brightUnit;
    brightUnit.bind(res.brightPass.texture);
    _blurProgram->setUniform(_blurCache.brightPassTexture, brightUnit);
    _blurProgram->setUniform(_blurCache.nLevels, res.brightPass.levels);
    // Radius 0 only uses the first level, radius 1 spreads over the whole chain
    const float sigma = 0.3f + _bloom.radius.value() * 3.7f;
    _blurProgram->setUniform(_blurCache.sigma, sigma);
    drawQuad();
    _blurProgram->deactivate();

    // Pass 4: Upsample with a tent filter to hide the half-resolution blockiness
    glBindFramebuffer(GL_FRAMEBUFFER, res.upsample.framebuffer);
    glViewport(0, 0, res.upsample.size.x, res.upsample.size.y);
    _upsampleProgram->activate();
    opengl::TextureUnit blurUnit;
    blurUnit.bind(res.blur.texture);
    _upsampleProgram->setUniform(_upsampleCache.blurTexture, blurUnit);
    drawQuad();
    _upsampleProgram->deactivate();
}

void PostProcessor::renderComposite(const WindowResources& res,
                                    GLuint inputTexture) const
{
    ZoneScoped;
    TracyGpuZone("Composite");

    const bool bloom = isBloomEnabled() && res.upsample.texture != 0;
    const bool toneMapping = isToneMappingEnabled();
    const bool vignette = isVignetteEnabled();

    _compositeProgram->activate();
    opengl::TextureUnit sceneUnit;
    sceneUnit.bind(inputTexture);
    _compositeProgram->setUniform(_compositeCache.sceneTexture, sceneUnit);
    opengl::TextureUnit bloomUnit;
    if (bloom) {
        bloomUnit.bind(res.upsample.texture);
    }
    _compositeProgram->setUniform(_compositeCache.bloomTexture, bloomUnit);
    _compositeProgram->setUniform(
        _compositeCache.passthrough,
        !bloom && !toneMapping && !vignette
    );
    _compositeProgram->setUniform(_compositeCache.gamma, _gamma.value());
    _compositeProgram->setUniform(_compositeCache.applyBloom, bloom);
    _compositeProgram->setUniform(
        _compositeCache.bloomIntensity,
        _bloom.intensity.value()
    );
    _compositeProgram->setUniform(_compositeCache.applyToneMapping, toneMapping);
    _compositeProgram->setUniform(
        _compositeCache.toneMappingOperator,
        _toneMapping.op.value()
    );
    _compositeProgram->setUniform(
        _compositeCache.exposure,
        _toneMapping.exposure.value()
    );
    _compositeProgram->setUniform(
        _compositeCache.whitePoint,
        _toneMapping.whitePoint.value()
    );
    _compositeProgram->setUniform(_compositeCache.applyVignette, vignette);
    _compositeProgram->setUniform(
        _compositeCache.vignetteIntensity,
        _vignette.intensity
    );
    _compositeProgram->setUniform(
        _compositeCache.vignetteFalloff,
        _vignette.falloff.value()
    );
    drawQuad();
    _compositeProgram->deactivate();
}

void PostProcessor::renderSharpening(const WindowResources& res) const {
    ZoneScoped;
    TracyGpuZone("Sharpening");

    _sharpenProgram->activate();
    opengl::TextureUnit inputUnit;
    inputUnit.bind(res.composite.texture);
    _sharpenProgram->setUniform(_sharpenCache.inputTexture, inputUnit);
    _sharpenProgram->setUniform(_sharpenCache.amount, _sharpening.amount.value());
    drawQuad();
    _sharpenProgram->deactivate();
}

} // namespace openspace
