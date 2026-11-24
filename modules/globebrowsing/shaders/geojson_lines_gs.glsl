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

#version __CONTEXT__

#include "geojson_drawdata.glsl"

// Expands each line segment into a screen-space quad, with the line width taken from
// the per-draw data. Replaces fixed-function glLineWidth, which cannot vary within a
// multidraw call (and whose width range is implementation-defined)

layout(lines) in;
in Data {
  vec4 positionViewSpace;
  flat int drawIndex;
} in_data[];

layout(triangle_strip, max_vertices = 4) out;
out Data {
  vec4 positionViewSpace;
  flat vec4 color;
  // Signed distance from the line's center line, in pixels, for anti-aliasing
  float acrossLine;
  flat float halfWidth;
  float depth;
} out_data;

uniform dmat4 projectionTransform;
uniform vec2 resolution; // framebuffer resolution in pixels

// Segments shorter than this (in pixels) have no usable direction
const float MinSegmentLength = 0.0001;


void main() {
  DrawElement element = drawElements[in_data[0].drawIndex];

  vec4 clip0 = vec4(projectionTransform * dvec4(in_data[0].positionViewSpace));
  vec4 clip1 = vec4(projectionTransform * dvec4(in_data[1].positionViewSpace));

  // Skip segments with an endpoint at or behind the camera plane; offsetting such
  // segments in NDC space is not well-defined
  if (clip0.w <= 0.0 || clip1.w <= 0.0) {
    return;
  }

  vec2 screen0 = (clip0.xy / clip0.w * 0.5 + 0.5) * resolution;
  vec2 screen1 = (clip1.xy / clip1.w * 0.5 + 0.5) * resolution;

  vec2 direction = screen1 - screen0;
  float segmentLength = length(direction);
  direction = (segmentLength > MinSegmentLength) ?
    direction / segmentLength :
    vec2(1.0, 0.0);
  vec2 normal = vec2(-direction.y, direction.x);

  float halfWidth = 0.5 * element.sizeOrWidth;
  // Extra half pixel on each side for the anti-aliasing feather
  float extent = halfWidth + 0.5;

  // Extend the quad ends by the half width (square caps), to cover the notches that
  // otherwise appear at the joints between segments of a line strip
  screen0 -= direction * halfWidth;
  screen1 += direction * halfWidth;

  // All output variables are re-assigned before every EmitVertex, since their values
  // are undefined after each emit
  for (int side = 0; side < 2; side++) {
    float across = (side == 0) ? -extent : extent;
    vec2 offset = normal * across;

    // Back from screen space to clip space, keeping each endpoint's own w. Set z to 0
    // to disable near and far plane, unique handling for perspective in space
    vec2 ndc0 = ((screen0 + offset) / resolution) * 2.0 - 1.0;
    out_data.positionViewSpace = in_data[0].positionViewSpace;
    out_data.color = element.color;
    out_data.acrossLine = across;
    out_data.halfWidth = halfWidth;
    out_data.depth = clip0.w;
    gl_Position = vec4(ndc0 * clip0.w, 0.0, clip0.w);
    EmitVertex();

    vec2 ndc1 = ((screen1 + offset) / resolution) * 2.0 - 1.0;
    out_data.positionViewSpace = in_data[1].positionViewSpace;
    out_data.color = element.color;
    out_data.acrossLine = across;
    out_data.halfWidth = halfWidth;
    out_data.depth = clip1.w;
    gl_Position = vec4(ndc1 * clip1.w, 0.0, clip1.w);
    EmitVertex();
  }

  EndPrimitive();
}
