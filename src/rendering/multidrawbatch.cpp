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

#include <openspace/rendering/multidrawbatch.h>

#include <ghoul/misc/assert.h>
#include <algorithm>
#include <cstring>

namespace openspace::rendering {

MultiDrawBatch::~MultiDrawBatch() {
    ghoul_assert(_vao == 0, "deinitialize() must be called before destruction");
}

void MultiDrawBatch::initialize(std::vector<StreamSpec> streams,
                                std::vector<AttributeSpec> attributes,
                                size_t drawRecordSize)
{
    ghoul_assert(_vao == 0, "MultiDrawBatch is already initialized");
    ghoul_assert(!streams.empty(), "At least one vertex stream is required");
    ghoul_assert(drawRecordSize > 0, "Draw record size must be non-zero");

    _streams = std::move(streams);
    _drawRecordSize = drawRecordSize;

    _streamVbos.resize(_streams.size(), 0);
    glCreateBuffers(static_cast<GLsizei>(_streamVbos.size()), _streamVbos.data());
    glCreateBuffers(1, &_ssbo);

    glCreateVertexArrays(1, &_vao);
    for (size_t i = 0; i < _streams.size(); i++) {
        glVertexArrayVertexBuffer(
            _vao,
            static_cast<GLuint>(i),
            _streamVbos[i],
            0,
            _streams[i].stride
        );
    }
    for (const AttributeSpec& a : attributes) {
        ghoul_assert(a.streamIndex < _streams.size(), "Invalid stream index");
        glEnableVertexArrayAttrib(_vao, a.location);
        glVertexArrayAttribFormat(
            _vao,
            a.location,
            a.nComponents,
            a.type,
            GL_FALSE,
            a.offsetInStream
        );
        glVertexArrayAttribBinding(_vao, a.location, a.streamIndex);
    }
}

void MultiDrawBatch::deinitialize() {
    if (_vao != 0) {
        glDeleteVertexArrays(1, &_vao);
        _vao = 0;
    }
    if (!_streamVbos.empty()) {
        glDeleteBuffers(static_cast<GLsizei>(_streamVbos.size()), _streamVbos.data());
        _streamVbos.clear();
    }
    if (_ssbo != 0) {
        glDeleteBuffers(1, &_ssbo);
        _ssbo = 0;
    }
    _streams.clear();
    _draws.clear();
    _pending.clear();
    _recordStaging.clear();
    _firsts.clear();
    _counts.clear();
    _groups.clear();
    _groupSizes.clear();
    _isCommitted = false;
}

bool MultiDrawBatch::isInitialized() const {
    return _vao != 0;
}

MultiDrawBatch::DrawHandle MultiDrawBatch::addDraw(GLsizei nVertices,
                                  std::span<const std::span<const std::byte>> streamData)
{
    ghoul_assert(isInitialized(), "MultiDrawBatch must be initialized");
    ghoul_assert(streamData.size() == _streams.size(), "One data span per stream");

    DrawEntry entry;
    entry.active = true;
    entry.nVertices = nVertices;
    entry.streamData.reserve(streamData.size());
    for (size_t i = 0; i < streamData.size(); i++) {
        ghoul_assert(
            streamData[i].size() ==
                static_cast<size_t>(nVertices) * _streams[i].stride,
            "Stream data size must match vertex count and stride"
        );
        entry.streamData.emplace_back(streamData[i].begin(), streamData[i].end());
    }

    // Reuse a freed slot if one exists, to keep handles small
    for (size_t i = 0; i < _draws.size(); i++) {
        if (!_draws[i].active) {
            _draws[i] = std::move(entry);
            _isCommitted = false;
            return i;
        }
    }
    _draws.push_back(std::move(entry));
    _isCommitted = false;
    return _draws.size() - 1;
}

void MultiDrawBatch::removeDraw(DrawHandle handle) {
    ghoul_assert(handle < _draws.size() && _draws[handle].active, "Invalid draw handle");
    _draws[handle] = DrawEntry();
    _isCommitted = false;
}

void MultiDrawBatch::commit() {
    ghoul_assert(isInitialized(), "MultiDrawBatch must be initialized");

    GLint first = 0;
    for (DrawEntry& d : _draws) {
        if (d.active) {
            d.first = first;
            first += d.nVertices;
        }
    }
    const size_t totalVertices = static_cast<size_t>(first);

    std::vector<std::byte> staging;
    for (size_t s = 0; s < _streams.size(); s++) {
        staging.clear();
        staging.reserve(totalVertices * _streams[s].stride);
        for (const DrawEntry& d : _draws) {
            if (d.active) {
                staging.insert(
                    staging.end(),
                    d.streamData[s].begin(),
                    d.streamData[s].end()
                );
            }
        }
        glNamedBufferData(
            _streamVbos[s],
            static_cast<GLsizeiptr>(staging.size()),
            staging.empty() ? nullptr : staging.data(),
            _streams[s].usage
        );
    }
    _isCommitted = true;
}

void MultiDrawBatch::updateStreamRange(DrawHandle handle, GLuint streamIndex,
                                       std::span<const std::byte> data)
{
    ghoul_assert(handle < _draws.size() && _draws[handle].active, "Invalid draw handle");
    ghoul_assert(streamIndex < _streams.size(), "Invalid stream index");

    DrawEntry& d = _draws[handle];
    ghoul_assert(
        data.size() == d.streamData[streamIndex].size(),
        "Updated data must have the same size as the original data"
    );

    std::memcpy(d.streamData[streamIndex].data(), data.data(), data.size());

    // If a commit is pending, the retained copy updated above is uploaded by it and the
    // draw's buffer range may be stale, so only upload directly when committed
    if (_isCommitted) {
        glNamedBufferSubData(
            _streamVbos[streamIndex],
            static_cast<GLintptr>(d.first) * _streams[streamIndex].stride,
            static_cast<GLsizeiptr>(data.size()),
            data.data()
        );
    }
}

void MultiDrawBatch::beginFrame() {
    _pending.clear();
    _recordStaging.clear();
}

void MultiDrawBatch::emitDraw(DrawHandle handle, const void* record, int64_t groupKey) {
    ghoul_assert(handle < _draws.size() && _draws[handle].active, "Invalid draw handle");
    ghoul_assert(_isCommitted, "commit() must be called before emitting draws");

    const size_t offset = _recordStaging.size();
    _recordStaging.resize(offset + _drawRecordSize);
    std::memcpy(
        _recordStaging.data() + offset,
        record,
        _drawRecordSize
    );
    _pending.push_back({ .groupKey = groupKey, .handle = handle,
        .recordOffset = offset });
}

void MultiDrawBatch::endFrame() {
    _firsts.clear();
    _counts.clear();
    _groups.clear();
    _groupSizes.clear();

    if (_pending.empty()) {
        return;
    }

    // In the common case draws are emitted already grouped (often a single group), in
    // which case the sort and the record reshuffle can be skipped entirely
    const bool alreadyGrouped = std::is_sorted(
        _pending.cbegin(),
        _pending.cend(),
        [](const PendingDraw& a, const PendingDraw& b) { return a.groupKey < b.groupKey; }
    );

    if (!alreadyGrouped) {
        // Stable sort keeps emission order within each group deterministic
        std::stable_sort(
            _pending.begin(),
            _pending.end(),
            [](const PendingDraw& a, const PendingDraw& b) {
                return a.groupKey < b.groupKey;
            }
        );
    }

    _firsts.reserve(_pending.size());
    _counts.reserve(_pending.size());
    for (const PendingDraw& p : _pending) {
        const DrawEntry& d = _draws[p.handle];

        if (_groups.empty() || _groups.back().groupKey != p.groupKey) {
            _groups.push_back({
                .groupKey = p.groupKey,
                .baseDrawId = static_cast<int>(_firsts.size())
            });
            _groupSizes.push_back(0);
        }
        _groupSizes.back()++;

        _firsts.push_back(d.first);
        _counts.push_back(d.nVertices);
    }

    const std::byte* records = _recordStaging.data();
    if (!alreadyGrouped) {
        // Reorder the records to match the sorted draw order
        _sortedRecordStaging.clear();
        _sortedRecordStaging.reserve(_recordStaging.size());
        for (const PendingDraw& p : _pending) {
            _sortedRecordStaging.insert(
                _sortedRecordStaging.end(),
                _recordStaging.begin() + p.recordOffset,
                _recordStaging.begin() + p.recordOffset + _drawRecordSize
            );
        }
        records = _sortedRecordStaging.data();
    }

    glNamedBufferData(
        _ssbo,
        static_cast<GLsizeiptr>(_recordStaging.size()),
        records,
        GL_STREAM_DRAW
    );
}

std::span<const MultiDrawBatch::Group> MultiDrawBatch::groups() const {
    return _groups;
}

void MultiDrawBatch::bindVertexArray() const {
    glBindVertexArray(_vao);
}

void MultiDrawBatch::renderGroup(GLenum mode, const Group& group) const {
    ghoul_assert(
        &group >= _groups.data() && &group < _groups.data() + _groups.size(),
        "Group must be an element of groups()"
    );
    const size_t groupIndex = &group - _groups.data();
    glMultiDrawArrays(
        mode,
        _firsts.data() + group.baseDrawId,
        _counts.data() + group.baseDrawId,
        static_cast<GLsizei>(_groupSizes[groupIndex])
    );
}

GLuint MultiDrawBatch::ssboId() const {
    return _ssbo;
}

size_t MultiDrawBatch::nDrawsThisFrame() const {
    return _firsts.size();
}

} // namespace openspace::rendering
