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

#ifndef __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCULLING___H__
#define __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCULLING___H__

// Conservative per-feature frustum and horizon culling for the batched GeoJson
// rendering. Every test is performed in globe model space against a precomputed
// bounding sphere per feature. The frustum planes are widened by a guard band and every
// test adds a slack distance covering the camera moving within a small ball, so a cull
// result stays valid until the camera exits that ball, which lets the emit pass stay
// skipped on frames with little camera motion

#include <ghoul/glm.h>
#include <array>

namespace openspace { struct RenderData; }

namespace openspace::geojson {

/// Fraction by which the frustum's half-extents are widened for the guard band
constexpr double GuardBandScale = 0.15;

/// Lower bound for the camera altitude used when scaling the translation threshold,
/// so the threshold never collapses to zero at the surface (meters)
constexpr double MinCullAltitude = 100.0;

/// Extra padding added to the height slack, so that heights refined by the height map
/// sweep rarely exceed the slack captured at the last emit (meters)
constexpr double HeightSlackPadding = 500.0;

/// Padding added to every bounding sphere radius; also covers the float precision of
/// the accumulated vertex positions at Earth-radius magnitudes (meters)
constexpr double SphereEpsilon = 1.0;

/**
 * Bounding sphere of one geometry feature, in globe model space, covering all of its
 * built render features (including extrusion walls when built).
 */
struct FeatureCullSphere {
    glm::dvec3 center = glm::dvec3(0.0);
    /// A negative radius means no sphere is available and the feature is never culled
    double radius = -1.0;
    /// dot(center, center), precomputed for the horizon test
    double distSq = 0.0;
    /// Point features get extra slack for their world-sized billboard expansion
    bool isPoints = false;
};

/**
 * Camera-derived state for one cull pass, built once per emit by the manager and
 * shared by all components.
 */
struct GeoJsonCullContext {
    bool frustumEnabled = false;
    bool horizonEnabled = false;
    /// Left, right, bottom, top and near planes in model space, xyz normalized and
    /// pointing into the frustum, widened by the guard band
    std::array<glm::dvec4, 5> frustumPlanes;
    /// Camera position in globe model space
    glm::dvec3 cameraPosModel = glm::dvec3(0.0);
    /// dot(cameraPosModel, cameraPosModel)
    double cameraDistSq = 0.0;
    /// Camera view direction in globe model space, normalized
    glm::dvec3 viewDirModel = glm::dvec3(0.0);
    /// The globe ellipsoid's minimum radius (the horizon occluder sphere)
    double minimumRadius = 0.0;
    /// Camera rotation (radians) covered by the frustum guard band; rotating less
    /// than this keeps the last cull result valid
    double rotThreshold = 0.0;
    /// Camera translation covered by the cull slack; moving less than this keeps the
    /// last cull result valid (meters)
    double posThreshold = 0.0;
};

/**
 * Builds the cull context for the current frame. The frustum planes are derived from
 * the camera's projection tangents, widened by `GuardBandScale` on each side, and
 * transformed into globe model space.
 */
GeoJsonCullContext buildCullContext(const RenderData& data,
    const glm::dmat4& modelTransform, const glm::dmat4& inverseModelTransform,
    double minimumRadius, bool frustumEnabled, bool horizonEnabled);

/**
 * Returns true when the sphere, grown by `slack`, is entirely outside one of the
 * widened frustum planes and therefore cannot be visible.
 */
bool isSphereOutsideFrustum(const FeatureCullSphere& sphere,
    const GeoJsonCullContext& context, double slack);

/**
 * Returns true when the sphere, grown by `slack`, is entirely occluded by the globe.
 * The occluder is the ellipsoid's minimum-radius sphere shrunk by the grown feature
 * radius: if the segment from the camera to the sphere center passes the shrunken
 * occluder, every segment to any real feature point passes the full occluder.
 */
bool isSphereBeyondHorizon(const FeatureCullSphere& sphere,
    const GeoJsonCullContext& context, double slack);

} // namespace openspace::geojson

#endif // __OPENSPACE_MODULE_GLOBEBROWSING___GEOJSONCULLING___H__
