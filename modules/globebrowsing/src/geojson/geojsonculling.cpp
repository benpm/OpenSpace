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

#include <modules/globebrowsing/src/geojson/geojsonculling.h>

#include <openspace/camera/camera.h>
#include <openspace/util/updatestructures.h>
#include <algorithm>
#include <cmath>

namespace openspace::geojson {

GeoJsonCullContext buildCullContext(const RenderData& data,
                                    const glm::dmat4& modelTransform,
                                    const glm::dmat4& inverseModelTransform,
                                    double minimumRadius, bool frustumEnabled,
                                    bool horizonEnabled)
{
    GeoJsonCullContext context;
    context.frustumEnabled = frustumEnabled;
    context.horizonEnabled = horizonEnabled;
    context.minimumRadius = minimumRadius;

    context.cameraPosModel = glm::dvec3(
        inverseModelTransform * glm::dvec4(data.camera.position(), 1.0)
    );
    context.cameraDistSq = glm::dot(context.cameraPosModel, context.cameraPosModel);
    context.viewDirModel = glm::normalize(glm::dvec3(
        inverseModelTransform * glm::dvec4(data.camera.viewDirectionWorldSpace(), 0.0)
    ));

    // Half-extent tangents of the perspective frustum at unit distance, from the
    // projection matrix. The frustum may be asymmetric (off-axis viewports), so all
    // four sides are kept separate
    const glm::dmat4 projection = glm::dmat4(data.camera.projectionMatrix());
    const double srx = projection[0][0];
    const double sry = projection[1][1];
    if (srx <= 0.0 || sry <= 0.0) {
        // Not a perspective projection; only the horizon test is usable. The zero
        // thresholds force a fresh cull on any camera motion
        context.frustumEnabled = false;
        return context;
    }
    const double ox = projection[2][0];
    const double oy = projection[2][1];
    const double tanRight = (ox + 1.0) / srx;
    const double tanLeft = (ox - 1.0) / srx;
    const double tanTop = (oy + 1.0) / sry;
    const double tanBottom = (oy - 1.0) / sry;

    // Widen each side outward by GuardBandScale of the half extent
    const double halfX = 0.5 * (tanRight - tanLeft) * GuardBandScale;
    const double halfY = 0.5 * (tanTop - tanBottom) * GuardBandScale;
    const double wideRight = tanRight + halfX;
    const double wideLeft = tanLeft - halfX;
    const double wideTop = tanTop + halfY;
    const double wideBottom = tanBottom - halfY;

    // The smallest angular margin any plane gained from the widening bounds how far
    // the camera may rotate before a stale cull result could become wrong. Halved
    // for margin against the linearized threshold math
    const double margin = std::min(
        std::min(
            std::atan(wideRight) - std::atan(tanRight),
            std::atan(tanLeft) - std::atan(wideLeft)
        ),
        std::min(
            std::atan(wideTop) - std::atan(tanTop),
            std::atan(tanBottom) - std::atan(wideBottom)
        )
    );
    context.rotThreshold = 0.5 * margin;

    const double altitude = std::max(
        glm::length(context.cameraPosModel) - minimumRadius,
        MinCullAltitude
    );
    context.posThreshold = std::tan(context.rotThreshold) * altitude;

    // Inward-pointing planes through the camera position in view space (camera at the
    // origin looking down -z). The near plane is placed at the camera itself, which
    // only makes the test more conservative than the true near plane
    const std::array<glm::dvec4, 5> viewPlanes = {
        glm::dvec4(1.0, 0.0, wideLeft, 0.0),
        glm::dvec4(-1.0, 0.0, -wideRight, 0.0),
        glm::dvec4(0.0, 1.0, wideBottom, 0.0),
        glm::dvec4(0.0, -1.0, -wideTop, 0.0),
        glm::dvec4(0.0, 0.0, -1.0, 0.0)
    };

    // Planes transform model -> view with the transpose of the point transform
    const glm::dmat4 planeTransform =
        glm::transpose(data.camera.combinedViewMatrix() * modelTransform);
    for (size_t i = 0; i < viewPlanes.size(); i++) {
        glm::dvec4 p = planeTransform * viewPlanes[i];
        p /= glm::length(glm::dvec3(p));
        context.frustumPlanes[i] = p;
    }

    return context;
}

bool isSphereOutsideFrustum(const FeatureCullSphere& sphere,
                            const GeoJsonCullContext& context, double slack)
{
    if (sphere.radius < 0.0) {
        return false;
    }

    const double negLimit = -(sphere.radius + slack);
    for (const glm::dvec4& plane : context.frustumPlanes) {
        const double dist =
            plane.x * sphere.center.x + plane.y * sphere.center.y +
            plane.z * sphere.center.z + plane.w;
        if (dist < negLimit) {
            return true;
        }
    }
    return false;
}

bool isSphereBeyondHorizon(const FeatureCullSphere& sphere,
                           const GeoJsonCullContext& context, double slack)
{
    if (sphere.radius < 0.0) {
        return false;
    }

    // Shrink the occluder sphere by the grown feature radius. Every real feature
    // point lies within that distance of the center, so if the segment from the
    // camera to the center intersects the shrunken occluder, every segment to a real
    // point intersects the globe
    const double occluderRadius = context.minimumRadius - (sphere.radius + slack);
    if (occluderRadius <= 0.0) {
        return false;
    }
    const double occluderSq = occluderRadius * occluderRadius;
    if (context.cameraDistSq <= occluderSq || sphere.distSq <= occluderSq) {
        return false;
    }

    const glm::dvec3 toCenter = sphere.center - context.cameraPosModel;
    const double distSq = glm::dot(toCenter, toCenter);
    const double tangentSum = std::sqrt(context.cameraDistSq - occluderSq) +
        std::sqrt(sphere.distSq - occluderSq);
    return distSq > tangentSum * tangentSum;
}

} // namespace openspace::geojson
