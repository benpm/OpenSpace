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

#ifndef __OPENSPACE_CORE___MULTIDRAWBATCH___H__
#define __OPENSPACE_CORE___MULTIDRAWBATCH___H__

#include <ghoul/opengl/ghoul_gl.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace openspace::rendering {

/**
 * A reusable facility for merging many small draws that share a shader program and
 * vertex layout into a small number of glMultiDrawArrays calls.
 *
 * Vertex data for each draw is provided as one span per vertex stream (VBO) and is
 * retained CPU-side, so any draw can be added or removed without the other draws'
 * providers having to regenerate their data.
 *
 * Per-draw data (colors, sizes, flags, ...) is a fixed-size record stored in an SSBO.
 * The draw list and SSBO are rebuilt densely every frame from the emitted draws; a
 * hidden draw is simply not emitted. Draws are grouped by an arbitrary group key
 * (texture id, GL state, ...); each group is rendered with one glMultiDrawArrays and
 * shaders fetch the record via `baseDrawId + gl_DrawID`, where `baseDrawId` is a
 * uniform the client sets to `Group::baseDrawId` before rendering the group.
 */
class MultiDrawBatch {
public:
    using DrawHandle = size_t;
    static constexpr DrawHandle InvalidHandle = static_cast<DrawHandle>(-1);

    struct StreamSpec {
        GLsizei stride;
        GLenum usage;
    };

    struct AttributeSpec {
        GLuint location;
        GLint nComponents;
        GLenum type;
        GLuint streamIndex;
        GLuint offsetInStream;
    };

    struct Group {
        int64_t groupKey;
        int baseDrawId;
    };

    ~MultiDrawBatch();

    void initialize(std::vector<StreamSpec> streams,
        std::vector<AttributeSpec> attributes, size_t drawRecordSize);
    void deinitialize();
    bool isInitialized() const;

    /**
     * Register a draw of \p nVertices vertices, providing one data span per stream.
     * The data is copied and retained. The returned handle stays valid until
     * `removeDraw()`; the actual buffer range is (re)assigned by `commit()`.
     */
    DrawHandle addDraw(GLsizei nVertices,
        std::span<const std::span<const std::byte>> streamData);
    void removeDraw(DrawHandle handle);

    /**
     * Re-concatenate all retained draw data and upload each stream's VBO.
     */
    void commit();

    /**
     * Overwrite one stream's retained data for a draw and upload just that range. The
     * data size must match the size given to `addDraw()`.
     */
    void updateStreamRange(DrawHandle handle, GLuint streamIndex,
        std::span<const std::byte> data);

    /**
     * Allow `endFrame()` to merge queued draws that are adjacent in the committed
     * vertex buffers, belong to the same group and have byte-identical records into a
     * single sub-draw. Merging is opportunistic (non-contiguous draws are not merged)
     * and only valid for non-strip primitive modes, where concatenating two draws'
     * vertices cannot create new primitives between them.
     */
    void setCoalescingEnabled(bool enabled);

    void beginFrame();

    /**
     * Queue a draw for this frame.
     *
     * \param handle The draw to queue
     * \param record Must point at drawRecordSize bytes of per-draw data
     * \param groupKey The group under which the draw is rendered
     */
    void emitDraw(DrawHandle handle, const void* record, int64_t groupKey);

    /**
     * Sort queued draws by group key, build first/count arrays and upload the SSBO.
     */
    void endFrame();

    /**
     * Returns the groups queued this frame, in ascending group-key order.
     */
    std::span<const Group> groups() const;
    void bindVertexArray() const;
    void renderGroup(GLenum mode, const Group& group) const;
    GLuint ssboId() const;

    /**
     * Returns the number of sub-draws after coalescing (what glMultiDrawArrays
     * receives).
     */
    size_t nDrawsThisFrame() const;

private:
    struct DrawEntry {
        bool isActive = false;
        GLsizei nVertices = 0;
        GLint first = 0;
        std::vector<std::vector<std::byte>> streamData;
    };

    struct PendingDraw {
        int64_t groupKey;
        DrawHandle handle;
        size_t recordOffset;
    };

    GLuint _vao = 0;
    std::vector<GLuint> _streamVbos;
    GLuint _ssbo = 0;
    size_t _drawRecordSize = 0;
    std::vector<StreamSpec> _streams;
    std::vector<DrawEntry> _draws;
    std::vector<DrawHandle> _freeSlots;
    bool _isCommitted = false;
    bool _shouldCoalesce = false;

    // Per-frame state
    std::vector<PendingDraw> _pending;
    std::vector<std::byte> _recordStaging;
    std::vector<std::byte> _sortedRecordStaging;
    std::vector<GLint> _firsts;
    std::vector<GLsizei> _counts;
    std::vector<Group> _groups;
    std::vector<size_t> _groupSizes;
};

} // namespace openspace::rendering

#endif // __OPENSPACE_CORE___MULTIDRAWBATCH___H__
