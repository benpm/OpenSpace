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

uniform sampler2D blurTexture;


void main() {
  // 3x3 tent filter while going from half to full resolution hides the blockiness
  vec2 texelSize = 1.0 / vec2(textureSize(blurTexture, 0));
  vec2 uv = in_data.texCoords;

  vec3 result = texture(blurTexture, uv).rgb * 0.25;
  result += texture(blurTexture, uv + vec2(-1.0,  0.0) * texelSize).rgb * 0.125;
  result += texture(blurTexture, uv + vec2( 1.0,  0.0) * texelSize).rgb * 0.125;
  result += texture(blurTexture, uv + vec2( 0.0, -1.0) * texelSize).rgb * 0.125;
  result += texture(blurTexture, uv + vec2( 0.0,  1.0) * texelSize).rgb * 0.125;
  result += texture(blurTexture, uv + vec2(-1.0, -1.0) * texelSize).rgb * 0.0625;
  result += texture(blurTexture, uv + vec2( 1.0, -1.0) * texelSize).rgb * 0.0625;
  result += texture(blurTexture, uv + vec2(-1.0,  1.0) * texelSize).rgb * 0.0625;
  result += texture(blurTexture, uv + vec2( 1.0,  1.0) * texelSize).rgb * 0.0625;

  out_color = vec4(result, 1.0);
}
