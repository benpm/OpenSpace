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

#include "fragment.glsl"

in Data {
  vec4 positionViewSpace;
  float depth;
  flat vec4 color;
  flat uint flags;
} in_data;

uniform float ambientIntensity = 0.2;
uniform float diffuseIntensity = 0.8;

uniform uint nLightSources;
uniform vec3 lightDirectionsViewSpace[8];
uniform float lightIntensities[8];

// Keep in sync with geojson_drawdata.glsl and the GeoJsonDrawRecord struct in
// globegeometryfeature.h (this stage only needs the flag, not the whole SSBO include)
const uint FlagPerformShading = 16u;

const vec3 LightColor = vec3(1.0);


Fragment getFragment() {
  if (in_data.color.a == 0.0) {
    discard;
  }

  Fragment frag;
  frag.color = in_data.color;

  // Simple diffuse phong shading based on light sources
  if ((in_data.flags & FlagPerformShading) != 0u && nLightSources > 0) {
    // All polygon triangles are flat shaded (the old vertex normals were one flat
    // normal per triangle), so the normal is derived from the view-space position
    // derivatives instead of a vertex attribute. Flip it to face the camera, matching
    // the visible side that used to be lit
    vec3 n = normalize(
      cross(dFdx(in_data.positionViewSpace.xyz), dFdy(in_data.positionViewSpace.xyz))
    );
    if (!gl_FrontFacing) {
      n = -n;
    }

    // Ambient color
    frag.color.xyz = ambientIntensity * in_data.color.rgb;

    for (int i = 0; i < nLightSources; i++) {
      vec3 l = lightDirectionsViewSpace[i];
      vec3 diffuseColor = diffuseIntensity * max(dot(n, l), 0.0) * in_data.color.rgb;
      frag.color.xyz += lightIntensities[i] * (LightColor * diffuseColor);
    }
  }

  frag.depth = in_data.depth;
  frag.gPosition = in_data.positionViewSpace;
  frag.gNormal = vec4(0.0, 0.0, 0.0, 1.0);
  return frag;
}
