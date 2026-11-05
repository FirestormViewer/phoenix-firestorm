/**
 * @file lltexturematrix_test.cpp
 * @brief Texture-animation ownership across deferred draw-batch rebuilds.
 *
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "../llviewerprecompiledheaders.h"
#include "../llface.h"
#include "../llspatialpartition.h"
#include "lltut.h"

namespace tut
{
    // Exercise the actual member types without constructing a scene or GL
    // context. This covers lifetime/identity, not rebuild scheduling.
    struct TextureMatrixData
    {
        decltype(LLFace::mTextureMatrix) face_matrix = std::make_shared<LLMatrix4>();
        decltype(LLDrawInfo::mTextureMatrix) batch_matrix = face_matrix;
    };

    typedef test_group<TextureMatrixData> texture_matrix_group;
    typedef texture_matrix_group::object texture_matrix_object;
    texture_matrix_group texture_matrix_tests("texture_matrix_lifetime");

    template<> template<>
    void texture_matrix_object::test<1>()
    {
        // Disabling animation or destroying the face must not invalidate a
        // batch that can still be drawn by a shadow/reflection pass.
        face_matrix->mMatrix[3][0] = 0.25f;
        std::weak_ptr<const LLMatrix4> lifetime = batch_matrix;
        face_matrix.reset();
        ensure("batch keeps retired face matrix alive", !lifetime.expired());
        ensure_equals("retired matrix remains readable", batch_matrix->mMatrix[3][0], 0.25f);
        batch_matrix.reset();
        ensure("last batch releases retired matrix", lifetime.expired());
    }

    template<> template<>
    void texture_matrix_object::test<2>()
    {
        // Copying matrix values into the batch would freeze animation.
        face_matrix->mMatrix[3][1] = 0.5f;
        ensure_equals("batch sees live animation", batch_matrix->mMatrix[3][1], 0.5f);
        decltype(LLDrawInfo::mTextureMatrix) another_batch = face_matrix;
        ensure("same matrix retains batching identity", another_batch == batch_matrix);
    }

    template<> template<>
    void texture_matrix_object::test<3>()
    {
        face_matrix->mMatrix[3][0] = 0.25f;
        std::weak_ptr<const LLMatrix4> old_lifetime = batch_matrix;
        face_matrix.reset();
        face_matrix = std::make_shared<LLMatrix4>();
        face_matrix->mMatrix[3][0] = 0.75f;
        decltype(LLDrawInfo::mTextureMatrix) new_batch = face_matrix;
        ensure("restarted animation has distinct batch identity", new_batch != batch_matrix);
        ensure_equals("old batch retains old matrix", batch_matrix->mMatrix[3][0], 0.25f);
        ensure_equals("new batch uses new matrix", new_batch->mMatrix[3][0], 0.75f);
        batch_matrix = new_batch;
        ensure("rebuilding last old batch releases old matrix", old_lifetime.expired());
    }
}
