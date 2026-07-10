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

// Per-draw data for batched GeoJson rendering. Must be kept in sync with the
// GeoJsonDrawRecord struct in globegeometryfeature.h (std430, 32 bytes)
struct DrawElement {
  vec4 color;               // rgb + final opacity
  float heightOffset;       // meters
  float sizeOrWidth;        // point size (world units) or line width (pixels)
  float textureWidthFactor; // points only
  uint flags;
};

layout(std430) buffer DrawData {
  DrawElement drawElements[];
};

// Offset of the current multidraw group's first record in drawElements. Shaders index
// records with drawElements[baseDrawId + gl_DrawID]
uniform int baseDrawId;

const uint FlagUseHeightMap = 1u;
const uint FlagBottomAnchor = 2u;
const uint FlagsRenderModeShift = 2u;
const uint FlagsRenderModeMask = 12u; // bits 2-3
