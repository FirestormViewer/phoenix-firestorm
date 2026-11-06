/**
 * @file llvertexbuffer_pool_test.cpp
 * @brief Exercise real VBO reuse against a context-free GL state model.
 *
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "linden_common.h"
#include "lltut.h"
#include "llglslshader.h"
#include "llvertexbuffer.h"
#include "llvector4a.h"

#include <array>
#include <cstring>
#include <map>
#include <vector>

namespace
{
constexpr U32 UI_MASK = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_COLOR;

template<typename Function>
struct ScopedGLFunction
{
    Function& entry;
    Function saved;

    ScopedGLFunction(Function& entry_point, Function replacement) : entry(entry_point), saved(entry_point)
    {
        entry = replacement;
    }
    ~ScopedGLFunction() { entry = saved; }
};

struct MockGL
{
    struct Attribute
    {
        GLuint buffer = 0;
        size_t offset = 0;
        GLsizei stride = 0;
        bool enabled = false;
    };

    static MockGL* active;
    static GLuint next_name;
    GLuint array_buffer = 0;
    GLuint element_buffer = 0;
    U32 allocations = 0;
    U32 attribute_setups = 0;
    U32 element_binds = 0;
    std::map<GLuint, std::vector<U8>> storage;
    std::array<Attribute, LLVertexBuffer::TYPE_MAX> attributes;

    static GLuint& binding(GLenum target)
    {
        tut::ensure("expected a vertex/index buffer target",
                    target == GL_ARRAY_BUFFER || target == GL_ELEMENT_ARRAY_BUFFER);
        return target == GL_ARRAY_BUFFER ? active->array_buffer : active->element_buffer;
    }
    static void APIENTRY genBuffers(GLsizei count, GLuint* names)
    {
        for (GLsizei i = 0; i < count; ++i) names[i] = next_name++;
    }
    static void APIENTRY bindBuffer(GLenum target, GLuint name)
    {
        binding(target) = name;
        if (target == GL_ELEMENT_ARRAY_BUFFER) ++active->element_binds;
    }
    static void APIENTRY bufferData(GLenum target, GLsizeiptr size, const void* data, GLenum)
    {
        tut::ensure("storage allocation has a bound buffer", binding(target) != 0);
        tut::ensure("nonnegative allocation size", size >= 0);
        auto& bytes = active->storage[binding(target)];
        bytes.resize(size);
        if (data) std::memcpy(bytes.data(), data, size);
        ++active->allocations;
    }
    static void APIENTRY bufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
    {
        tut::ensure("upload has a bound buffer", binding(target) != 0);
        auto& bytes = active->storage.at(binding(target));
        tut::ensure("upload fits the allocation", offset >= 0 && size >= 0
                    && size_t(offset) <= bytes.size() && size_t(size) <= bytes.size() - size_t(offset));
        std::memcpy(bytes.data() + offset, data, size);
    }
    static void APIENTRY deleteBuffers(GLsizei count, const GLuint* names)
    {
        for (GLsizei i = 0; i < count; ++i) active->storage.erase(names[i]);
    }
    static void APIENTRY vertexAttribPointer(GLuint index, GLint, GLenum, GLboolean, GLsizei stride, const void* pointer)
    {
        auto& attribute = active->attributes.at(index);
        attribute.buffer = active->array_buffer;
        attribute.offset = reinterpret_cast<uintptr_t>(pointer);
        attribute.stride = stride;
        ++active->attribute_setups;
    }
    static void APIENTRY enableAttribute(GLuint index) { active->attributes.at(index).enabled = true; }
    static void APIENTRY disableAttribute(GLuint index) { active->attributes.at(index).enabled = false; }

    template<typename Value>
    Value read(GLuint buffer, size_t offset) const
    {
        const auto& bytes = storage.at(buffer);
        tut::ensure("attribute fetch fits the allocation",
                    offset <= bytes.size() && sizeof(Value) <= bytes.size() - offset);
        Value value;
        std::memcpy(&value, bytes.data() + offset, sizeof(value));
        return value;
    }
    template<typename Value>
    Value fetch(LLVertexBuffer::AttributeType type) const
    {
        const auto& attribute = attributes.at(type);
        tut::ensure("draw attribute is enabled", attribute.enabled);
        return read<Value>(attribute.buffer, attribute.offset);
    }
};
MockGL* MockGL::active = nullptr;
GLuint MockGL::next_name = 1;
}

namespace tut
{
struct vertexbuffer_pool_data
{
    MockGL gl;
    ScopedGLFunction<PFNGLGENBUFFERSPROC> gen{glGenBuffers, MockGL::genBuffers};
    ScopedGLFunction<PFNGLBINDBUFFERPROC> bind{glBindBuffer, MockGL::bindBuffer};
    ScopedGLFunction<PFNGLBUFFERDATAPROC> allocate{glBufferData, MockGL::bufferData};
    ScopedGLFunction<PFNGLBUFFERSUBDATAPROC> upload{glBufferSubData, MockGL::bufferSubData};
    ScopedGLFunction<PFNGLDELETEBUFFERSPROC> remove{glDeleteBuffers, MockGL::deleteBuffers};
    ScopedGLFunction<PFNGLVERTEXATTRIBPOINTERPROC> attribute{glVertexAttribPointer, MockGL::vertexAttribPointer};
    ScopedGLFunction<PFNGLENABLEVERTEXATTRIBARRAYPROC> enable{glEnableVertexAttribArray, MockGL::enableAttribute};
    ScopedGLFunction<PFNGLDISABLEVERTEXATTRIBARRAYPROC> disable{glDisableVertexAttribArray, MockGL::disableAttribute};

    bool saved_apple = gGLManager.mIsApple;
    bool saved_amd = gGLManager.mIsAMD;
    bool saved_inited = gGLManager.mInited;
    LLGLSLShader* saved_shader = LLGLSLShader::sCurBoundShaderPtr;
    U32 saved_ui_mask = gUIProgram.mAttributeMask;
    U32 saved_mask = LLVertexBuffer::sLastMask;
    U32 saved_array = LLVertexBuffer::sGLRenderBuffer;
    U32 saved_element = LLVertexBuffer::sGLRenderIndices;

    vertexbuffer_pool_data()
    {
        MockGL::active = &gl;
        gGLManager.mIsApple = false;
        gGLManager.mIsAMD = false;
        // No real GL context is alive. The production delayed-deletion helper
        // therefore avoids its process-wide queue; mock storage is fixture-owned.
        gGLManager.mInited = false;
        gUIProgram.mAttributeMask = UI_MASK;
        LLGLSLShader::sCurBoundShaderPtr = &gUIProgram;
        LLVertexBuffer::sLastMask = 0;
        LLVertexBuffer::initClass(nullptr);
        LLVertexBuffer::unbind();
        LLVertexBuffer::setupClientArrays(UI_MASK);
    }
    ~vertexbuffer_pool_data()
    {
        LLVertexBuffer::unbind();
        LLVertexBuffer::cleanupClass();
        LLVertexBuffer::sLastMask = saved_mask;
        LLVertexBuffer::sGLRenderBuffer = saved_array;
        LLVertexBuffer::sGLRenderIndices = saved_element;
        gUIProgram.mAttributeMask = saved_ui_mask;
        LLGLSLShader::sCurBoundShaderPtr = saved_shader;
        gGLManager.mIsApple = saved_apple;
        gGLManager.mIsAMD = saved_amd;
        gGLManager.mInited = saved_inited;
        MockGL::active = nullptr;
    }

    void fill(LLVertexBuffer& buffer)
    {
        const U32 count = buffer.getNumVerts();
        std::vector<LLVector4a> positions(count);
        for (auto& position : positions) position.set(10.f, 20.f, 30.f, 1.f);
        const std::vector<LLVector2> uv(count, LLVector2(0.25f, 0.75f));
        const std::vector<LLColor4U> colors(count, LLColor4U(17, 51, 85, 119));
        buffer.setPositionData(positions.data());
        if (buffer.hasDataType(LLVertexBuffer::TYPE_NORMAL)) buffer.setNormalData(positions.data());
        buffer.setTexCoord0Data(uv.data());
        buffer.setColorData(colors.data());
    }

    void checkRecycledLayout(U32 vertices, U32 mask)
    {
        LLPointer<LLVertexBuffer> previous = new LLVertexBuffer(UI_MASK);
        ensure("original allocation", previous->allocateBuffer(32, 0));
        // Establish a known original draw layout. No unbind is allowed between
        // releasing it and using its pooled replacement: that would hide #6576.
        LLVertexBuffer::unbind();
        previous->setBuffer();
        fill(*previous);
        const GLuint name = gl.array_buffer;
        const U32 allocations = gl.allocations;
        const U32 old_uv = previous->getOffset(LLVertexBuffer::TYPE_TEXCOORD0);
        const U32 old_color = previous->getOffset(LLVertexBuffer::TYPE_COLOR);
        previous = nullptr;

        LLPointer<LLVertexBuffer> replacement = new LLVertexBuffer(mask);
        ensure("replacement allocation", replacement->allocateBuffer(vertices, 0));
        replacement->setBuffer();
        fill(*replacement);
        ensure_equals("reuse the existing storage, rather than hiding the bug with a new allocation", gl.allocations, allocations);
        ensure_equals("reuse the same GL name", gl.array_buffer, name);
        ensure("UV offset must change", replacement->getOffset(LLVertexBuffer::TYPE_TEXCOORD0) != old_uv);
        ensure("colour offset must change", replacement->getOffset(LLVertexBuffer::TYPE_COLOR) != old_color);

        // First establish that the upload itself is correct. Then fetch through
        // the attribute pointers a draw would use: the unfixed code reads other
        // valid bytes here, producing wrong UVs/colours despite correct storage.
        const auto stored_uv = gl.read<LLVector2>(name, replacement->getOffset(LLVertexBuffer::TYPE_TEXCOORD0));
        ensure_equals("uploaded U", stored_uv.mV[0], 0.25f);
        ensure_equals("uploaded V", stored_uv.mV[1], 0.75f);
        const auto position = gl.fetch<LLVector4a>(LLVertexBuffer::TYPE_VERTEX);
        ensure_equals("drawn position remains correct", position.getF32ptr()[0], 10.f);
        const auto drawn_uv = gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0);
        ensure_equals("draw uses replacement U, not previous layout bytes", drawn_uv.mV[0], 0.25f);
        ensure_equals("draw uses replacement V, not previous layout bytes", drawn_uv.mV[1], 0.75f);
        const auto color = gl.fetch<LLColor4U>(LLVertexBuffer::TYPE_COLOR);
        ensure_equals("draw uses replacement colour", color, LLColor4U(17, 51, 85, 119));
    }
};

using vertexbuffer_pool_group = test_group<vertexbuffer_pool_data>;
using vertexbuffer_pool_object = vertexbuffer_pool_group::object;
vertexbuffer_pool_group vertexbuffer_pool_tests("LLVertexBufferPool");

template<> template<>
void vertexbuffer_pool_object::test<1>()
{
    set_test_name("recycled name with a different vertex count reads the replacement UVs and colours");
    checkRecycledLayout(33, UI_MASK);
}

template<> template<>
void vertexbuffer_pool_object::test<2>()
{
    set_test_name("recycled name with a different attribute layout reads the replacement UVs and colours");
    checkRecycledLayout(22, UI_MASK | LLVertexBuffer::MAP_NORMAL);
}

template<> template<>
void vertexbuffer_pool_object::test<3>()
{
    set_test_name("releasing an unrelated buffer preserves the current layout and cached replay fast path");
    LLPointer<LLVertexBuffer> other = new LLVertexBuffer(UI_MASK);
    LLPointer<LLVertexBuffer> current = new LLVertexBuffer(UI_MASK);
    ensure("other allocation", other->allocateBuffer(32, 0));
    ensure("current allocation", current->allocateBuffer(33, 0));
    LLVertexBuffer::unbind();
    current->setBuffer();
    const GLuint name = gl.array_buffer;
    const U32 setups = gl.attribute_setups;
    other = nullptr;
    ensure_equals("unrelated release preserves binding shadow", LLVertexBuffer::sGLRenderBuffer, name);
    current->setBuffer();
    ensure_equals("cached replay does not repeat attribute setup", gl.attribute_setups, setups);
}

template<> template<>
void vertexbuffer_pool_object::test<4>()
{
    set_test_name("releasing current vertex/index storage invalidates both shadows before reuse");
    LLPointer<LLVertexBuffer> previous = new LLVertexBuffer(UI_MASK);
    ensure("indexed allocation", previous->allocateBuffer(32, 32));
    LLVertexBuffer::unbind();
    previous->setBuffer();
    const GLuint name = gl.element_buffer;
    const U32 allocations = gl.allocations;
    previous = nullptr;
    ensure_equals("released vertex binding invalidated", LLVertexBuffer::sGLRenderBuffer, U32(0));
    ensure_equals("released index binding invalidated", LLVertexBuffer::sGLRenderIndices, U32(0));
    LLPointer<LLVertexBuffer> replacement = new LLVertexBuffer(UI_MASK);
    ensure("indexed replacement", replacement->allocateBuffer(32, 32));
    const U32 binds = gl.element_binds;
    replacement->setBuffer();
    ensure_equals("indexed storage reused", gl.allocations, allocations);
    ensure_equals("index name reused", gl.element_buffer, name);
    ensure_equals("index binding explicitly restored", gl.element_binds, binds + 1);
}

template<> template<>
void vertexbuffer_pool_object::test<5>()
{
    set_test_name("fresh allocation does not masquerade as configured draw attributes");
    LLPointer<LLVertexBuffer> previous = new LLVertexBuffer(UI_MASK);
    ensure("previous allocation", previous->allocateBuffer(32, 0));
    LLVertexBuffer::unbind();
    previous->setBuffer();
    fill(*previous);
    const U32 allocations = gl.allocations;
    LLPointer<LLVertexBuffer> fresh = new LLVertexBuffer(UI_MASK);
    ensure("fresh allocation", fresh->allocateBuffer(33, 0));
    ensure_equals("exercise fresh storage, with the previous owner still alive", gl.allocations, allocations + 1);
    const GLuint name = gl.array_buffer;
    fresh->setBuffer();
    fill(*fresh);
    const std::vector<LLVector2> uv(33, LLVector2(0.5f, 0.875f));
    fresh->setTexCoord0Data(uv.data());
    ensure_equals("fresh storage has correct U", gl.read<LLVector2>(name, fresh->getOffset(LLVertexBuffer::TYPE_TEXCOORD0)).mV[0], 0.5f);
    ensure_equals("draw fetches fresh U rather than previous buffer U", gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0).mV[0], 0.5f);
    ensure_equals("draw attributes reference the fresh buffer", gl.attributes[LLVertexBuffer::TYPE_VERTEX].buffer, name);
}

template<> template<>
void vertexbuffer_pool_object::test<6>()
{
    set_test_name("upload rebinding an existing buffer does not masquerade as configured draw attributes");
    LLPointer<LLVertexBuffer> previous = new LLVertexBuffer(UI_MASK);
    LLPointer<LLVertexBuffer> updated = new LLVertexBuffer(UI_MASK);
    ensure("previous allocation", previous->allocateBuffer(32, 0));
    ensure("updated allocation", updated->allocateBuffer(33, 0));
    LLVertexBuffer::unbind();
    updated->setBuffer();
    fill(*updated);
    const GLuint name = gl.array_buffer;
    previous->setBuffer();
    fill(*previous);

    LLStrider<LLVector2> uv;
    ensure("mapped UVs", updated->getTexCoord0Strider(uv));
    for (U32 i = 0; i < updated->getNumVerts(); ++i) uv[i] = LLVector2(0.75f, 0.125f);
    updated->unmapBuffer();
    ensure_equals("upload bound the updated buffer", gl.array_buffer, name);
    ensure_equals("uploaded storage has correct U", gl.read<LLVector2>(name, updated->getOffset(LLVertexBuffer::TYPE_TEXCOORD0)).mV[0], 0.75f);
    updated->setBuffer();
    ensure_equals("draw fetches updated U rather than previous buffer U", gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0).mV[0], 0.75f);
    ensure_equals("draw attributes reference updated buffer", gl.attributes[LLVertexBuffer::TYPE_VERTEX].buffer, name);
}

template<> template<>
void vertexbuffer_pool_object::test<7>()
{
    set_test_name("attribute enable mask does not masquerade as the configured pointer mask");
    LLPointer<LLVertexBuffer> current = new LLVertexBuffer(UI_MASK);
    LLPointer<LLVertexBuffer> other = new LLVertexBuffer(UI_MASK);
    ensure("current allocation", current->allocateBuffer(32, 0));
    ensure("other allocation", other->allocateBuffer(33, 0));
    LLVertexBuffer::unbind();
    other->setBuffer();
    fill(*other);
    const std::vector<LLVector2> other_uv(33, LLVector2(0.5f, 0.875f));
    other->setTexCoord0Data(other_uv.data());

    // Exercise the pointer-mask distinction directly. The normal shader bind
    // currently unbinds too, which would hide an incorrect pointer-mask cache.
    gUIProgram.mAttributeMask = LLVertexBuffer::MAP_VERTEX;
    LLVertexBuffer::setupClientArrays(gUIProgram.mAttributeMask);
    current->setBuffer();
    fill(*current);
    gUIProgram.mAttributeMask = UI_MASK;
    LLVertexBuffer::setupClientArrays(UI_MASK);
    current->setBuffer();
    ensure_equals("newly required UV pointer belongs to current buffer", gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0).mV[0], 0.25f);
}

template<> template<>
void vertexbuffer_pool_object::test<8>()
{
    set_test_name("same-sized layout change recalculates offsets and refreshes attribute setup");
    LLPointer<LLVertexBuffer> target = new LLVertexBuffer(UI_MASK | LLVertexBuffer::MAP_NORMAL);
    LLPointer<LLVertexBuffer> source = new LLVertexBuffer(UI_MASK | LLVertexBuffer::MAP_TANGENT);
    ensure("target allocation", target->allocateBuffer(22, 0));
    ensure("source allocation", source->allocateBuffer(22, 0));
    ensure_equals("same byte size despite different offsets", source->getSize(), target->getSize());
    LLVertexBuffer::unbind();
    target->setBuffer();
    fill(*target);
    const U32 allocations = gl.allocations;
    const GLuint name = gl.array_buffer;
    source->clone(*target);
    ensure_equals("same-sized layout change keeps storage", gl.allocations, allocations);
    ensure_equals("clone recalculates UV offset", target->getOffset(LLVertexBuffer::TYPE_TEXCOORD0), source->getOffset(LLVertexBuffer::TYPE_TEXCOORD0));
    target->setBuffer();
    fill(*target);
    ensure_equals("same-sized layout keeps GL name", gl.array_buffer, name);
    ensure_equals("draw uses recalculated UV offset", gl.attributes[LLVertexBuffer::TYPE_TEXCOORD0].offset, size_t(source->getOffset(LLVertexBuffer::TYPE_TEXCOORD0)));
    ensure_equals("draw uses recalculated colour offset", gl.attributes[LLVertexBuffer::TYPE_COLOR].offset, size_t(source->getOffset(LLVertexBuffer::TYPE_COLOR)));
}

template<> template<>
void vertexbuffer_pool_object::test<9>()
{
    set_test_name("explicit VAO invalidation restores pointers despite unchanged upload binding");
    LLPointer<LLVertexBuffer> current = new LLVertexBuffer(UI_MASK);
    ensure("current allocation", current->allocateBuffer(32, 0));
    current->setBuffer();
    fill(*current);
    const U32 setups = gl.attribute_setups;
    // A new VAO starts without attributes, but ARRAY_BUFFER is context state
    // and can still name the same buffer. Mirror LLRender::init's reset hook.
    gl.attributes = {};
    LLVertexBuffer::setupClientArrays(0);
    LLVertexBuffer::setupClientArrays(UI_MASK);
    LLVertexBuffer::invalidateAttributeSetup();
    current->setBuffer();
    ensure_equals("all UI pointers restored", gl.attribute_setups, setups + 3);
    ensure_equals("restored UV fetch", gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0).mV[0], 0.25f);
}

template<> template<>
void vertexbuffer_pool_object::test<10>()
{
    set_test_name("unchanged allocation and content updates preserve the attribute fast path");
    LLPointer<LLVertexBuffer> current = new LLVertexBuffer(UI_MASK);
    ensure("current allocation", current->allocateBuffer(32, 0));
    current->setBuffer();
    fill(*current);
    const U32 setups = gl.attribute_setups;
    const U32 allocations = gl.allocations;
    ensure("unchanged allocation request", current->allocateBuffer(32, 0));
    fill(*current);
    current->setBuffer();
    ensure_equals("unchanged geometry capacity keeps storage", gl.allocations, allocations);
    ensure_equals("content updates do not reconfigure pointers", gl.attribute_setups, setups);
}

template<> template<>
void vertexbuffer_pool_object::test<11>()
{
    set_test_name("rebinding after another buffer upload preserves already-correct attribute pointers");
    LLPointer<LLVertexBuffer> current = new LLVertexBuffer(UI_MASK);
    LLPointer<LLVertexBuffer> updated = new LLVertexBuffer(UI_MASK);
    ensure("current allocation", current->allocateBuffer(32, 0));
    ensure("updated allocation", updated->allocateBuffer(33, 0));
    updated->setBuffer();
    fill(*updated);
    current->setBuffer();
    fill(*current);
    const GLuint name = gl.array_buffer;
    const U32 setups = gl.attribute_setups;
    LLStrider<LLVector2> uv;
    ensure("mapped UVs", updated->getTexCoord0Strider(uv));
    for (U32 i = 0; i < updated->getNumVerts(); ++i) uv[i] = LLVector2(0.75f, 0.125f);
    updated->unmapBuffer();
    ensure("upload binds a different buffer", gl.array_buffer != name);
    current->setBuffer();
    ensure_equals("upload binding restored", gl.array_buffer, name);
    ensure_equals("already-correct pointers need no setup", gl.attribute_setups, setups);
    ensure_equals("current UV fetch is preserved", gl.fetch<LLVector2>(LLVertexBuffer::TYPE_TEXCOORD0).mV[0], 0.25f);
}
}
