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

uniform sampler2D brightPassTexture;
uniform int nLevels;
uniform float sigma;


void main() {
  // Every mip level of the bright-pass texture is a progressively wider box blur of the
  // thresholded image. Summing them with Gaussian weights over the level index gives a
  // cheap approximation of a very wide Gaussian blur, with the radius controlled by sigma
  vec3 bloom = vec3(0.0);
  float totalWeight = 0.0;
  for (int i = 0; i < nLevels; i++) {
    float x = float(i);
    float weight = exp(-(x * x) / (2.0 * sigma * sigma));
    bloom += textureLod(brightPassTexture, in_data.texCoords, x).rgb * weight;
    totalWeight += weight;
  }
  out_color = vec4(bloom / totalWeight, 1.0);
}
