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

in Data {
  vec2 texCoords;
} in_data;

layout(location = 0) out vec4 out_color;

uniform sampler2D sceneTexture;
uniform sampler2D bloomTexture;
uniform bool passthrough;
uniform float gamma;

uniform bool applyBloom;
uniform float bloomIntensity;

uniform bool applyToneMapping;
// Has to match the ToneMappingOperator enum in postprocessor.cpp
uniform int toneMappingOperator;
uniform float exposure;
uniform float whitePoint;

uniform bool applyVignette;
uniform float vignetteIntensity;
uniform float vignetteFalloff;
// Number of eye images in the texture horizontally and vertically, (2, 1) for
// side-by-side stereo
uniform vec2 eyeLayout;

const int OperatorClamp = 0;
const int OperatorReinhard = 1;
const int OperatorReinhardExtended = 2;
const int OperatorAces = 3;
const int OperatorFilmic = 4;


// Fit of the ACES filmic curve by Krzysztof Narkowicz
vec3 acesFilm(vec3 x) {
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// The Uncharted 2 curve by John Hable
vec3 hable(vec3 x) {
  const float A = 0.15;
  const float B = 0.50;
  const float C = 0.10;
  const float D = 0.20;
  const float E = 0.02;
  const float F = 0.30;
  return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

vec3 toneMap(vec3 color) {
  switch (toneMappingOperator) {
    case OperatorReinhard:
      return color / (1.0 + color);
    case OperatorReinhardExtended: {
      vec3 numerator = color * (1.0 + color / (whitePoint * whitePoint));
      return numerator / (1.0 + color);
    }
    case OperatorAces:
      return acesFilm(color);
    case OperatorFilmic: {
      const float ExposureBias = 2.0;
      vec3 curr = hable(ExposureBias * color);
      vec3 whiteScale = 1.0 / hable(vec3(11.2));
      return curr * whiteScale;
    }
    case OperatorClamp:
    default:
      return clamp(color, 0.0, 1.0);
  }
}

void main() {
  vec4 scene = texture(sceneTexture, in_data.texCoords);
  if (passthrough) {
    out_color = scene;
    return;
  }

  // All effects operate on linear values; the frame is encoded with 'gamma'
  vec3 color = pow(max(scene.rgb, vec3(0.0)), vec3(gamma));

  if (applyBloom) {
    color += texture(bloomTexture, in_data.texCoords).rgb * bloomIntensity;
  }

  if (applyToneMapping) {
    color = toneMap(color * exposure);
  }

  if (applyVignette) {
    // The vignette is centered on each eye image separately
    vec2 uv = fract(in_data.texCoords * eyeLayout);
    float falloff = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    float vignette = pow(falloff, vignetteFalloff);
    color *= mix(1.0, vignette, vignetteIntensity);
  }

  out_color = vec4(pow(clamp(color, 0.0, 1.0), vec3(1.0 / gamma)), scene.a);
}
