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

uniform sampler2D inputTexture;
// 0 is the mildest and 1 the strongest sharpening
uniform float amount;


// Contrast-adaptive sharpening in the spirit of AMD FidelityFX CAS: the sharpening
// weight of a pixel shrinks where its neighborhood already has high contrast, which
// avoids ringing around hard edges and leaves flat, noisy areas alone
void main() {
  vec2 uv = in_data.texCoords;
  vec4 center = texture(inputTexture, uv);

  vec3 n = textureOffset(inputTexture, uv, ivec2( 0,  1)).rgb;
  vec3 s = textureOffset(inputTexture, uv, ivec2( 0, -1)).rgb;
  vec3 e = textureOffset(inputTexture, uv, ivec2( 1,  0)).rgb;
  vec3 w = textureOffset(inputTexture, uv, ivec2(-1,  0)).rgb;

  vec3 minColor = min(min(min(n, s), min(e, w)), center.rgb);
  vec3 maxColor = max(max(max(n, s), max(e, w)), center.rgb);

  // How much room there is to sharpen without clipping, per channel
  vec3 rcpMax = 1.0 / max(maxColor, vec3(1e-4));
  vec3 headroom = clamp(min(minColor, 1.0 - maxColor) * rcpMax, 0.0, 1.0);
  vec3 sharpening = sqrt(headroom);

  // The peak weight goes from -1/8 (mild) to -1/5 (strong)
  float peak = -1.0 / mix(8.0, 5.0, clamp(amount, 0.0, 1.0));
  vec3 weight = sharpening * peak;

  vec3 result = (weight * (n + s + e + w) + center.rgb) / (4.0 * weight + 1.0);
  out_color = vec4(clamp(result, 0.0, 1.0), center.a);
}
