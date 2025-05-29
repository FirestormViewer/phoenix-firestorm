/**
 * @file fspanelface.cpp
 * @brief Consolidated materials/texture panel in the tools floater
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2024, Zi Ree@Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fspanelface.h"

#include <unordered_set>    // Used to find all faces with same texture

#include "llagent.h"
#include "llagentdata.h"
#include "llbutton.h"
#include "llcalc.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "lldrawpoolbump.h"
#include "llerror.h"
#include "llface.h"
// #include "llfilesystem.h"
#include "llfetchedgltfmaterial.h"
#include "llgltfmateriallist.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h" // gInventory
#include "llinventorymodelbackgroundfetch.h"
#include "llfloatermediasettings.h"
#include "llfloaterreg.h"
#include "llfloatertools.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llmaterialmgr.h"
#include "llmaterialeditor.h"
#include "llmediactrl.h"
#include "llmediaentry.h"
#include "llmenubutton.h"
#include "llnotificationsutil.h"
#include "llpanelcontents.h"
#include "llpluginclassmedia.h"
#include "llradiogroup.h"
#include "llrect.h"
#include "llresmgr.h"
// #include "llsd.h"
// #include "llsdserialize.h"
// #include "llsdutil.h"
#include "llselectmgr.h"
#include "llspinctrl.h"
#include "llstring.h"
#include "lltextbox.h"
#include "lltexturectrl.h"
#include "lltextureentry.h"
#include "lltooldraganddrop.h"
#include "lltoolface.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llui.h"
// #include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
// #include "llviewermenufile.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "llvoinventorylistener.h"
#include "lluictrlfactory.h"
#include "llviewertexturelist.h"// Update sel manager as to which channel we're editing so it can reflect the correct overlay UI

// Dirty flags - taken from llmaterialeditor.cpp ... LL please put this in a .h! -Zi
static const U32 MATERIAL_BASE_COLOR_DIRTY = 0x1 << 0;
static const U32 MATERIAL_BASE_COLOR_TEX_DIRTY = 0x1 << 1;

static const U32 MATERIAL_NORMAL_TEX_DIRTY = 0x1 << 2;

static const U32 MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY = 0x1 << 3;
static const U32 MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY = 0x1 << 4;
static const U32 MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY = 0x1 << 5;

static const U32 MATERIAL_EMISIVE_COLOR_DIRTY = 0x1 << 6;
static const U32 MATERIAL_EMISIVE_TEX_DIRTY = 0x1 << 7;

static const U32 MATERIAL_DOUBLE_SIDED_DIRTY = 0x1 << 8;
static const U32 MATERIAL_ALPHA_MODE_DIRTY = 0x1 << 9;
static const U32 MATERIAL_ALPHA_CUTOFF_DIRTY = 0x1 << 10;

FSPanelFace::Selection FSPanelFace::sMaterialOverrideSelection;

void FSPanelFace::updateSelectedGLTFMaterials(std::function<void(LLGLTFMaterial*)> func)
{
    struct LLSelectedTEGLTFMaterialFunctor : public LLSelectedTEFunctor
    {
        LLSelectedTEGLTFMaterialFunctor(std::function<void(LLGLTFMaterial*)> func) : mFunc(func) {}
        virtual ~LLSelectedTEGLTFMaterialFunctor() {};
        bool apply(LLViewerObject* object, S32 face) override
        {
            LLGLTFMaterial new_override;
            const LLTextureEntry* tep = object->getTE(face);
            if (tep->getGLTFMaterialOverride())
            {
                new_override = *tep->getGLTFMaterialOverride();
            }
            mFunc(&new_override);
            LLGLTFMaterialList::queueModify(object, face, &new_override);

            return true;
        }

        std::function<void(LLGLTFMaterial*)> mFunc;
    } select_func(func);

    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&select_func);
}

void FSPanelFace::updateSelectedGLTFMaterialsWithScale(std::function<void(LLGLTFMaterial*, const F32, const F32)> func)
{
    struct LLSelectedTEGLTFMaterialFunctor : public LLSelectedTEFunctor
    {
        LLSelectedTEGLTFMaterialFunctor(std::function<void(LLGLTFMaterial*, const F32, const F32)> func) : mFunc(func) {}
        virtual ~LLSelectedTEGLTFMaterialFunctor() {};
        bool apply(LLViewerObject* object, S32 face) override
        {
            LLGLTFMaterial new_override;
            const LLTextureEntry* tep = object->getTE(face);
            if (tep->getGLTFMaterialOverride())
            {
                new_override = *tep->getGLTFMaterialOverride();
            }

            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            mFunc(&new_override, object->getScale().mV[s_axis], object->getScale().mV[t_axis]);
            LLGLTFMaterialList::queueModify(object, face, &new_override);

            return true;
        }

        std::function<void(LLGLTFMaterial*, const F32, const F32)> mFunc;
    } select_func(func);

    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&select_func);
}

template<typename T>
void readSelectedGLTFMaterial(std::function<T(const LLGLTFMaterial*)> func, T& value, bool& identical, bool has_tolerance, T tolerance)
{
    struct LLSelectedTEGetGLTFMaterialFunctor : public LLSelectedTEGetFunctor<T>
    {
        LLSelectedTEGetGLTFMaterialFunctor(std::function<T(const LLGLTFMaterial*)> func) : mFunc(func) {}
        virtual ~LLSelectedTEGetGLTFMaterialFunctor() {};
        T get(LLViewerObject* object, S32 face) override
        {
            const LLTextureEntry* tep = object->getTE(face);
            const LLGLTFMaterial* render_material = tep->getGLTFRenderMaterial();

            return mFunc(render_material);
        }

        std::function<T(const LLGLTFMaterial*)> mFunc;
    } select_func(func);
    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&select_func, value, has_tolerance, tolerance);
}

void getSelectedGLTFMaterialMaxRepeats(LLGLTFMaterial::TextureInfo channel, F32& repeats, bool& identical)
{
    // The All channel should read base color values
    if (channel == LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_COUNT)
        channel = LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_BASE_COLOR;

    struct LLSelectedTEGetGLTFMaterialMaxRepeatsFunctor : public LLSelectedTEGetFunctor<F32>
    {
        LLSelectedTEGetGLTFMaterialMaxRepeatsFunctor(LLGLTFMaterial::TextureInfo channel) : mChannel(channel) {}
        virtual ~LLSelectedTEGetGLTFMaterialMaxRepeatsFunctor() {};
        F32 get(LLViewerObject* object, S32 face) override
        {
            const LLTextureEntry* tep = object->getTE(face);
            const LLGLTFMaterial* render_material = tep->getGLTFRenderMaterial();
            if (!render_material)
                return 0.f;

            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            F32 repeats_u = render_material->mTextureTransform[mChannel].mScale[VX] / object->getScale().mV[s_axis];
            F32 repeats_v = render_material->mTextureTransform[mChannel].mScale[VY] / object->getScale().mV[t_axis];
            return llmax(repeats_u, repeats_v);
        }

        LLGLTFMaterial::TextureInfo mChannel;
    } max_repeats_func(channel);
    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&max_repeats_func, repeats);
}

//
// keep LLRenderMaterialFunctor in sync with llmaterialeditor.cpp - Would be nice if we
// had this in its own file so we could include it from both sides ... -Zi
//

// local preview of material changes
class FSRenderMaterialFunctor : public LLSelectedTEFunctor
{
public:
    FSRenderMaterialFunctor(const LLUUID &id)
        : mMatId(id)
    {
    }

    bool apply(LLViewerObject* objectp, S32 te) override
    {
        if (objectp && objectp->permModify() && objectp->getVolume())
        {
            LLVOVolume* vobjp = (LLVOVolume*)objectp;
            vobjp->setRenderMaterialID(te, mMatId, false /*preview only*/);
            vobjp->updateTEMaterialTextures(te);
        }
        return true;
    }
private:
    LLUUID mMatId;
};

//
// keep LLRenderMaterialOverrideFunctor in sync with llmaterialeditor.cpp just take out the
// reverting functionality of non texture and color related GLTF settings as it makes no
// real sense with all the texture controls visible for the material at all times.
// TODO: look at how to handle local textures, especially when saving materials
// - Would be nice if we had this in its own file so we could include it from both sides ... -Zi
//
class FSRenderMaterialOverrideFunctor : public LLSelectedNodeFunctor
{
public:
    FSRenderMaterialOverrideFunctor(
        LLGLTFMaterial* material_to_apply,
        U32 unsaved_changes,
        U32 reverted_changes
    )
    : mMaterialToApply(material_to_apply)
    , mUnsavedChanges(unsaved_changes)
    , mRevertedChanges(reverted_changes)
    {
    }

    virtual bool apply(LLSelectNode* nodep) override
    {
        LLViewerObject* objectp = nodep->getObject();
        if (!objectp || !objectp->permModify() || !objectp->getVolume())
        {
            LL_DEBUGS("APPLY_GLTF_CHANGES") << 10001 << " skipped object" << LL_ENDL;
            return false;
        }
        S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces()); // avatars have TEs but no faces
        LL_DEBUGS("APPLY_GLTF_CHANGES") << 10002 << " " << num_tes << LL_ENDL;

        // post override from given object and te to the simulator
        // requestData should have:
        //  object_id - UUID of LLViewerObject
        //  side - S32 index of texture entry
        //  gltf_json - String of GLTF json for override data

        for (S32 te = 0; te < num_tes; ++te)
        {
            if (!nodep->isTESelected(te))
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << 10003 << " skipping unselected te " << te << LL_ENDL;
                continue;
            }

            LL_DEBUGS("APPLY_GLTF_CHANGES") << 10004 << " te " << te << LL_ENDL;
            // Get material from object
            // Selection can cover multiple objects, and live editor is
            // supposed to overwrite changed values only
            LLTextureEntry* tep = objectp->getTE(te);

            if (tep->getGLTFMaterial() == nullptr)
            {
                // overrides are not supposed to work or apply if
                // there is no base material to work from
                LL_DEBUGS("APPLY_GLTF_CHANGES") << 10004 << " skipping te with no base material " << te << LL_ENDL;
                continue;
            }

            LLPointer<LLGLTFMaterial> material = tep->getGLTFMaterialOverride();
            LL_DEBUGS("APPLY_GLTF_CHANGES") << "10004b" << " material is null? " << material.isNull() << LL_ENDL;
            // make a copy to not invalidate existing
            // material for multiple objects
            if (material.isNull())
            {
                // Start with a material override which does not make any changes
                material = new LLGLTFMaterial();
            }
            else
            {
                material = new LLGLTFMaterial(*material);
            }

            LLPointer<LLGLTFMaterial> revert_mat;
            if (nodep->mSavedGLTFOverrideMaterials.size() > te)
            {
                if (nodep->mSavedGLTFOverrideMaterials[te].notNull())
                {
                    revert_mat = nodep->mSavedGLTFOverrideMaterials[te];
                }
                else
                {
                    // mSavedGLTFOverrideMaterials[te] being present but null
                    // means we need to use a default value
                    revert_mat = new LLGLTFMaterial();
                }
            }
            // else can not revert at all

            // Override object's values with values from editor where appropriate
            if (mUnsavedChanges & MATERIAL_BASE_COLOR_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_BASE_COLOR_DIRTY" << LL_ENDL;
                material->setBaseColorFactor(mMaterialToApply->mBaseColor, true);
            }
            else if ((mRevertedChanges & MATERIAL_BASE_COLOR_DIRTY) && revert_mat.notNull())
            {
                material->setBaseColorFactor(revert_mat->mBaseColor, false);
            }

            if (mUnsavedChanges & MATERIAL_BASE_COLOR_TEX_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_BASE_COLOR_TEX_DIRTY" << LL_ENDL;
                material->setBaseColorId(mMaterialToApply->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR], true);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_BASE_COLOR_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }
            else if ((mRevertedChanges & MATERIAL_BASE_COLOR_TEX_DIRTY) && revert_mat.notNull())
            {
                material->setBaseColorId(revert_mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR], false);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_BASE_COLOR_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }

            if (mUnsavedChanges & MATERIAL_NORMAL_TEX_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_NORMAL_TEX_DIRTY" << LL_ENDL;
                material->setNormalId(mMaterialToApply->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL], true);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_NORMAL_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }
            else if ((mRevertedChanges & MATERIAL_NORMAL_TEX_DIRTY) && revert_mat.notNull())
            {
                material->setNormalId(revert_mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL], false);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_NORMAL_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }

            if (mUnsavedChanges & MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY" << LL_ENDL;
                material->setOcclusionRoughnessMetallicId(mMaterialToApply->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS], true);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }
            else if ((mRevertedChanges & MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY) && revert_mat.notNull())
            {
                material->setOcclusionRoughnessMetallicId(revert_mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS], false);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }

            if (mUnsavedChanges & MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY" << LL_ENDL;
                material->setMetallicFactor(mMaterialToApply->mMetallicFactor, true);
            }

            if (mUnsavedChanges & MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY" << LL_ENDL;
                material->setRoughnessFactor(mMaterialToApply->mRoughnessFactor, true);
            }

            if (mUnsavedChanges & MATERIAL_EMISIVE_COLOR_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_EMISIVE_COLOR_DIRTY" << LL_ENDL;
                material->setEmissiveColorFactor(LLColor3(mMaterialToApply->mEmissiveColor), true);
            }
            else if ((mRevertedChanges & MATERIAL_EMISIVE_COLOR_DIRTY) && revert_mat.notNull())
            {
                material->setEmissiveColorFactor(revert_mat->mEmissiveColor, false);
            }

            if (mUnsavedChanges & MATERIAL_EMISIVE_TEX_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_EMISIVE_TEX_DIRTY" << LL_ENDL;
                material->setEmissiveId(mMaterialToApply->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE], true);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_EMISIVE_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }
            else if ((mRevertedChanges & MATERIAL_EMISIVE_TEX_DIRTY) && revert_mat.notNull())
            {
                material->setEmissiveId(revert_mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE], false);
                /*
                LLUUID tracking_id = mEditor->getLocalTextureTrackingIdFromFlag(MATERIAL_EMISIVE_TEX_DIRTY);
                if (tracking_id.notNull())
                {
                    LLLocalBitmapMgr::getInstance()->associateGLTFMaterial(tracking_id, material);
                }
                */
            }

            if (mUnsavedChanges & MATERIAL_DOUBLE_SIDED_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_DOUBLE_SIDED_DIRTY" << LL_ENDL;
                material->setDoubleSided(mMaterialToApply->mDoubleSided, true);
            }

            if (mUnsavedChanges & MATERIAL_ALPHA_MODE_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_ALPHA_MODE_DIRTY" << LL_ENDL;
                material->setAlphaMode(mMaterialToApply->mAlphaMode, true);
            }

            if (mUnsavedChanges & MATERIAL_ALPHA_CUTOFF_DIRTY)
            {
                LL_DEBUGS("APPLY_GLTF_CHANGES") << "applying MATERIAL_ALPHA_CUTOFF_DIRTY" << LL_ENDL;
                material->setAlphaCutoff(mMaterialToApply->mAlphaCutoff, true);
            }

            LL_DEBUGS("APPLY_GLTF_CHANGES") << 10100 << " queueing material update " << te << LL_ENDL;
            LLGLTFMaterialList::queueModify(objectp, te, material);
        }
        return true;
    }

private:
    LLGLTFMaterial* mMaterialToApply;
    U32 mUnsavedChanges;
    U32 mRevertedChanges;
};

//
// Get all material parameters
//
struct LLSelectedTEGetmatId : public LLSelectedTEFunctor
{
    LLSelectedTEGetmatId() :
        mHasFacesWithoutPBR(false),
        mHasFacesWithPBR(false),
        mInitialized(false),
        mMaterial(nullptr)
    {
    }

    bool apply(LLViewerObject* object, S32 te_index) override
    {
        LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << "object " << object << " te_index " << te_index << LL_ENDL;
        LLUUID pbr_id = object->getRenderMaterialID(te_index);
        LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000000 << " Material: " << pbr_id << LL_ENDL;

        if (pbr_id.isNull())
        {
            mHasFacesWithoutPBR = true;
            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000001 << LL_ENDL;
            return false;
        }

        mHasFacesWithPBR = true;
        LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000002 << LL_ENDL;

        if (mInitialized)
        {
            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000003 << LL_ENDL;
            if (mMaterialId != pbr_id)
            {
                LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000004 << LL_ENDL;
                mIdenticalMaterial = false;
            }

            LLGLTFMaterial* te_mat = object->getTE(te_index)->getGLTFMaterial();
            LLGLTFMaterial* te_override = object->getTE(te_index)->getGLTFMaterialOverride();

            // copy base material values to the override material, so we can apply the override to it in the next steps
            if (te_mat)
            {
                mMaterialOverride = *te_mat;
            }

            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000005 << " te_override:" << te_override << LL_ENDL;
            for(U32 map = 0; map < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; map++)
            {
                LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000009 << " map:" << map << LL_ENDL;

                LLUUID texture_map_id;

                LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000018 << "te_mat:" << te_mat << " mMaterialOverride.mTextureId[map]:" << mMaterialOverride.mTextureId[map] << LL_ENDL;
                texture_map_id = mMaterialOverride.mTextureId[map];

                if (te_override)
                {
                    LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << "10000018b" << " te_override->mTextureId[map]:" << te_override->mTextureId[map] << LL_ENDL;
                    if (te_override->mTextureId[map].notNull())
                    {
                        texture_map_id = te_override->mTextureId[map];
                        if (texture_map_id == LLGLTFMaterial::GLTF_OVERRIDE_NULL_UUID)
                        {
                            texture_map_id.setNull();
                        }
                    }
                }

                if (texture_map_id != mMaterialSummary.mTextureId[map])
                {
                    LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000018 << " mIdenticalMap[" << map << "]: false" << LL_ENDL;
                    mIdenticalMap[map] = false;
                }
            }

            if (te_override)
            {
                // copy values from base material to the override where the override has defaults
                if (te_override->mBaseColor == LLGLTFMaterial::getDefaultBaseColor()) mMaterialOverride.mBaseColor = te_mat->mBaseColor;
                if (te_override->mMetallicFactor == LLGLTFMaterial::getDefaultMetallicFactor()) mMaterialOverride.mMetallicFactor = te_mat->mMetallicFactor;
                if (te_override->mRoughnessFactor == LLGLTFMaterial::getDefaultRoughnessFactor()) mMaterialOverride.mRoughnessFactor = te_mat->mRoughnessFactor;
                if (te_override->mEmissiveColor == LLGLTFMaterial::getDefaultEmissiveColor()) mMaterialOverride.mEmissiveColor = te_mat->mEmissiveColor;
                if (te_override->mDoubleSided == LLGLTFMaterial::getDefaultDoubleSided()) mMaterialOverride.mDoubleSided = te_mat->mDoubleSided;
                if (te_override->mAlphaMode == LLGLTFMaterial::getDefaultAlphaMode()) mMaterialOverride.mAlphaMode = te_mat->mAlphaMode;
                if (te_override->mAlphaCutoff == LLGLTFMaterial::getDefaultAlphaCutoff()) mMaterialOverride.mAlphaCutoff = te_mat->mAlphaCutoff;
            }

            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << "10000018c" << " te_mat->mEmissiveColor:" << te_mat->mEmissiveColor << LL_ENDL;

            if (mMaterialOverride.mBaseColor != mMaterialSummary.mBaseColor) mIdenticalBaseColor = false;
            if (mMaterialOverride.mMetallicFactor != mMaterialSummary.mMetallicFactor) mIdenticalMetallic = false;
            if (mMaterialOverride.mRoughnessFactor != mMaterialSummary.mRoughnessFactor) mIdenticalRoughness = false;
            if (mMaterialOverride.mEmissiveColor != mMaterialSummary.mEmissiveColor) mIdenticalEmissive = false;
            if (mMaterialOverride.mDoubleSided != mMaterialSummary.mDoubleSided) mIdenticalDoubleSided = false;
            if (mMaterialOverride.mAlphaMode != mMaterialSummary.mAlphaMode) mIdenticalAlphaMode = false;
            if (mMaterialOverride.mAlphaCutoff != mMaterialSummary.mAlphaCutoff) mIdenticalAlphaCutoff = false;

            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000011 << LL_ENDL;
        }
        else
        {
            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000012 << LL_ENDL;

            mIdenticalMaterial = true;
            mIdenticalBaseColor = true;
            mIdenticalMetallic = true;
            mIdenticalRoughness = true;
            mIdenticalEmissive = true;
            mIdenticalDoubleSided = true;
            mIdenticalAlphaMode = true;
            mIdenticalAlphaCutoff = true;

            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000013 << LL_ENDL;

            LLGLTFMaterial* mat = object->getTE(te_index)->getGLTFMaterial();
            LLGLTFMaterial* override = object->getTE(te_index)->getGLTFMaterialOverride();

            mMaterialId = pbr_id;
            mMaterial = override;

            LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000014 << " override:" << override << LL_ENDL;

            // copy base material values to the "summary" material, so we can apply the override to it in the next step
            if (mat)
            {
                mMaterialSummary = *mat;
            }

            for(U32 map = 0; map < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; map++)
            {
                // initialize array to say "all PBR maps are the same"
                mIdenticalMap[map] = true;

                LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << 10000017 << " map:" << map << LL_ENDL;

                if (override && override->mTextureId[map].notNull())
                {
                    LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS") << "10000017b" << " override->mTextureId[map]:" << override->mTextureId[map] << LL_ENDL;
                    if (override->mTextureId[map] == LLGLTFMaterial::GLTF_OVERRIDE_NULL_UUID)
                    {
                        mMaterialSummary.mTextureId[map].setNull();
                    }
                    else
                    {
                        mMaterialSummary.mTextureId[map] = override->mTextureId[map];
                    }
                }
            }

            if (override)
            {
                // copy values from override to the "summary" material where the override differs from the default
                if (override->mBaseColor != LLGLTFMaterial::getDefaultBaseColor()) mMaterialSummary.mBaseColor = override->mBaseColor;
                if (override->mMetallicFactor != LLGLTFMaterial::getDefaultMetallicFactor()) mMaterialSummary.mMetallicFactor = override->mMetallicFactor;
                if (override->mRoughnessFactor != LLGLTFMaterial::getDefaultRoughnessFactor()) mMaterialSummary.mRoughnessFactor = override->mRoughnessFactor;
                if (override->mEmissiveColor != LLGLTFMaterial::getDefaultEmissiveColor()) mMaterialSummary.mEmissiveColor = override->mEmissiveColor;
                if (override->mDoubleSided != LLGLTFMaterial::getDefaultDoubleSided()) mMaterialSummary.mDoubleSided = override->mDoubleSided;
                if (override->mAlphaMode != LLGLTFMaterial::getDefaultAlphaMode()) mMaterialSummary.setAlphaMode(override->getAlphaMode());
                if (override->mAlphaCutoff != LLGLTFMaterial::getDefaultAlphaCutoff()) mMaterialSummary.mAlphaCutoff = override->mAlphaCutoff;
            }

            mInitialized = true;
        }

        return true;
    }

    bool mHasFacesWithoutPBR;
    bool mHasFacesWithPBR;
    bool mInitialized;

    LLGLTFMaterial* mMaterial;
    LLGLTFMaterial mMaterialOverride;
    LLGLTFMaterial mMaterialSummary;

    bool mIdenticalMaterial;

    bool mIdenticalMap[LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT];
    bool mIdenticalBaseColor;
    bool mIdenticalMetallic;
    bool mIdenticalRoughness;
    bool mIdenticalEmissive;
    bool mIdenticalDoubleSided;
    bool mIdenticalAlphaMode;
    bool mIdenticalAlphaCutoff;

    LLUUID mMaterialId;
};

//
// "Use texture" label for normal/specular type comboboxes
// Filled in at initialization from translated strings
//
std::string USE_TEXTURE_LABEL;      // subtly different (and more clear) name from llpanelface.cpp to avoid clashes -Zi

FSPanelFace::FSPanelFace() :
    LLPanel(),
    mNeedMediaTitle(true),
    mUnsavedChanges(0),
    mRevertedChanges(0),
    mExcludeWater(false)
{
    // register callbacks before buildFromFile() or they won't work!
    mCommitCallbackRegistrar.add("BuildTool.Flip",              boost::bind(&FSPanelFace::onCommitFlip, this, _2));
    mCommitCallbackRegistrar.add("BuildTool.GLTFUVSpinner",     boost::bind(&FSPanelFace::onCommitGLTFUVSpinner, this, _1, _2));
    mCommitCallbackRegistrar.add("BuildTool.SelectSameTexture", boost::bind(&FSPanelFace::onClickBtnSelectSameTexture, this, _1, _2));
    mCommitCallbackRegistrar.add("BuildTool.ShowFindAllButton", boost::bind(&FSPanelFace::onShowFindAllButton, this, _1, _2));
    mCommitCallbackRegistrar.add("BuildTool.HideFindAllButton", boost::bind(&FSPanelFace::onHideFindAllButton, this, _1, _2));

    buildFromFile("panel_fs_tools_texture.xml");

    USE_TEXTURE_LABEL = LLTrans::getString("use_texture");
}

FSPanelFace::~FSPanelFace()
{
    unloadMedia();
}

//
// Methods
//

// make sure all referenced UI controls are actually present in the skin
// and if not, terminate immediately with the line number of the error,
// so the skin creator will be alerted that something is wrong, and the
// rest of the code can assume all UI controls are actually there -Zi
#define SKIN_CHECK(x) if ( !(x) ) { skin_error(__LINE__); }

void skin_error(int line)
{
    LL_ERRS("BuildTool") << "Skin error in line: " << line << LL_ENDL;
}

void FSPanelFace::onMatTabChange()
{
    static S32 last_mat = -1;
    if( auto curr_mat = getCurrentMaterialType(); curr_mat != last_mat )
    {
        LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
        LLViewerObject* objectp = node ? node->getObject() : NULL;
        if(objectp)
        {
            last_mat = curr_mat;
            // Iterate through the linkset and mark each object for update
            for (LLObjectSelection::iterator iter = LLSelectMgr::getInstance()->getSelection()->begin();
                 iter != LLSelectMgr::getInstance()->getSelection()->end(); ++iter)
            {
                LLSelectNode* select_node = *iter;
                LLViewerObject* linked_objectp = select_node->getObject();
                if (linked_objectp)
                {
                    linked_objectp->markForUpdate();
                    linked_objectp->faceMappingChanged();

                    // Ensure full rebuild of geometry
                    if (linked_objectp->mDrawable.notNull())
                    {
                        linked_objectp->mDrawable->setState(LLDrawable::REBUILD_ALL);
                    }
                }
            }
        }

        // Since we allow both PBR and BP textures to be applied at the same time,
        // we need to hide or show the GLTF material only locally based on the current tab.
        gSavedSettings.setBOOL("FSShowSelectedInBlinnPhong", (curr_mat == MATMEDIA_MATERIAL));
        if (curr_mat != MATMEDIA_PBR)
            LLSelectMgr::getInstance()->hideGLTFMaterial();
        else
            LLSelectMgr::getInstance()->showGLTFMaterial();

        // Fixes some UI desync
        updateUI(true);
    }
}

void FSPanelFace::onMatChannelTabChange()
{
    // Fixes some UI desync
    static S32 last_channel = -1;
    if (auto curr_channel = getCurrentMatChannel(); curr_channel != last_channel)
    {
        last_channel = curr_channel;
        if (!mSetChannelTab)
            updateUI(true);
    }
    mSetChannelTab = false;
}

void FSPanelFace::onPBRChannelTabChange()
{
    // Fixes some UI desync
    static S32 last_channel = -1;
    if (auto curr_channel = getCurrentPBRChannel(); curr_channel != last_channel)
    {
        last_channel = curr_channel;
        if (!mSetChannelTab)
            updateUI(true);
    }
    mSetChannelTab = false;
}

bool FSPanelFace::postBuild()
{
    //
    // grab pointers to all relevant UI elements here and verify they are good
    //

    // Tab controls
    SKIN_CHECK(mTabsPBRMatMedia = findChild<LLTabContainer>("tabs_material_type"));
    SKIN_CHECK(mTabsMatChannel = findChild<LLTabContainer>("tabs_blinn_phong_uvs"));
    SKIN_CHECK(mTabsPBRChannel = findChild<LLTabContainer>("tabs_pbr_transforms"));

    // common controls and parameters for Blinn-Phong and PBR
    SKIN_CHECK(mBtnCopyFaces = findChild<LLButton>("copy_face_btn"));
    SKIN_CHECK(mBtnPasteFaces = findChild<LLButton>("paste_face_btn"));
    SKIN_CHECK(mCtrlGlow = findChild<LLSpinCtrl>("glow"));
    SKIN_CHECK(mCtrlRpt = findChild<LLSpinCtrl>("rptctrl"));

    // Blinn-Phong alpha parameters
    SKIN_CHECK(mCtrlColorTransp = findChild<LLSpinCtrl>("ColorTrans"));
    SKIN_CHECK(mColorTransPercent = findChild<LLView>("color trans percent"));
    SKIN_CHECK(mLabelAlphaMode = findChild<LLTextBox>("label alphamode"));
    SKIN_CHECK(mComboAlphaMode = findChild<LLComboBox>("combobox alphamode"));
    SKIN_CHECK(mCtrlMaskCutoff = findChild<LLSpinCtrl>("maskcutoff"));
    SKIN_CHECK(mCheckFullbright = findChild<LLCheckBoxCtrl>("checkbox fullbright"));
    SKIN_CHECK(mCheckHideWater = findChild<LLCheckBoxCtrl>("checkbox_hide_water"));

    // Blinn-Phong texture transforms and controls
    SKIN_CHECK(mComboTexGen = findChild<LLComboBox>("combobox texgen"));
    SKIN_CHECK(mCheckPlanarAlign = findChild<LLCheckBoxCtrl>("checkbox planar align"));
    SKIN_CHECK(mLabelBumpiness = findChild<LLView>("label bumpiness"));
    SKIN_CHECK(mComboBumpiness = findChild<LLComboBox>("combobox bumpiness"));
    SKIN_CHECK(mLabelShininess = findChild<LLView>("label shininess"));
    SKIN_CHECK(mComboShininess = findChild<LLComboBox>("combobox shininess"));
    SKIN_CHECK(mCtrlTexScaleU = findChild<LLSpinCtrl>("TexScaleU"));
    SKIN_CHECK(mCtrlTexScaleV = findChild<LLSpinCtrl>("TexScaleV"));
    SKIN_CHECK(mCtrlTexOffsetU = findChild<LLSpinCtrl>("TexOffsetU"));
    SKIN_CHECK(mCtrlTexOffsetV = findChild<LLSpinCtrl>("TexOffsetV"));
    SKIN_CHECK(mCtrlTexRot = findChild<LLSpinCtrl>("TexRot"));
    SKIN_CHECK(mCtrlBumpyScaleU = findChild<LLSpinCtrl>("bumpyScaleU"));
    SKIN_CHECK(mCtrlBumpyScaleV = findChild<LLSpinCtrl>("bumpyScaleV"));
    SKIN_CHECK(mCtrlBumpyOffsetU = findChild<LLSpinCtrl>("bumpyOffsetU"));
    SKIN_CHECK(mCtrlBumpyOffsetV = findChild<LLSpinCtrl>("bumpyOffsetV"));
    SKIN_CHECK(mCtrlBumpyRot = findChild<LLSpinCtrl>("bumpyRot"));
    SKIN_CHECK(mCtrlShinyScaleU = findChild<LLSpinCtrl>("shinyScaleU"));
    SKIN_CHECK(mCtrlShinyScaleV = findChild<LLSpinCtrl>("shinyScaleV"));
    SKIN_CHECK(mCtrlShinyOffsetU = findChild<LLSpinCtrl>("shinyOffsetU"));
    SKIN_CHECK(mCtrlShinyOffsetV = findChild<LLSpinCtrl>("shinyOffsetV"));
    SKIN_CHECK(mCtrlShinyRot = findChild<LLSpinCtrl>("shinyRot"));
    SKIN_CHECK(mCtrlGlossiness = findChild<LLSpinCtrl>("glossiness"));
    SKIN_CHECK(mCtrlEnvironment = findChild<LLSpinCtrl>("environment"));

    // Blinn-Phong Diffuse tint color swatch
    SKIN_CHECK(mColorSwatch = findChild<LLColorSwatchCtrl>("colorswatch"));

    // Blinn-Phong Diffuse texture swatch
    SKIN_CHECK(mTextureCtrl = findChild<LLTextureCtrl>("texture control"));

    // Blinn-Phong Normal texture swatch
    SKIN_CHECK(mBumpyTextureCtrl = findChild<LLTextureCtrl>("bumpytexture control"));

    // Blinn-Phong Specular texture swatch
    SKIN_CHECK(mShinyTextureCtrl = findChild<LLTextureCtrl>("shinytexture control"));

    // Blinn-Phong Specular tint color swatch
    SKIN_CHECK(mShinyColorSwatch = findChild<LLColorSwatchCtrl>("shinycolorswatch"));

    // Texture alignment and maps synchronization
    SKIN_CHECK(mBtnAlignMedia = findChild<LLButton>("button align"));
    SKIN_CHECK(mBtnAlignTextures = findChild<LLButton>("button align textures"));
    SKIN_CHECK(mCheckSyncMaterials = findChild<LLCheckBoxCtrl>("checkbox_sync_settings"))

    // Media
    SKIN_CHECK(mBtnDeleteMedia = findChild<LLButton>("delete_media"));
    SKIN_CHECK(mBtnAddMedia = findChild<LLButton>("add_media"));
    SKIN_CHECK(mTitleMedia = findChild<LLMediaCtrl>("title_media"));
    SKIN_CHECK(mTitleMediaText = findChild<LLTextBox>("media_info"));

    // PBR
    SKIN_CHECK(mMaterialCtrlPBR = findChild<LLTextureCtrl>("pbr_control"));
    SKIN_CHECK(mBaseTexturePBR = findChild<LLTextureCtrl>("base_color_picker"));
    SKIN_CHECK(mBaseTintPBR = findChild<LLColorSwatchCtrl>("base_color_tint_picker"));
    SKIN_CHECK(mNormalTexturePBR = findChild<LLTextureCtrl>("normal_map_picker"));
    SKIN_CHECK(mORMTexturePBR = findChild<LLTextureCtrl>("metallic_map_picker"));
    SKIN_CHECK(mEmissiveTexturePBR = findChild<LLTextureCtrl>("emissive_map_picker"));
    SKIN_CHECK(mEmissiveTintPBR = findChild<LLColorSwatchCtrl>("emissive_color_tint_picker"));
    SKIN_CHECK(mCheckDoubleSidedPBR = findChild<LLCheckBoxCtrl>("double sided"));
    SKIN_CHECK(mAlphaPBR = findChild<LLSpinCtrl>("transparency"));
    SKIN_CHECK(mLabelAlphaModePBR = findChild<LLTextBox>("blend mode label"));
    SKIN_CHECK(mAlphaModePBR = findChild<LLComboBox>("alpha mode"));
    SKIN_CHECK(mMaskCutoffPBR = findChild<LLSpinCtrl>("alpha cutoff"));
    SKIN_CHECK(mMetallicFactorPBR = findChild<LLSpinCtrl>("metalness factor"));
    SKIN_CHECK(mRoughnessFactorPBR = findChild<LLSpinCtrl>("roughness factor"));
    SKIN_CHECK(mBtnSavePBR = findChild<LLButton>("save_selected_pbr"));

    //
    // hook up callbacks and do setup of all relevant UI elements here
    //
    mTabsPBRMatMedia->setCommitCallback(boost::bind(&FSPanelFace::onMatTabChange, this));
    mTabsMatChannel->setCommitCallback(boost::bind(&FSPanelFace::onMatChannelTabChange, this));
    mTabsPBRChannel->setCommitCallback(boost::bind(&FSPanelFace::onPBRChannelTabChange, this));

    // common controls and parameters for Blinn-Phong and PBR
    mBtnCopyFaces->setCommitCallback(boost::bind(&FSPanelFace::onCopyFaces, this));
    mBtnPasteFaces->setCommitCallback(boost::bind(&FSPanelFace::onPasteFaces, this));
    mCtrlGlow->setCommitCallback(boost::bind(&FSPanelFace::onCommitGlow, this));
    mCtrlRpt->setCommitCallback(boost::bind(&FSPanelFace::onCommitRepeatsPerMeter, this));

    // Blinn-Phong alpha parameters
    mCtrlColorTransp->setCommitCallback(boost::bind(&FSPanelFace::onCommitAlpha, this));
    mComboAlphaMode->setCommitCallback(boost::bind(&FSPanelFace::onCommitAlphaMode, this));
    mCtrlMaskCutoff->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialMaskCutoff, this));
    mCheckFullbright->setCommitCallback(boost::bind(&FSPanelFace::onCommitFullbright, this));
    mCheckHideWater->setCommitCallback(boost::bind(&FSPanelFace::onCommitHideWater, this));

    // Blinn-Phong texture transforms and controls
    mComboTexGen->setCommitCallback(boost::bind(&FSPanelFace::onCommitTexGen, this));
    mCheckPlanarAlign->setCommitCallback(boost::bind(&FSPanelFace::onCommitPlanarAlign, this));
    mComboBumpiness->setCommitCallback(boost::bind(&FSPanelFace::onCommitBump, this));
    mComboShininess->setCommitCallback(boost::bind(&FSPanelFace::onCommitShiny, this));
    mCtrlTexScaleU->setCommitCallback(boost::bind(&FSPanelFace::onCommitTextureScaleX, this));
    mCtrlTexScaleV->setCommitCallback(boost::bind(&FSPanelFace::onCommitTextureScaleY, this));
    mCtrlTexOffsetU->setCommitCallback(boost::bind(&FSPanelFace::onCommitTextureOffsetX, this));
    mCtrlTexOffsetV->setCommitCallback(boost::bind(&FSPanelFace::onCommitTextureOffsetY, this));
    mCtrlTexRot->setCommitCallback(boost::bind(&FSPanelFace::onCommitTextureRot, this));
    mCtrlBumpyScaleU->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialBumpyScaleX, this));
    mCtrlBumpyScaleV->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialBumpyScaleY, this));
    mCtrlBumpyOffsetU->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialBumpyOffsetX, this));
    mCtrlBumpyOffsetV->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialBumpyOffsetY, this));
    mCtrlBumpyRot->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialBumpyRot, this));
    mCtrlShinyScaleU->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialShinyScaleX, this));
    mCtrlShinyScaleV->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialShinyScaleY, this));
    mCtrlShinyOffsetU->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialShinyOffsetX, this));
    mCtrlShinyOffsetV->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialShinyOffsetY, this));
    mCtrlShinyRot->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialShinyRot, this));
    mCtrlGlossiness->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialGloss, this));
    mCtrlEnvironment->setCommitCallback(boost::bind(&FSPanelFace::onCommitMaterialEnv, this));

    // Blinn-Phong Diffuse tint color swatch
    mColorSwatch->setCommitCallback(boost::bind(&FSPanelFace::onCommitColor, this));
    mColorSwatch->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelColor, this));
    mColorSwatch->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectColor, this));

    // Blinn-Phong Diffuse texture swatch
    mTextureCtrl->setDefaultImageAssetID(DEFAULT_OBJECT_TEXTURE);
    mTextureCtrl->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectTexture, this));
    mTextureCtrl->setCommitCallback(boost::bind(&FSPanelFace::onCommitTexture, this));
    mTextureCtrl->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelTexture, this));
    mTextureCtrl->setDragCallback(boost::bind(&FSPanelFace::onDragTexture, this, _1, _2));
    mTextureCtrl->setOnCloseCallback(boost::bind(&FSPanelFace::onCloseTexturePicker, this));
    mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
    mTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

    // Blinn-Phong Normal texture swatch
    mBumpyTextureCtrl->setDefaultImageAssetID(DEFAULT_OBJECT_NORMAL);
    mBumpyTextureCtrl->setBlankImageAssetID(BLANK_OBJECT_NORMAL);
    mBumpyTextureCtrl->setCommitCallback(boost::bind(&FSPanelFace::onCommitNormalTexture, this));
    mBumpyTextureCtrl->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelNormalTexture, this));
    mBumpyTextureCtrl->setDragCallback(boost::bind(&FSPanelFace::onDragTexture, this, _1, _2));
    mBumpyTextureCtrl->setOnCloseCallback(boost::bind(&FSPanelFace::onCloseTexturePicker, this));
    mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
    mBumpyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

    // Blinn-Phong Specular texture swatch
    mShinyTextureCtrl->setDefaultImageAssetID(DEFAULT_OBJECT_SPECULAR);
    mShinyTextureCtrl->setCommitCallback(boost::bind(&FSPanelFace::onCommitSpecularTexture, this));
    mShinyTextureCtrl->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelSpecularTexture, this));
    mShinyTextureCtrl->setDragCallback(boost::bind(&FSPanelFace::onDragTexture, this, _1, _2));
    mShinyTextureCtrl->setOnCloseCallback(boost::bind(&FSPanelFace::onCloseTexturePicker, this));
    mShinyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
    mShinyTextureCtrl->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);

    // Blinn-Phong Specular tint color swatch
    mShinyColorSwatch->setCommitCallback(boost::bind(&FSPanelFace::onCommitShinyColor, this));
    mShinyColorSwatch->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelShinyColor, this));
    mShinyColorSwatch->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectShinyColor, this));
    mShinyColorSwatch->setCanApplyImmediately(true);

    // Texture alignment and maps synchronization
    mBtnAlignMedia->setCommitCallback(boost::bind(&FSPanelFace::onClickAutoFix, this));
    mBtnAlignTextures->setCommitCallback(boost::bind(&FSPanelFace::onAlignTexture, this));
    mCheckSyncMaterials->setCommitCallback(boost::bind(&FSPanelFace::onClickMapsSync, this));

    // Media
    mBtnDeleteMedia->setCommitCallback(boost::bind(&FSPanelFace::onClickBtnDeleteMedia, this));
    mBtnAddMedia->setCommitCallback(boost::bind(&FSPanelFace::onClickBtnAddMedia, this));

    // PBR Base Material swatch
    // mMaterialCtrlPBR->setDefaultImageAssetID(LLUUID::null);  // we have no default material, and null is standard for LLUUID -Zi
    mMaterialCtrlPBR->setBlankImageAssetID(BLANK_MATERIAL_ASSET_ID);
    mMaterialCtrlPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this));
    mMaterialCtrlPBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this));
    mMaterialCtrlPBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this));
    mMaterialCtrlPBR->setDragCallback(boost::bind(&FSPanelFace::onDragPbr, this, _2));
    mMaterialCtrlPBR->setOnTextureSelectedCallback(boost::bind(&FSPanelFace::onPbrSelectionChanged, this, _1));
    mMaterialCtrlPBR->setOnCloseCallback(boost::bind(&FSPanelFace::onCloseTexturePicker, this));
    mMaterialCtrlPBR->setImmediateFilterPermMask(PERM_NONE);
    mMaterialCtrlPBR->setDnDFilterPermMask(PERM_COPY | PERM_TRANSFER);
    mMaterialCtrlPBR->setBakeTextureEnabled(false);
    mMaterialCtrlPBR->setInventoryPickType(PICK_MATERIAL);

    // PBR Base Color texture swatch
    mBaseTexturePBR->setDefaultImageAssetID(DEFAULT_OBJECT_TEXTURE);
    mBaseTexturePBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mBaseTexturePBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mBaseTexturePBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR Base Color tint color swatch
    mBaseTintPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mBaseTintPBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mBaseTintPBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR Base Normal map swatch
    mNormalTexturePBR->setDefaultImageAssetID(DEFAULT_OBJECT_NORMAL);
    mNormalTexturePBR->setBlankImageAssetID(BLANK_OBJECT_NORMAL);
    mNormalTexturePBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mNormalTexturePBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mNormalTexturePBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR Base Emissive map swatch
    mEmissiveTexturePBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mEmissiveTexturePBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mEmissiveTexturePBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR Emissive tint color swatch
    mEmissiveTintPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mEmissiveTintPBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mEmissiveTintPBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR Base (O)RM map swatch
    mORMTexturePBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mORMTexturePBR->setOnCancelCallback(boost::bind(&FSPanelFace::onCancelPbr, this, _1));
    mORMTexturePBR->setOnSelectCallback(boost::bind(&FSPanelFace::onSelectPbr, this, _1));

    // PBR parameters
    mCheckDoubleSidedPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mAlphaPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mAlphaModePBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mMaskCutoffPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mMetallicFactorPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));
    mRoughnessFactorPBR->setCommitCallback(boost::bind(&FSPanelFace::onCommitPbr, this, _1));

    // PBR Save material button
    mBtnSavePBR->setCommitCallback(boost::bind(&FSPanelFace::onClickBtnSavePBR, this));

    // Only allow fully permissive textures
    if (!gAgent.isGodlike())
    {
        mBaseTexturePBR->setFilterPermissionMasks(PERM_COPY | PERM_TRANSFER);
        mNormalTexturePBR->setFilterPermissionMasks(PERM_COPY | PERM_TRANSFER);
        mORMTexturePBR->setFilterPermissionMasks(PERM_COPY | PERM_TRANSFER);
        mEmissiveTexturePBR->setFilterPermissionMasks(PERM_COPY | PERM_TRANSFER);
    }

    //
    // set up user interface
    //

    LLGLTFMaterialList::addSelectionUpdateCallback(onMaterialOverrideReceived);
    sMaterialOverrideSelection.connect();

    changePrecision(gSavedSettings.getS32("FSBuildToolDecimalPrecision"));

    selectMaterialType(MATMEDIA_PBR);
    selectMatChannel(LLRender::DIFFUSE_MAP);
    selectPBRChannel(LLRender::NUM_TEXTURE_CHANNELS);

    return true;
}

//
// Things the UI provides...
//

LLUUID  FSPanelFace::getCurrentNormalMap() const     { return mBumpyTextureCtrl->getImageAssetID(); }
LLUUID  FSPanelFace::getCurrentSpecularMap() const   { return mShinyTextureCtrl->getImageAssetID(); }
U32     FSPanelFace::getCurrentShininess()           { return mComboShininess->getCurrentIndex(); }
U32     FSPanelFace::getCurrentBumpiness()           { return mComboBumpiness->getCurrentIndex(); }
U8      FSPanelFace::getCurrentDiffuseAlphaMode()    { return (U8)mComboAlphaMode->getCurrentIndex(); }
U8      FSPanelFace::getCurrentAlphaMaskCutoff()     { return (U8)mCtrlMaskCutoff->getValue().asInteger(); }
U8      FSPanelFace::getCurrentEnvIntensity()        { return (U8)mCtrlEnvironment->getValue().asInteger(); }
U8      FSPanelFace::getCurrentGlossiness()          { return (U8)mCtrlGlossiness->getValue().asInteger(); }
F32     FSPanelFace::getCurrentBumpyRot()            { return (F32)mCtrlBumpyRot->getValue().asReal(); }
F32     FSPanelFace::getCurrentBumpyScaleU()         { return (F32)mCtrlBumpyScaleU->getValue().asReal(); }
F32     FSPanelFace::getCurrentBumpyScaleV()         { return (F32)mCtrlBumpyScaleV->getValue().asReal(); }
F32     FSPanelFace::getCurrentBumpyOffsetU()        { return (F32)mCtrlBumpyOffsetU->getValue().asReal(); }
F32     FSPanelFace::getCurrentBumpyOffsetV()        { return (F32)mCtrlBumpyOffsetV->getValue().asReal(); }
F32     FSPanelFace::getCurrentShinyRot()            { return (F32)mCtrlShinyRot->getValue().asReal(); }
F32     FSPanelFace::getCurrentShinyScaleU()         { return (F32)mCtrlShinyScaleU->getValue().asReal(); }
F32     FSPanelFace::getCurrentShinyScaleV()         { return (F32)mCtrlShinyScaleV->getValue().asReal(); }
F32     FSPanelFace::getCurrentShinyOffsetU()        { return (F32)mCtrlShinyOffsetU->getValue().asReal(); }
F32     FSPanelFace::getCurrentShinyOffsetV()        { return (F32)mCtrlShinyOffsetV->getValue().asReal(); }

// <FS:CR> UI provided diffuse parameters
F32     FSPanelFace::getCurrentTextureRot()          { return (F32)mCtrlTexRot->getValue().asReal(); }
F32     FSPanelFace::getCurrentTextureScaleU()       { return (F32)mCtrlTexScaleU->getValue().asReal(); }
F32     FSPanelFace::getCurrentTextureScaleV()       { return (F32)mCtrlTexScaleV->getValue().asReal(); }
F32     FSPanelFace::getCurrentTextureOffsetU()      { return (F32)mCtrlTexOffsetU->getValue().asReal(); }
F32     FSPanelFace::getCurrentTextureOffsetV()      { return (F32)mCtrlTexOffsetV->getValue().asReal(); }
// </FS:CR>

LLRender::eTexIndex FSPanelFace::getTextureChannelToEdit()
{
    S32 matmedia_selection = getCurrentMaterialType();
    switch (matmedia_selection)
    {
        case MATMEDIA_MATERIAL:
        {
            return getCurrentMatChannel();
        }
        case MATMEDIA_PBR:
        {
            return getCurrentPBRChannel();
        }
    }
    return (LLRender::eTexIndex)0;
}

LLRender::eTexIndex FSPanelFace::getTextureDropChannel()
{
    if (getCurrentMaterialType() == MATMEDIA_MATERIAL)
    {
        return getCurrentMatChannel();
    }

    return (LLRender::eTexIndex)0;
}

LLGLTFMaterial::TextureInfo FSPanelFace::getPBRDropChannel()
{
    if (getCurrentMaterialType() == MATMEDIA_PBR)
    {
        LLRender::eTexIndex current_pbr_channel = getCurrentPBRChannel();
        if (current_pbr_channel == LLRender::BASECOLOR_MAP) return LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR;
        if (current_pbr_channel == LLRender::GLTF_NORMAL_MAP) return LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL;
        if (current_pbr_channel == LLRender::METALLIC_ROUGHNESS_MAP) return LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS;
        if (current_pbr_channel == LLRender::EMISSIVE_MAP) return LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE;
    }
    return (LLGLTFMaterial::TextureInfo)0;
}

LLMaterialPtr FSPanelFace::createDefaultMaterial(LLMaterialPtr current_material)
{
    LLMaterialPtr new_material(current_material.notNull() ? new LLMaterial(current_material->asLLSD()) : new LLMaterial());
    llassert_always(new_material);

    // Preserve old diffuse alpha mode or assert correct default blend mode as appropriate for the alpha channel content of the diffuse texture
    //
    new_material->setDiffuseAlphaMode(current_material.isNull() ? (isAlpha() ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE) : current_material->getDiffuseAlphaMode());
    return new_material;
}

void FSPanelFace::onVisibilityChange(bool new_visibility)
{
    if (new_visibility)
    {
        gAgent.showLatestFeatureNotification("gltf");
    }
    LLPanel::onVisibilityChange(new_visibility);
}

void FSPanelFace::draw()
{
    // TODO(Beq) why does it have to call applyTE?
    updateCopyTexButton();

    // grab media name/title and update the UI widget
    // Todo: move it, it's preferable not to update
    // labels inside draw
    updateMediaTitle();

    LLPanel::draw();

    // not sure if there isn't a better place for this, but for the time being this seems to work,
    // and trying other places always failed in some way or other -Zi
    static U64MicrosecondsImplicit next_update_time = 0LL;
    if ((mUnsavedChanges || mRevertedChanges) && gFrameTime > next_update_time)
    {
        LL_DEBUGS("APPLY_GLTF_CHANGES") << "detected unsaved changes: " << mUnsavedChanges << LL_ENDL;

        LLPointer<LLGLTFMaterial> mat = new LLGLTFMaterial();
        getGLTFMaterial(mat);

        LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();
        FSRenderMaterialOverrideFunctor override_func(mat, mUnsavedChanges, mRevertedChanges);
        selected_objects->applyToNodes(&override_func);

        LLGLTFMaterialList::flushUpdates();

        mUnsavedChanges = 0;
        mRevertedChanges = 0;

        next_update_time = gFrameTime + 100000LL;   // 100 ms

        // delete mat;  // do not delete mat, it's still referenced elsewhere (probably in the material list) -Zi
    }

    if (sMaterialOverrideSelection.update())
    {
        setMaterialOverridesFromSelection();
        updatePBROverrideDisplay();
    }
}

void FSPanelFace::sendTexture()
{
    if (!mTextureCtrl->getTentative())
    {
        // we grab the item id first, because we want to do a
        // permissions check in the selection manager. ARGH!
        LLUUID id = mTextureCtrl->getImageItemID();
        if (id.isNull())
        {
            id = mTextureCtrl->getImageAssetID();
        }
        if (!LLSelectMgr::getInstance()->selectionSetImage(id, getCurrentMaterialType()==MATMEDIA_PBR) )
        {
            // need to refresh value in texture ctrl
            refresh();
        }
    }
}

void FSPanelFace::sendBump(U32 bumpiness)
{
    if (bumpiness < BUMPY_TEXTURE)
    {
        LL_DEBUGS("Materials") << "clearing bumptexture control" << LL_ENDL;
        mBumpyTextureCtrl->clear();
        mBumpyTextureCtrl->setImageAssetID(LLUUID());
    }

    updateBumpyControls(bumpiness == BUMPY_TEXTURE, true);

    LLUUID current_normal_map = mBumpyTextureCtrl->getImageAssetID();

    U8 bump = (U8)bumpiness & TEM_BUMP_MASK;

    // Clear legacy bump to None when using an actual normal map
    //
    if (!current_normal_map.isNull())
    {
        bump = 0;
    }

    // Set the normal map or reset it to null as appropriate
    //
    LLSelectedTEMaterial::setNormalID(this, current_normal_map);
    LLSelectMgr::getInstance()->selectionSetBumpmap(bump, mBumpyTextureCtrl->getImageItemID());
}

void FSPanelFace::sendTexGen()
{
    U8 tex_gen = (U8) mComboTexGen->getCurrentIndex() << TEM_TEX_GEN_SHIFT;
    LLSelectMgr::getInstance()->selectionSetTexGen(tex_gen);
}

void FSPanelFace::sendShiny(U32 shininess)
{
    if (shininess < SHINY_TEXTURE)
    {
        mShinyTextureCtrl->clear();
        mShinyTextureCtrl->setImageAssetID(LLUUID());
    }

    LLUUID specmap = getCurrentSpecularMap();

    U8 shiny = (U8)shininess & TEM_SHINY_MASK;
    if (!specmap.isNull())
    {
        shiny = 0;
    }

    LLSelectedTEMaterial::setSpecularID(this, specmap);
    LLSelectMgr::getInstance()->selectionSetShiny(shiny, mShinyTextureCtrl->getImageItemID());

    updateShinyControls(!specmap.isNull(), true);
}

void FSPanelFace::sendFullbright()
{
    U8 fullbright = mCheckFullbright->get() ? TEM_FULLBRIGHT_MASK : 0;
    LLSelectMgr::getInstance()->selectionSetFullbright(fullbright);
}

void FSPanelFace::sendColor()
{
    LLSelectMgr::getInstance()->selectionSetColorOnly(mColorSwatch->get());
}

void FSPanelFace::sendAlpha()
{
    F32 alpha = (100.0f - mCtrlColorTransp->get()) / 100.0f;
    LLSelectMgr::getInstance()->selectionSetAlphaOnly(alpha);
}

void FSPanelFace::sendGlow()
{
    LLSelectMgr::getInstance()->selectionSetGlow(mCtrlGlow->get());
}

struct FSPanelFaceSetTEFunctor : public LLSelectedTEFunctor
{
    FSPanelFaceSetTEFunctor(FSPanelFace* panel) :
        mPanel(panel)
    {
    }

    virtual bool apply(LLViewerObject* object, S32 te)
    {
        std::string prefix;

        // Effectively the same as MATMEDIA_PBR - separate for the sake of clarity

        const std::string& tab_name = mPanel->mTabsMatChannel->getCurrentPanel()->getName();

        prefix = "Tex";
        if (tab_name == "panel_blinn_phong_normal")
        {
            prefix = "bumpy";
        }
        else if (tab_name == "panel_blinn_phong_specular")
        {
            prefix = "shiny";
        }

        LLSpinCtrl* ctrlTexScaleS = mPanel->findChild<LLSpinCtrl>(prefix + "ScaleU");
        LLSpinCtrl* ctrlTexScaleT = mPanel->findChild<LLSpinCtrl>(prefix + "ScaleV");
        LLSpinCtrl* ctrlTexOffsetS = mPanel->findChild<LLSpinCtrl>(prefix + "OffsetU");
        LLSpinCtrl* ctrlTexOffsetT = mPanel->findChild<LLSpinCtrl>(prefix + "OffsetV");
        LLSpinCtrl* ctrlTexRotation = mPanel->findChild<LLSpinCtrl>(prefix + "Rot");

        bool align_planar = mPanel->mCheckPlanarAlign->get();

        llassert(object);

        if (ctrlTexScaleS)
        {
            bool valid = !ctrlTexScaleS->getTentative();
            if (valid || align_planar)
            {
                F32 value = ctrlTexScaleS->get();
                if (mPanel->mComboTexGen->getCurrentIndex() == 1)
                {
                    value *= 0.5f;
                }
                object->setTEScaleS(te, value);

                if (align_planar)
                {
                    FSPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, value, te, object->getID());
                    FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, value, te, object->getID());
                }
            }
        }

        if (ctrlTexScaleT)
        {
            bool valid = !ctrlTexScaleT->getTentative();
            if (valid || align_planar)
            {
                F32 value = ctrlTexScaleT->get();
                if (mPanel->mComboTexGen->getCurrentIndex() == 1)
                {
                    value *= 0.5f;
                }
                object->setTEScaleT(te, value);

                if (align_planar)
                {
                    FSPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, value, te, object->getID());
                    FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, value, te, object->getID());
                }
            }
        }

        if (ctrlTexOffsetS)
        {
            bool valid = !ctrlTexOffsetS->getTentative();
            if (valid || align_planar)
            {
                F32 value = ctrlTexOffsetS->get();
                object->setTEOffsetS(te, value);

                if (align_planar)
                {
                    FSPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, value, te, object->getID());
                    FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, value, te, object->getID());
                }
            }
        }

        if (ctrlTexOffsetT)
        {
            bool valid = !ctrlTexOffsetT->getTentative();
            if (valid || align_planar)
            {
                F32 value = ctrlTexOffsetT->get();
                object->setTEOffsetT(te, value);

                if (align_planar)
                {
                    FSPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, value, te, object->getID());
                    FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, value, te, object->getID());
                }
            }
        }

        if (ctrlTexRotation)
        {
            bool valid = !ctrlTexRotation->getTentative();
            if (valid || align_planar)
            {
                F32 value = ctrlTexRotation->get() * DEG_TO_RAD;
                object->setTERotation(te, value);

                if (align_planar)
                {
                    FSPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, value, te, object->getID());
                    FSPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, value, te, object->getID());
                }
            }
        }
        return true;
    }
private:
    FSPanelFace* mPanel;
};

// Functor that aligns a face to mCenterFace
struct FSPanelFaceSetAlignedTEFunctor : public LLSelectedTEFunctor
{
    FSPanelFaceSetAlignedTEFunctor(FSPanelFace* panel, LLFace* center_face) :
        mPanel(panel),
        mCenterFace(center_face)
    {
    }

    virtual bool apply(LLViewerObject* object, S32 te)
    {
        LLFace* facep = object->mDrawable->getFace(te);
        if (!facep)
        {
            return true;
        }

        if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
        {
            return true;
        }

        bool set_aligned = true;
        if (facep == mCenterFace)
        {
            set_aligned = false;
        }
        if (set_aligned)
        {
            LLVector2 uv_offset, uv_scale;
            F32 uv_rot;
            set_aligned = facep->calcAlignedPlanarTE(mCenterFace, &uv_offset, &uv_scale, &uv_rot);
            if (set_aligned)
            {
                object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
                object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
                object->setTERotation(te, uv_rot);

                FSPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, uv_rot, te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, uv_rot, te, object->getID());

                FSPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());

                FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
            }
        }
        if (!set_aligned)
        {
            FSPanelFaceSetTEFunctor setfunc(mPanel);
            setfunc.apply(object, te);
        }
        return true;
    }
private:
    FSPanelFace* mPanel;
    LLFace* mCenterFace;
};

struct FSPanelFaceSetAlignedConcreteTEFunctor : public LLSelectedTEFunctor
{
    FSPanelFaceSetAlignedConcreteTEFunctor(FSPanelFace* panel, LLFace* center_face, LLRender::eTexIndex map) :
        mPanel(panel),
        mChefFace(center_face),
        mMap(map)
    {
    }

    virtual bool apply(LLViewerObject* object, S32 te)
    {
        LLFace* facep = object->mDrawable->getFace(te);
        if (!facep)
        {
            return true;
        }

        if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
        {
            return true;
        }

        if (mChefFace != facep)
        {
            LLVector2 uv_offset, uv_scale;
            F32 uv_rot;
            if (facep->calcAlignedPlanarTE(mChefFace, &uv_offset, &uv_scale, &uv_rot, mMap))
            {
                switch (mMap)
                {
                    case LLRender::DIFFUSE_MAP:
                        object->setTEOffset(te, uv_offset.mV[VX], uv_offset.mV[VY]);
                        object->setTEScale(te, uv_scale.mV[VX], uv_scale.mV[VY]);
                        object->setTERotation(te, uv_rot);
                        break;

                    case LLRender::NORMAL_MAP:
                        FSPanelFace::LLSelectedTEMaterial::setNormalRotation(mPanel, uv_rot, te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setNormalOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setNormalOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setNormalRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setNormalRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
                        break;

                    case LLRender::SPECULAR_MAP:
                        FSPanelFace::LLSelectedTEMaterial::setSpecularRotation(mPanel, uv_rot, te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetX(mPanel, uv_offset.mV[VX], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setSpecularOffsetY(mPanel, uv_offset.mV[VY], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatX(mPanel, uv_scale.mV[VX], te, object->getID());
                        FSPanelFace::LLSelectedTEMaterial::setSpecularRepeatY(mPanel, uv_scale.mV[VY], te, object->getID());
                        break;

                    default: /*make compiler happy*/
                        break;
                }
            }
        }

        return true;
    }

    private:
        FSPanelFace* mPanel;
        LLFace* mChefFace;
        LLRender::eTexIndex mMap;
};

// Functor that tests if a face is aligned to mCenterFace
struct FSPanelFaceGetIsAlignedTEFunctor : public LLSelectedTEFunctor
{
    FSPanelFaceGetIsAlignedTEFunctor(LLFace* center_face) :
        mCenterFace(center_face)
    {
    }

    virtual bool apply(LLViewerObject* object, S32 te)
    {
        LLFace* facep = object->mDrawable->getFace(te);
        if (!facep)
        {
            return false;
        }

        if (facep->getViewerObject()->getVolume()->getNumVolumeFaces() <= te)
        { //volume face does not exist, can't be aligned
            return false;
        }

        if (facep == mCenterFace)
        {
            return true;
        }

        LLVector2 aligned_st_offset, aligned_st_scale;
        F32 aligned_st_rot;
        if ( facep->calcAlignedPlanarTE(mCenterFace, &aligned_st_offset, &aligned_st_scale, &aligned_st_rot) )
        {
            const LLTextureEntry* tep = facep->getTextureEntry();
            LLVector2 st_offset, st_scale;
            tep->getOffset(&st_offset.mV[VX], &st_offset.mV[VY]);
            tep->getScale(&st_scale.mV[VX], &st_scale.mV[VY]);
            F32 st_rot = tep->getRotation();

            bool eq_offset_x = is_approx_equal_fraction(st_offset.mV[VX], aligned_st_offset.mV[VX], 12);
            bool eq_offset_y = is_approx_equal_fraction(st_offset.mV[VY], aligned_st_offset.mV[VY], 12);
            bool eq_scale_x  = is_approx_equal_fraction(st_scale.mV[VX], aligned_st_scale.mV[VX], 12);
            bool eq_scale_y  = is_approx_equal_fraction(st_scale.mV[VY], aligned_st_scale.mV[VY], 12);
            bool eq_rot      = is_approx_equal_fraction(st_rot, aligned_st_rot, 6);

            // needs a fuzzy comparison, because of fp errors
            if (eq_offset_x &&
                eq_offset_y &&
                eq_scale_x &&
                eq_scale_y &&
                eq_rot)
            {
                return true;
            }
        }
        return false;
    }
    private:
        LLFace* mCenterFace;
};

struct FSPanelFaceSendFunctor : public LLSelectedObjectFunctor
{
    virtual bool apply(LLViewerObject* object)
    {
        object->sendTEUpdate();
        return true;
    }
};

void FSPanelFace::sendTextureInfo()
{
    if (mCheckPlanarAlign->getValue().asBoolean())
    {
        LLFace* last_face = NULL;
        bool identical_face =false;
        LLSelectedTE::getFace(last_face, identical_face);
        FSPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
        LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
    }
    else
    {
        FSPanelFaceSetTEFunctor setfunc(this);
        LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
    }

    FSPanelFaceSendFunctor sendfunc;
    LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void FSPanelFace::alignTextureLayer()
{
    LLFace* last_face = NULL;
    bool identical_face = false;
    LLSelectedTE::getFace(last_face, identical_face);

    // TODO: make this respect PBR channels -Zi
    FSPanelFaceSetAlignedConcreteTEFunctor setfunc(this, last_face, getTextureChannelToEdit());
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
}

void FSPanelFace::getState()
{
    updateUI();
}

void FSPanelFace::updateUI(bool force_set_values /*false*/)
{ //set state of UI to match state of texture entry(ies)  (calls setEnabled, setValue, etc, but NOT setVisible)
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    LLViewerObject* objectp = node ? node->getObject() : NULL;

    if (objectp
        && objectp->getPCode() == LL_PCODE_VOLUME
        && objectp->permModify())
    {
        // TODO: Find out what "permanent" objects are supposed to allow to be edited. Right now this will
        //       completely blank out the texture panel, so we could just move that into the if() above -Zi
        bool editable = !objectp->isPermanentEnforced();
        bool attachment = objectp->isAttachment();

        // this object's faces can potentially be edited by us, so display the edit controls unless this is
        // a "permanent" pathfinding(?) object -Zi
        gSavedSettings.setBOOL("FSInternalCanEditObjectFaces", editable);
        LL_DEBUGS("ENABLEDISABLETOOLS") << "show face edit controls: " << editable << LL_ENDL;

        // TODO: editable always true here so we don't have to go through all the following code and strip it out
        //       until we know what the permanent thing abobe is supposed to do and we know if blanking out all
        //       controls has any unforseen side effects to editing. -Zi
        editable = true;

        bool has_pbr_material;
        bool has_faces_without_pbr;
        updateUIGLTF(objectp, has_pbr_material, has_faces_without_pbr, force_set_values);

        // TODO: if has_pbr_material AND has_faces_without_pbr give "Mixed Selection!" warning somehow

        mTabsPBRMatMedia->enableTabButton(
            mTabsPBRMatMedia->getIndexForPanel(
                mTabsPBRMatMedia->getPanelByName("panel_material_type_blinn_phong")), editable );

        // only turn on auto-adjust button if there is a media renderer and the media is loaded
        mBtnAlignMedia->setEnabled(editable);

        bool enable_material_controls = (!gSavedSettings.getBOOL("SyncMaterialSettings"));

        // *NOTE: The "identical" variable is currently only used to decide if
        // the texgen control should be tentative - this is not used by GLTF
        // materials. -Cosmic;2022-11-09
        bool identical         = true;  // true because it is anded below
        bool identical_diffuse = false;
        bool identical_norm    = false;
        bool identical_spec    = false;

        LLUUID id;
        LLUUID normmap_id;
        LLUUID specmap_id;

        LLSelectedTE::getTexId(id, identical_diffuse);
        LLSelectedTEMaterial::getNormalID(normmap_id, identical_norm);
        LLSelectedTEMaterial::getSpecularID(specmap_id, identical_spec);

        mTabsMatChannel->setEnabled(editable);
        mTabsPBRChannel->setEnabled(editable);

        mCheckSyncMaterials->setEnabled(editable);
        mCheckSyncMaterials->setValue(gSavedSettings.getBOOL("SyncMaterialSettings"));

        updateVisibility(objectp);

        LLColor4 color = LLColor4::white;
        bool identical_color = false;

        LLSelectedTE::getColor(color, identical_color);
        LLColor4 prev_color = mColorSwatch->get();

        mColorSwatch->setOriginal(color);
        mColorSwatch->set(color, force_set_values || (prev_color != color) || !editable);

        mColorSwatch->setValid(editable);
        mColorSwatch->setEnabled(editable);
        mColorSwatch->setCanApplyImmediately( editable );

        F32 transparency = (1.f - color.mV[VALPHA]) * 100.f;
        mCtrlColorTransp->setValue(editable ? transparency : 0);
        mCtrlColorTransp->setEnabled(editable );
        mColorTransPercent->setMouseOpaque(editable );

        // Water exclusion
        mExcludeWater = (id == IMG_ALPHA_GRAD) && normmap_id.isNull() && specmap_id.isNull() && (transparency == 0);
        {
            mCheckHideWater->setEnabled(editable && !has_pbr_material && !isMediaTexSelected());
            mCheckHideWater->set(mExcludeWater);
            editable &= !mExcludeWater;

            // disable controls for water exclusion face after updateVisibility, so the whole panel is not hidden
            // TODO
            // mComboMatMedia->setEnabled(editable);
            // mRadioMaterialType->setEnabled(editable);
            // mRadioPbrType->setEnabled(editable);
            // mCheckSyncSettings->setEnabled(editable);
        }

        // Shiny
        U8 shiny = 0;
        {
            bool identical_shiny = false;

            // Shiny - Legacy only, Blinn-Pbong specular is done further down
            LLSelectedTE::getShiny(shiny, identical_shiny);
            identical = identical && identical_shiny;

            shiny = specmap_id.isNull() ? shiny : SHINY_TEXTURE;

            mComboShininess->selectNthItem(shiny);
            mComboShininess->setEnabled(editable);
            mComboShininess->setTentative(!identical_shiny);
            mLabelShininess->setEnabled(editable);
        }

        // Bumpy
        U8 bumpy = 0;
        {
            bool identical_bumpy = false;
            LLSelectedTE::getBumpmap(bumpy, identical_bumpy);

            LLUUID norm_map_id = getCurrentNormalMap();

            bumpy = norm_map_id.isNull() ? bumpy : BUMPY_TEXTURE;

            mComboBumpiness->selectNthItem(bumpy);
            mComboBumpiness->setEnabled(editable);
            mComboBumpiness->setTentative(!identical_bumpy);
            mLabelBumpiness->setEnabled(editable);
        }

        // Texture
        {
            LLGLenum image_format = GL_RGB;
            bool identical_image_format = false;
            bool missing_asset = false;
            LLSelectedTE::getImageFormat(image_format, identical_image_format, missing_asset);

            if (!missing_asset)
            {
                mIsAlpha = false;
                switch (image_format)
                {
                    case GL_RGBA:
                    case GL_ALPHA:
                    {
                        mIsAlpha = true;
                    }
                    break;

                    case GL_RGB: break;
                    default:
                    {
                        LL_WARNS() << "Unexpected tex format in FSPanelFace, resorting to no alpha" << LL_ENDL;
                    }
                    break;
                }
            }
            else
            {
                // Don't know image's properties, use material's mode value
                mIsAlpha = true;
            }

            if (LLViewerMedia::getInstance()->textureHasMedia(id))
            {
                mBtnAlignMedia->setEnabled(editable);
            }

            if (identical_diffuse)
            {
                mTextureCtrl->setTentative(false);
                mTextureCtrl->setEnabled(editable);
                mTextureCtrl->setImageAssetID(id);

                bool can_change_alpha = editable && mIsAlpha && !missing_asset;
                mComboAlphaMode->setEnabled(can_change_alpha && transparency <= 0.f );
                mCtrlMaskCutoff->setEnabled(can_change_alpha);

                mTextureCtrl->setBakeTextureEnabled(true);
            }
            else if (id.isNull())
            {
                // None selected
                mTextureCtrl->setTentative(false);
                mTextureCtrl->setEnabled(false);
                mTextureCtrl->setImageAssetID(LLUUID::null);
                mComboAlphaMode->setEnabled(false);
                mCtrlMaskCutoff->setEnabled(false);

                mTextureCtrl->setBakeTextureEnabled(false);
            }
            else
            {
                // Tentative: multiple selected with different textures
                mTextureCtrl->setTentative(true);
                mTextureCtrl->setEnabled(editable );
                mTextureCtrl->setImageAssetID(id);

                bool can_change_alpha = editable && mIsAlpha && !missing_asset;
                mComboAlphaMode->setEnabled(can_change_alpha && transparency <= 0.f );
                mCtrlMaskCutoff->setEnabled(can_change_alpha);

                mTextureCtrl->setBakeTextureEnabled(true);
            }

            mShinyTextureCtrl->setTentative(!identical_spec);
            mShinyTextureCtrl->setEnabled(editable);
            mShinyTextureCtrl->setImageAssetID(specmap_id);

            mBumpyTextureCtrl->setTentative(!identical_norm);
            mBumpyTextureCtrl->setEnabled(editable );
            mBumpyTextureCtrl->setImageAssetID(normmap_id);

            if (attachment)
            {
                // attachments are in world and in inventory,
                // server doesn't support changing permissions
                // in such case
                mTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
                mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
                mShinyTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
            }
            else
            {
                mTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
                mBumpyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
                mShinyTextureCtrl->setImmediateFilterPermMask(PERM_NONE);
            }

            // Diffuse Alpha Mode

            // Init to the default that is appropriate for the alpha content of the asset
            //
            U8 alpha_mode = mIsAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

            bool identical_alpha_mode = false;

            // See if that's been overridden by a material setting for same...
            //
            LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(alpha_mode, identical_alpha_mode, mIsAlpha);

            // it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
            // Want masking? Want emissive? Tough! You get BLEND!
            alpha_mode = (transparency > 0.f) ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : alpha_mode;

            // ... unless there is no alpha channel in the texture, in which case alpha mode MUST be none
            alpha_mode = mIsAlpha ? alpha_mode : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

            mComboAlphaMode->selectNthItem(alpha_mode);

            updateAlphaControls();

            mLabelAlphaMode->setEnabled(mComboAlphaMode->getEnabled());
        }

        // planar align
        bool align_planar = mCheckPlanarAlign->get();
        bool identical_planar_aligned = false;
        {
            const bool texture_info_selected = (getCurrentMaterialType() == MATMEDIA_PBR && getCurrentPBRChannel() != LLRender::NUM_TEXTURE_CHANNELS);
            const bool enabled = (editable && isIdenticalPlanarTexgen() && !texture_info_selected);

            mCheckPlanarAlign->setValue(align_planar && enabled);
            mCheckPlanarAlign->setEnabled(enabled);
            mBtnAlignTextures->setEnabled(enabled && LLSelectMgr::getInstance()->getSelection()->getObjectCount() > 1);

            if (align_planar && enabled)
            {
                LLFace* last_face = nullptr;
                bool identical_face = false;
                LLSelectedTE::getFace(last_face, identical_face);

                FSPanelFaceGetIsAlignedTEFunctor get_is_aligend_func(last_face);
                // this will determine if the texture param controls are tentative:
                identical_planar_aligned = LLSelectMgr::getInstance()->getSelection()->applyToTEs(&get_is_aligend_func);
            }
        }

        // Needs to be public and before tex scale settings below to properly reflect
        // behavior when in planar vs default texgen modes in the
        // NORSPEC-84 et al
        //
        LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
        bool identical_texgen = true;
        bool identical_planar_texgen = false;

        {
            LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
            identical_planar_texgen = (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
        }

        // Texture scale
        {
            bool identical_diff_scale_s = false;
            bool identical_spec_scale_s = false;
            bool identical_norm_scale_s = false;

            identical = align_planar ? identical_planar_aligned : identical;

            F32 diff_scale_s = 1.f;
            F32 spec_scale_s = 1.f;
            F32 norm_scale_s = 1.f;

            LLSelectedTE::getScaleS(diff_scale_s, identical_diff_scale_s);
            LLSelectedTEMaterial::getSpecularRepeatX(spec_scale_s, identical_spec_scale_s);
            LLSelectedTEMaterial::getNormalRepeatX(norm_scale_s, identical_norm_scale_s);

            diff_scale_s = editable ? diff_scale_s : 1.0f;
            diff_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

            norm_scale_s = editable ? norm_scale_s : 1.0f;
            norm_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

            spec_scale_s = editable ? spec_scale_s : 1.0f;
            spec_scale_s *= identical_planar_texgen ? 2.0f : 1.0f;

            mCtrlTexScaleU->setValue(diff_scale_s);
            mCtrlShinyScaleU->setValue(spec_scale_s);
            mCtrlBumpyScaleU->setValue(norm_scale_s);

            // TODO: do we need to enable/disable all these manually anymore? -Zi
            /*
            mCtrlTexScaleU->setEnabled(editable && has_material);

            mCtrlShinyScaleU->setEnabled(editable && has_material && specmap_id.notNull() && enable_material_controls);
            mCtrlBumpyScaleU->setEnabled(editable && has_material && normmap_id.notNull() && enable_material_controls);
            childSetEnabled("flipTextureScaleSU", mCtrlShinyScaleU->getEnabled());
            childSetEnabled("flipTextureScaleNU", mCtrlBumpyScaleU->getEnabled());
            */

            bool diff_scale_tentative = !(identical && identical_diff_scale_s);
            bool norm_scale_tentative = !(identical && identical_norm_scale_s);
            bool spec_scale_tentative = !(identical && identical_spec_scale_s);

            mCtrlTexScaleU->setTentative(  LLSD(diff_scale_tentative));
            mCtrlShinyScaleU->setTentative(LLSD(spec_scale_tentative));
            mCtrlBumpyScaleU->setTentative(LLSD(norm_scale_tentative));

            mCheckSyncMaterials->setEnabled(editable && (specmap_id.notNull() || normmap_id.notNull()) && !align_planar);
        }

        {
            bool identical_diff_scale_t = false;
            bool identical_spec_scale_t = false;
            bool identical_norm_scale_t = false;

            F32 diff_scale_t = 1.f;
            F32 spec_scale_t = 1.f;
            F32 norm_scale_t = 1.f;

            LLSelectedTE::getScaleT(diff_scale_t, identical_diff_scale_t);
            LLSelectedTEMaterial::getSpecularRepeatY(spec_scale_t, identical_spec_scale_t);
            LLSelectedTEMaterial::getNormalRepeatY(norm_scale_t, identical_norm_scale_t);

            diff_scale_t = editable ? diff_scale_t : 1.0f;
            diff_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

            norm_scale_t = editable ? norm_scale_t : 1.0f;
            norm_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

            spec_scale_t = editable ? spec_scale_t : 1.0f;
            spec_scale_t *= identical_planar_texgen ? 2.0f : 1.0f;

            bool diff_scale_tentative = !identical_diff_scale_t;
            bool norm_scale_tentative = !identical_norm_scale_t;
            bool spec_scale_tentative = !identical_spec_scale_t;

            // TODO: do we need to enable/disable all these manually anymore? -Zi
            /*
            mCtrlTexScaleV->setEnabled(editable && has_material);

            mCtrlShinyScaleV->setEnabled(editable && has_material && specmap_id.notNull() && enable_material_controls);
            mCtrlBumpyScaleV->setEnabled(editable && has_material && normmap_id.notNull() && enable_material_controls);
            childSetEnabled("flipTextureScaleSV", mCtrlShinyScaleU->getEnabled());
            childSetEnabled("flipTextureScaleNV", mCtrlBumpyScaleU->getEnabled());
            */

            if (force_set_values)
            {
                mCtrlTexScaleV->forceSetValue(diff_scale_t);
            }
            else
            {
                mCtrlTexScaleV->setValue(diff_scale_t);
            }
            mCtrlShinyScaleV->setValue(norm_scale_t);
            mCtrlBumpyScaleV->setValue(spec_scale_t);

            mCtrlTexScaleV->setTentative(LLSD(diff_scale_tentative));
            mCtrlShinyScaleV->setTentative(LLSD(norm_scale_tentative));
            mCtrlBumpyScaleV->setTentative(LLSD(spec_scale_tentative));
        }

        // Texture offset
        {
            bool identical_diff_offset_s = false;
            bool identical_norm_offset_s = false;
            bool identical_spec_offset_s = false;

            F32 diff_offset_s = 0.0f;
            F32 norm_offset_s = 0.0f;
            F32 spec_offset_s = 0.0f;

            LLSelectedTE::getOffsetS(diff_offset_s, identical_diff_offset_s);
            LLSelectedTEMaterial::getNormalOffsetX(norm_offset_s, identical_norm_offset_s);
            LLSelectedTEMaterial::getSpecularOffsetX(spec_offset_s, identical_spec_offset_s);

            bool diff_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_s);
            bool norm_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_s);
            bool spec_offset_u_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_s);

            mCtrlTexOffsetU->setValue(  editable ? diff_offset_s : 0.0f);
            mCtrlBumpyOffsetU->setValue(editable ? norm_offset_s : 0.0f);
            mCtrlShinyOffsetU->setValue(editable ? spec_offset_s : 0.0f);

            mCtrlTexOffsetU->setTentative(LLSD(diff_offset_u_tentative));
            mCtrlBumpyOffsetU->setTentative(LLSD(norm_offset_u_tentative));
            mCtrlShinyOffsetU->setTentative(LLSD(spec_offset_u_tentative));

            // TODO: do we need to enable/disable all these manually anymore? -Zi
            /*
            mCtrlTexOffsetU->setEnabled(editable && has_material);

            mCtrlShinyOffsetU->setEnabled(editable && has_material && specmap_id.notNull() && enable_material_controls);
            mCtrlBumpyOffsetU->setEnabled(editable && has_material && normmap_id.notNull() && enable_material_controls);
            */
        }

        {
            bool identical_diff_offset_t = false;
            bool identical_norm_offset_t = false;
            bool identical_spec_offset_t = false;

            F32 diff_offset_t = 0.0f;
            F32 norm_offset_t = 0.0f;
            F32 spec_offset_t = 0.0f;

            LLSelectedTE::getOffsetT(diff_offset_t, identical_diff_offset_t);
            LLSelectedTEMaterial::getNormalOffsetY(norm_offset_t, identical_norm_offset_t);
            LLSelectedTEMaterial::getSpecularOffsetY(spec_offset_t, identical_spec_offset_t);

            bool diff_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_diff_offset_t);
            bool norm_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_norm_offset_t);
            bool spec_offset_v_tentative = !(align_planar ? identical_planar_aligned : identical_spec_offset_t);

            mCtrlTexOffsetV->setValue(  editable ? diff_offset_t : 0.0f);
            mCtrlBumpyOffsetV->setValue(editable ? norm_offset_t : 0.0f);
            mCtrlShinyOffsetV->setValue(editable ? spec_offset_t : 0.0f);

            mCtrlTexOffsetV->setTentative(LLSD(diff_offset_v_tentative));
            mCtrlBumpyOffsetV->setTentative(LLSD(norm_offset_v_tentative));
            mCtrlShinyOffsetV->setTentative(LLSD(spec_offset_v_tentative));

            // TODO: do we need to enable/disable all these manually anymore? -Zi
            /*
            mCtrlTexOffsetV->setEnabled(editable && has_material);

            mCtrlShinyOffsetV->setEnabled(editable && has_material && specmap_id.notNull() && enable_material_controls);
            mCtrlBumpyOffsetV->setEnabled(editable && has_material && normmap_id.notNull() && enable_material_controls);
            */
        }

        // Texture rotation
        {
            bool identical_diff_rotation = false;
            bool identical_norm_rotation = false;
            bool identical_spec_rotation = false;

            F32 diff_rotation = 0.f;
            F32 norm_rotation = 0.f;
            F32 spec_rotation = 0.f;

            LLSelectedTE::getRotation(diff_rotation,identical_diff_rotation);
            LLSelectedTEMaterial::getSpecularRotation(spec_rotation,identical_spec_rotation);
            LLSelectedTEMaterial::getNormalRotation(norm_rotation,identical_norm_rotation);

            bool diff_rot_tentative = !(align_planar ? identical_planar_aligned : identical_diff_rotation);
            bool norm_rot_tentative = !(align_planar ? identical_planar_aligned : identical_norm_rotation);
            bool spec_rot_tentative = !(align_planar ? identical_planar_aligned : identical_spec_rotation);

            F32 diff_rot_deg = diff_rotation * RAD_TO_DEG;
            F32 norm_rot_deg = norm_rotation * RAD_TO_DEG;
            F32 spec_rot_deg = spec_rotation * RAD_TO_DEG;

            // TODO: do we need to enable/disable all these manually anymore? -Zi
            /*
            mCtrlTexRot->setEnabled(editable && has_material);

            mCtrlShinyRot->setEnabled(editable && has_material && specmap_id.notNull() && enable_material_controls);
            mCtrlBumpyRot->setEnabled(editable && has_material && normmap_id.notNull() && enable_material_controls);
            */

            mCtrlTexRot->setTentative(LLSD(diff_rot_tentative));
            mCtrlBumpyRot->setTentative(LLSD(norm_rot_tentative));
            mCtrlShinyRot->setTentative(LLSD(spec_rot_tentative));

            mCtrlTexRot->setValue(  editable ? diff_rot_deg : 0.0f);
            mCtrlShinyRot->setValue(editable ? spec_rot_deg : 0.0f);
            mCtrlBumpyRot->setValue(editable ? norm_rot_deg : 0.0f);
        }

        {
            F32 glow = 0.f;
            bool identical_glow = false;
            LLSelectedTE::getGlow(glow,identical_glow);

            mCtrlGlow->setValue(glow);
            mCtrlGlow->setTentative(!identical_glow);
            mCtrlGlow->setEnabled(editable);
        }

        {
            // Maps from enum to combobox entry index
            mComboTexGen->selectNthItem(((S32)selected_texgen) >> 1);

            mComboTexGen->setEnabled(editable);
            mComboTexGen->setTentative(!identical);
        }

        {
            U8 fullbright_flag = 0;
            bool identical_fullbright = false;

            LLSelectedTE::getFullbright(fullbright_flag,identical_fullbright);

            mCheckFullbright->setValue((S32)(fullbright_flag != 0));
            mCheckFullbright->setEnabled(editable );
            mCheckFullbright->setTentative(!identical_fullbright);
        }

        // Repeats per meter
        {
            F32 repeats_diff = 1.f;
            F32 repeats_norm = 1.f;
            F32 repeats_spec = 1.f;

            F32 repeats_pbr_basecolor = 1.f;
            F32 repeats_pbr_metallic_roughness = 1.f;
            F32 repeats_pbr_normal = 1.f;
            F32 repeats_pbr_emissive = 1.f;

            bool identical_diff_repeats = false;
            bool identical_norm_repeats = false;
            bool identical_spec_repeats = false;

            bool identical_pbr_basecolor_repeats = false;
            bool identical_pbr_metallic_roughness_repeats = false;
            bool identical_pbr_normal_repeats = false;
            bool identical_pbr_emissive_repeats = false;

            S32 index = mComboTexGen->getCurrentIndex();
            bool enabled = editable && (index != 1);
            bool identical_repeats = true;

            S32 material_selection = getCurrentMaterialType();
            F32 repeats = 1.0f;

            LLRender::eTexIndex material_channel = LLRender::DIFFUSE_MAP;
            if (material_selection != MATMEDIA_PBR)
            {
                material_channel = getCurrentMatChannel();
                LLSelectedTE::getMaxDiffuseRepeats(repeats_diff, identical_diff_repeats);
                LLSelectedTEMaterial::getMaxNormalRepeats(repeats_norm, identical_norm_repeats);
                LLSelectedTEMaterial::getMaxSpecularRepeats(repeats_spec, identical_spec_repeats);
            }
            else if (material_selection == MATMEDIA_PBR)
            {
                enabled = editable;
                material_channel = getCurrentPBRChannel();

                getSelectedGLTFMaterialMaxRepeats(LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_BASE_COLOR,
                                                  repeats_pbr_basecolor, identical_pbr_basecolor_repeats);
                getSelectedGLTFMaterialMaxRepeats(LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS,
                                                  repeats_pbr_metallic_roughness, identical_pbr_metallic_roughness_repeats);
                getSelectedGLTFMaterialMaxRepeats(LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_NORMAL,
                                                  repeats_pbr_normal, identical_pbr_normal_repeats);
                getSelectedGLTFMaterialMaxRepeats(LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_EMISSIVE,
                                                  repeats_pbr_emissive, identical_pbr_emissive_repeats);
            }

            switch (material_channel)
            {
                default:
                case LLRender::DIFFUSE_MAP:
                {
                    if (material_selection != MATMEDIA_PBR)
                    {
                        enabled = editable && !id.isNull();
                    }
                    identical_repeats = identical_diff_repeats;
                    repeats = repeats_diff;
                }
                break;

                case LLRender::SPECULAR_MAP:
                {
                    if (material_selection != MATMEDIA_PBR)
                    {
                        enabled = (editable && ((shiny == SHINY_TEXTURE) && !specmap_id.isNull())
                                    && enable_material_controls);   // <FS:CR> Materials Alignment
                    }
                    identical_repeats = identical_spec_repeats;
                    repeats = repeats_spec;
                }
                break;

                case LLRender::NORMAL_MAP:
                {
                    if (material_selection != MATMEDIA_PBR)
                    {
                        enabled = (editable && ((bumpy == BUMPY_TEXTURE) && !normmap_id.isNull())
                                    && enable_material_controls);   // <FS:CR> Materials Alignment
                    }
                    identical_repeats = identical_norm_repeats;
                    repeats = repeats_norm;
                }
                break;

                case LLRender::NUM_TEXTURE_CHANNELS:
                case LLRender::BASECOLOR_MAP:
                {
                    identical_repeats = identical_pbr_basecolor_repeats;
                    repeats = repeats_pbr_basecolor;
                }
                break;

                case LLRender::METALLIC_ROUGHNESS_MAP:
                {
                    identical_repeats = identical_pbr_metallic_roughness_repeats;
                    repeats = repeats_pbr_metallic_roughness;
                }
                break;

                case LLRender::GLTF_NORMAL_MAP:
                {
                    identical_repeats = identical_pbr_normal_repeats;
                    repeats = repeats_pbr_normal;
                }
                break;

                case LLRender::EMISSIVE_MAP:
                {
                    identical_repeats = identical_pbr_emissive_repeats;
                    repeats = repeats_pbr_emissive;
                }
                break;
            }

            bool repeats_tentative = !identical_repeats;

            if (force_set_values || material_selection == MATMEDIA_PBR)
            {
                //onCommit, previosly edited element updates related ones
                mCtrlRpt->forceSetValue(editable ? repeats : 1.0f);
            }
            else
            {
                mCtrlRpt->setValue(editable ? repeats : 1.0f);
            }
            mCtrlRpt->setTentative(LLSD(repeats_tentative));
            mCtrlRpt->setEnabled(!identical_planar_texgen && enabled);
        }

        // Blinn-Phong Materials
        {
            LLMaterialPtr material;
            LLSelectedTEMaterial::getCurrent(material, identical);

            mComboShininess->setTentative(!identical_spec);
            mCtrlGlossiness->setTentative(!identical_spec);
            mCtrlEnvironment->setTentative(!identical_spec);
            mShinyColorSwatch->setTentative(!identical_spec);

            if (material && editable)
            {
                LL_DEBUGS("Materials") << material->asLLSD() << LL_ENDL;

                // Alpha
                U32 alpha_mode = material->getDiffuseAlphaMode();

                if (transparency > 0.f)
                { //it is invalid to have any alpha mode other than blend if transparency is greater than zero ...
                    alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
                }

                if (!mIsAlpha)
                { // ... unless there is no alpha channel in the texture, in which case alpha mode MUST be none
                    alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
                }

                mComboAlphaMode->selectNthItem(alpha_mode);

                mCtrlMaskCutoff->setValue(material->getAlphaMaskCutoff());

                updateAlphaControls();

                identical_planar_texgen = isIdenticalPlanarTexgen();

                // Shiny (specular)
                F32 offset_x, offset_y, repeat_x, repeat_y, rot;

                mShinyTextureCtrl->setImageAssetID(material->getSpecularID());

                if (material->getSpecularID().notNull() && (shiny == SHINY_TEXTURE))
                {
                    material->getSpecularOffset(offset_x,offset_y);
                    material->getSpecularRepeat(repeat_x,repeat_y);

                    if (identical_planar_texgen)
                    {
                        repeat_x *= 2.0f;
                        repeat_y *= 2.0f;
                    }

                    rot = material->getSpecularRotation();
                    mCtrlShinyScaleU->setValue(repeat_x);
                    mCtrlShinyScaleV->setValue(repeat_y);
                    mCtrlShinyRot->setValue(rot*RAD_TO_DEG);
                    mCtrlShinyOffsetU->setValue(offset_x);
                    mCtrlShinyOffsetV->setValue(offset_y);
                    mCtrlGlossiness->setValue(material->getSpecularLightExponent());
                    mCtrlEnvironment->setValue(material->getEnvironmentIntensity());

                    updateShinyControls(!material->getSpecularID().isNull(), true);
                }

                // Assert desired colorswatch color to match material AFTER updateShinyControls
                // to avoid getting overwritten with the default on some UI state changes.
                //
                if (material->getSpecularID().notNull())
                {
                    LLColor4 new_color = material->getSpecularLightColor();
                    LLColor4 old_color = mShinyColorSwatch->get();

                    mShinyColorSwatch->setOriginal(new_color);
                    mShinyColorSwatch->set(new_color, force_set_values || old_color != new_color || !editable);
                }

                // Bumpy (normal)
                mBumpyTextureCtrl->setImageAssetID(material->getNormalID());

                if (material->getNormalID().notNull())
                {
                    material->getNormalOffset(offset_x,offset_y);
                    material->getNormalRepeat(repeat_x,repeat_y);

                    if (identical_planar_texgen)
                    {
                        repeat_x *= 2.0f;
                        repeat_y *= 2.0f;
                    }

                    rot = material->getNormalRotation();
                    mCtrlBumpyScaleU->setValue(repeat_x);
                    mCtrlBumpyScaleV->setValue(repeat_y);
                    mCtrlBumpyRot->setValue(rot*RAD_TO_DEG);
                    mCtrlBumpyOffsetU->setValue(offset_x);
                    mCtrlBumpyOffsetV->setValue(offset_y);

                    updateBumpyControls(!material->getNormalID().isNull(), true);
                }
            }
        }

        S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();
        bool single_volume = (selected_count == 1);
        mBtnCopyFaces->setEnabled(editable && single_volume);
        mBtnPasteFaces->setEnabled(editable && !mClipboardParams.emptyMap() && (mClipboardParams.has("color") || mClipboardParams.has("texture")));

        // Set variable values for numeric expressions
        LLCalc* calcp = LLCalc::getInstance();
        calcp->setVar(LLCalc::TEX_U_SCALE, getCurrentTextureScaleU());
        calcp->setVar(LLCalc::TEX_V_SCALE, getCurrentTextureScaleV());
        calcp->setVar(LLCalc::TEX_U_OFFSET, getCurrentTextureOffsetU());
        calcp->setVar(LLCalc::TEX_V_OFFSET, getCurrentTextureOffsetV());
        calcp->setVar(LLCalc::TEX_ROTATION, getCurrentTextureRot());
        calcp->setVar(LLCalc::TEX_TRANSPARENCY, (F32)mCtrlColorTransp->getValue().asReal());
        calcp->setVar(LLCalc::TEX_GLOW, (F32)mCtrlGlow->getValue().asReal());

        // Find all faces with same texture
        // TODO: these were not yet added to the new texture panel -Zi
        /*
        getChild<LLUICtrl>("btn_select_same_diff")->setEnabled(LLSelectMgr::getInstance()->getTEMode() && mTextureCtrl->getEnabled());
        getChild<LLUICtrl>("btn_select_same_norm")->setEnabled(LLSelectMgr::getInstance()->getTEMode() && mBumpyTextureCtrl->getEnabled());
        getChild<LLUICtrl>("btn_select_same_spec")->setEnabled(LLSelectMgr::getInstance()->getTEMode() && mShinyTextureCtrl->getEnabled());
        */
    }
    else
    {
        // we can not edit this object's faces or there is no object selected, so
        // we can just as well blank out the whole thing. -Zi
        gSavedSettings.setBOOL("FSInternalCanEditObjectFaces", false);
        LL_DEBUGS("ENABLEDISABLETOOLS") << "hide face edit controls" << LL_ENDL;
    }
}

// One-off listener that updates the build floater UI when the agent inventory adds or removes an item
// TODO: CHECK - Why do we even have this? -Zi
class PBRPickerAgentListener : public LLInventoryObserver
{
protected:
    bool mChangePending = true;
public:
        PBRPickerAgentListener() : LLInventoryObserver()
    {
        gInventory.addObserver(this);
    }

    const bool isListening()
    {
        return mChangePending;
    }

        void changed(U32 mask) override
    {
        if (!(mask & (ADD | REMOVE)))
        {
            return;
        }

        if (gFloaterTools)
        {
            gFloaterTools->dirty();
        }
        gInventory.removeObserver(this);
        mChangePending = false;
    }

    ~PBRPickerAgentListener() override
    {
        gInventory.removeObserver(this);
        mChangePending = false;
    }
};

// One-off listener that updates the build floater UI when the prim inventory updates
// TODO: CHECK - Why do we even have this? -Zi
class PBRPickerObjectListener : public LLVOInventoryListener
{
    protected:
        LLViewerObject* mObjectp;
        bool mChangePending = true;

    public:
        PBRPickerObjectListener(LLViewerObject* object)
        : mObjectp(object)
        {
            registerVOInventoryListener(mObjectp, nullptr);
        }

        const bool isListeningFor(const LLViewerObject* objectp) const
        {
            return mChangePending && (objectp == mObjectp);
        }

        void inventoryChanged(LLViewerObject* object,
            LLInventoryObject::object_list_t* inventory,
            S32 serial_num,
            void* user_data) override
        {
            if (gFloaterTools)
            {
                gFloaterTools->dirty();
            }
            removeVOInventoryListener();
            mChangePending = false;
        }

        ~PBRPickerObjectListener()
        {
            removeVOInventoryListener();
            mChangePending = false;
        }
};

void FSPanelFace::updateUIGLTF(LLViewerObject* objectp, bool& has_pbr_material, bool& has_faces_without_pbr, bool force_set_values)
{
    const bool has_pbr_capabilities = LLMaterialEditor::capabilitiesAvailable();

    mTabsPBRMatMedia->enableTabButton(
        mTabsPBRMatMedia->getIndexForPanel(
            mTabsPBRMatMedia->getPanelByName("panel_material_type_pbr")), has_pbr_capabilities);
    LL_DEBUGS("ENABLEDISABLETOOLS") << "panel_material_type_pbr " << has_pbr_capabilities << LL_ENDL;

    LLSelectedTEGetmatId func;
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&func);

    has_pbr_material = func.mHasFacesWithPBR;
    has_faces_without_pbr = func.mHasFacesWithoutPBR;

    // this could just be saved in the class instead of fetching it again after updateUI() already did -Zi
    const bool settable = has_pbr_capabilities && objectp->permModify() && !objectp->isPermanentEnforced(); // do not depend on non-PBR here so we can turn any face into PBR -Zi
    const bool editable = LLMaterialEditor::canModifyObjectsMaterial() && !has_faces_without_pbr;
    const bool saveable = LLMaterialEditor::canSaveObjectsMaterial() && !has_faces_without_pbr;

    mMaterialCtrlPBR->setEnabled(settable);
    mMaterialCtrlPBR->setTentative(!func.mIdenticalMaterial);
    mMaterialCtrlPBR->setImageAssetID(func.mMaterialId);
    LL_DEBUGS("ENABLEDISABLETOOLS") << "mMaterialCtrlPBR " << settable << LL_ENDL;

    LL_DEBUGS("ENABLEDISABLETOOLS") << "PBR controls overall:  " << editable << LL_ENDL;

    mBaseTexturePBR->setEnabled(editable);
    mBaseTexturePBR->setImageAssetID(editable ? func.mMaterialSummary.mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] : LLUUID::null);
    mBaseTexturePBR->setTentative(!func.mIdenticalMap[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR]);

    mBaseTintPBR->setEnabled(editable);

    mBaseTintPBR->set(srgbColor4(func.mMaterialSummary.mBaseColor));
    mBaseTintPBR->setTentative(!func.mIdenticalBaseColor);          // TODO: split alpha from color? -Zi
    mBaseTintPBR->setValid(editable);
    LL_DEBUGS("ENABLEDISABLETOOLS") << "mBaseTintPBR valid " << editable << LL_ENDL;
    mBaseTintPBR->setFallbackImage(LLUI::getUIImage("locked_image.j2c") );

    mNormalTexturePBR->setEnabled(editable);
    mNormalTexturePBR->setImageAssetID(editable ? func.mMaterialSummary.mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL] : LLUUID::null);
    mNormalTexturePBR->setTentative(!func.mIdenticalMap[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL]);

    mORMTexturePBR->setEnabled(editable);
    mORMTexturePBR->setImageAssetID(editable ? func.mMaterialSummary.mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS] : LLUUID::null);
    mORMTexturePBR->setTentative(!func.mIdenticalMap[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS]);

    mEmissiveTexturePBR->setEnabled(editable);
    mEmissiveTexturePBR->setImageAssetID(editable ? func.mMaterialSummary.mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE] : LLUUID::null);
    mEmissiveTexturePBR->setTentative(!func.mIdenticalMap[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE]);

    mEmissiveTintPBR->setEnabled(editable);
    mEmissiveTintPBR->set(srgbColor4(func.mMaterialSummary.mEmissiveColor));
    mEmissiveTintPBR->setTentative(!func.mIdenticalEmissive);
    mEmissiveTintPBR->setValid(editable);
    LL_DEBUGS("ENABLEDISABLETOOLS") << "mEmissiveTintPBR valid " << editable << LL_ENDL;
    mEmissiveTintPBR->setFallbackImage(LLUI::getUIImage("locked_image.j2c") );

    mCheckDoubleSidedPBR->setEnabled(editable);
    mCheckDoubleSidedPBR->setValue(func.mMaterialSummary.mDoubleSided);
    mCheckDoubleSidedPBR->setTentative(!func.mIdenticalDoubleSided);

    mAlphaPBR->setEnabled(editable);
    mAlphaPBR->setValue(func.mMaterialSummary.mBaseColor.mV[VALPHA]);
    mAlphaPBR->setTentative(!func.mIdenticalBaseColor);         // TODO: split alpha from color? -Zi

    mAlphaModePBR->setEnabled(editable);
    mAlphaModePBR->setValue(func.mMaterialSummary.getAlphaMode());
    mAlphaModePBR->setTentative(!func.mIdenticalAlphaMode);
    mLabelAlphaModePBR->setEnabled(editable);

    mMaskCutoffPBR->setEnabled(editable);
    mMaskCutoffPBR->setValue(func.mMaterialSummary.mAlphaCutoff);
    mMaskCutoffPBR->setTentative(!func.mIdenticalAlphaCutoff);

    mMetallicFactorPBR->setEnabled(editable);
    mMetallicFactorPBR->setValue(func.mMaterialSummary.mMetallicFactor);
    mMetallicFactorPBR->setTentative(!func.mIdenticalMetallic);

    mRoughnessFactorPBR->setEnabled(editable);
    mRoughnessFactorPBR->setValue(func.mMaterialSummary.mRoughnessFactor);
    mRoughnessFactorPBR->setTentative(!func.mIdenticalRoughness);

    if (func.mMaterial)
    {
        mPBRBaseMaterialParams.mMap = func.mMaterial->mTextureId;
        mPBRBaseMaterialParams.mBaseColorTint = srgbColor4(func.mMaterial->mBaseColor);
        mPBRBaseMaterialParams.mMetallic = func.mMaterial->mMetallicFactor;
        mPBRBaseMaterialParams.mRoughness = func.mMaterial->mRoughnessFactor;
        mPBRBaseMaterialParams.mEmissiveTint = func.mMaterial->mEmissiveColor;
        mPBRBaseMaterialParams.mAlphaMode = func.mMaterial->mAlphaMode;
        mPBRBaseMaterialParams.mAlphaCutoff = func.mMaterial->mAlphaCutoff;
        mPBRBaseMaterialParams.mDoubleSided = func.mMaterial->mDoubleSided;
    }

    if (objectp->isAttachment())
    {
        // TODO: coming in from latest patch, seems broken?
        mMaterialCtrlPBR->setFilterPermissionMasks(PERM_COPY | PERM_TRANSFER | PERM_MODIFY);
        // TODO: before:
        // mMaterialCtrlPBR->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER | PERM_MODIFY);
    }
    else
    {
        mMaterialCtrlPBR->setImmediateFilterPermMask(PERM_NONE);
    }

    mBtnSavePBR->setEnabled(saveable); // TODO: flag for "something is not identical" -Zi     && pbr_identical);
    LL_DEBUGS("ENABLEDISABLETOOLS") << "mBtnSavePBR " << saveable << LL_ENDL;

    // TODO: CHECK - Find out if we need this at all -Zi
    if (objectp->isInventoryPending())
    {
        // Reuse the same listener when possible
        if (!mVOInventoryListener || !mVOInventoryListener->isListeningFor(objectp))
        {
            mVOInventoryListener = std::make_unique<PBRPickerObjectListener>(objectp);
        }
    }
    else
    {
        mVOInventoryListener = nullptr;
    }
    if (!func.mIdenticalMaterial || func.mMaterialId.isNull() || func.mMaterialId == BLANK_MATERIAL_ASSET_ID)
    {
        mAgentInventoryListener = nullptr;
    }
    else
    {
        if (!mAgentInventoryListener || !mAgentInventoryListener->isListening())
        {
            mAgentInventoryListener = std::make_unique<PBRPickerAgentListener>();
        }
    }

    // Control values will be set once per frame in setMaterialOverridesFromSelection
    sMaterialOverrideSelection.setDirty();
}

void FSPanelFace::updateCopyTexButton()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

    mBtnCopyFaces->setEnabled(objectp && objectp->getPCode() == LL_PCODE_VOLUME && objectp->permModify()
        && !objectp->isPermanentEnforced() && !objectp->isInventoryPending()
        && (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1)
        && LLMaterialEditor::canClipboardObjectsMaterial()
        && !mExcludeWater
    );
    std::string tooltip = (objectp && objectp->isInventoryPending()) ? LLTrans::getString("LoadingContents") : getString("paste_options");
    mBtnCopyFaces->setToolTip(tooltip);
}

void FSPanelFace::refresh()
{
    LL_DEBUGS("Materials") << LL_ENDL;
    getState();
}

void FSPanelFace::refreshMedia()
{
    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();
    LLViewerObject* first_object = selected_objects->getFirstObject();

    if (!(first_object
        && first_object->getPCode() == LL_PCODE_VOLUME
        && first_object->permModify()
        ))
    {
        mBtnAddMedia->setEnabled(false);
        mTitleMediaText->clear();
        clearMediaSettings();
        return;
    }

    std::string url = first_object->getRegion()->getCapability("ObjectMedia");
    bool has_media_capability = (!url.empty());

    if (!has_media_capability)
    {
        mBtnAddMedia->setEnabled(false);
        LL_WARNS("LLFloaterToolsMedia") << "Media not enabled (no capability) in this region!" << LL_ENDL;
        clearMediaSettings();
        return;
    }

    bool is_nonpermanent_enforced = (LLSelectMgr::getInstance()->getSelection()->getFirstRootNode()
        && LLSelectMgr::getInstance()->selectGetRootsNonPermanentEnforced())
        || LLSelectMgr::getInstance()->selectGetNonPermanentEnforced();
    bool editable = is_nonpermanent_enforced && (first_object->permModify() || selectedMediaEditable());

    // Check modify permissions and whether any selected objects are in
    // the process of being fetched. If they are, then we're not editable
    if (editable)
    {
        LLObjectSelection::iterator iter = selected_objects->begin();
        LLObjectSelection::iterator end = selected_objects->end();
        for (; iter != end; ++iter)
        {
            LLSelectNode* node = *iter;
            LLVOVolume* object = dynamic_cast<LLVOVolume*>(node->getObject());
            if (object && !object->permModify())
            {
                LL_INFOS("LLFloaterToolsMedia")
                    << "Selection not editable due to lack of modify permissions on object id "
                    << object->getID() << LL_ENDL;

                editable = false;
                break;
            }
        }
    }

    // Media settings
    bool bool_has_media = false;
    struct media_functor : public LLSelectedTEGetFunctor<bool>
    {
        bool get(LLViewerObject* object, S32 face)
        {
            LLTextureEntry *te = object->getTE(face);
            if (te)
            {
                return te->hasMedia();
            }
            return false;
        }
    } func;

    // check if all faces have media (or, all don't have media)
    LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo = selected_objects->getSelectedTEValue(&func, bool_has_media);

    const LLMediaEntry default_media_data;

    struct functor_getter_media_data : public LLSelectedTEGetFunctor< LLMediaEntry>
    {
        functor_getter_media_data(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        LLMediaEntry get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return *(object->getTE(face)->getMediaData());
            }
            return mMediaEntry;
        };

        const LLMediaEntry& mMediaEntry;

    } func_media_data(default_media_data);

    LLMediaEntry media_data_get;
    LLFloaterMediaSettings::getInstance()->mMultipleMedia = !(selected_objects->getSelectedTEValue(&func_media_data, media_data_get));

    std::string multi_media_info_str = LLTrans::getString("Multiple Media");
    std::string media_title = "";

    // update UI depending on whether "object" (prim or face) has media
    // and whether or not you are allowed to edit it.

    mBtnAddMedia->setEnabled(editable);

    // IF all the faces have media (or all don't have media)
    if (LLFloaterMediaSettings::getInstance()->mIdenticalHasMediaInfo)
    {
        // TODO: get media title and set it.
        mTitleMediaText->clear();

        // if identical is set, all faces are same (whether all empty or has the same media)
        if (!(LLFloaterMediaSettings::getInstance()->mMultipleMedia))
        {
            // media data is valid
            if (media_data_get != default_media_data)
            {
                // initial media title is the media URL (until we get the name)
                media_title = media_data_get.getHomeURL();
            }
            // else all faces might be empty
        }
        else // there are Different media been set on on the faces
        {
            media_title = multi_media_info_str;
        }

        mBtnDeleteMedia->setEnabled(bool_has_media && editable);

        // TODO: display a list of all media on the face - use 'identical' flag
    }
    else // not all faces have media but at least one does
    {
        // selected faces have not identical value
        LLFloaterMediaSettings::getInstance()->mMultipleValidMedia = selected_objects->isMultipleTEValue(&func_media_data, default_media_data);

        if (LLFloaterMediaSettings::getInstance()->mMultipleValidMedia)
        {
            media_title = multi_media_info_str;
        }
        else
        {
            // Media data is valid
            if (media_data_get != default_media_data)
            {
                // initial media title is the media URL (until we get the name)
                media_title = media_data_get.getHomeURL();
            }
        }

        mBtnDeleteMedia->setEnabled(true);
    }

    S32 materials_media = getCurrentMaterialType();
    if (materials_media == MATMEDIA_MEDIA)
    {
        // currently displaying media info, navigateTo and update title
        navigateToTitleMedia(media_title);
    }
    else
    {
        // Media can be heavy, don't keep it around
        // MAC specific: MAC doesn't support setVolume(0) so if not
        // unloaded, it might keep playing audio until user closes editor
        unloadMedia();
        mNeedMediaTitle = false;
    }

    mTitleMediaText->setText(media_title);

    // load values for media settings
    updateMediaSettings();

    LLFloaterMediaSettings::initValues(mMediaSettings, editable);
}

void FSPanelFace::unloadMedia()
{
    // destroy media source used to grab media title
    mTitleMedia->unloadMediaSource();
}

void FSPanelFace::onMaterialOverrideReceived(const LLUUID& object_id, S32 side)
{
    sMaterialOverrideSelection.onSelectedObjectUpdated(object_id, side);
}

//////////////////////////////////////////////////////////////////////////////
//
void FSPanelFace::navigateToTitleMedia(const std::string url)
{
    std::string multi_media_info_str = LLTrans::getString("Multiple Media");
    if (url.empty() || multi_media_info_str == url)
    {
        // nothing to show
        mNeedMediaTitle = false;
    }
    else
    {
        LLPluginClassMedia* media_plugin = mTitleMedia->getMediaPlugin();

        // check if url changed or if we need a new media source
        if (mTitleMedia->getCurrentNavUrl() != url || media_plugin == nullptr)
        {
            mTitleMedia->navigateTo(url);

            LLViewerMediaImpl* impl = LLViewerMedia::getInstance()->getMediaImplFromTextureID(mTitleMedia->getTextureID());
            if (impl)
            {
                // if it's a page with a movie, we don't want to hear it
                impl->setVolume(0);
            }
        }

        // flag that we need to update the title (even if no request were made)
        mNeedMediaTitle = true;
    }
}

bool FSPanelFace::selectedMediaEditable()
{
    U32 owner_mask_on;
    U32 owner_mask_off;
    U32 valid_owner_perms = LLSelectMgr::getInstance()->selectGetPerm(PERM_OWNER,
        &owner_mask_on, &owner_mask_off);
    U32 group_mask_on;
    U32 group_mask_off;
    U32 valid_group_perms = LLSelectMgr::getInstance()->selectGetPerm(PERM_GROUP,
        &group_mask_on, &group_mask_off);
    U32 everyone_mask_on;
    U32 everyone_mask_off;
    S32 valid_everyone_perms = LLSelectMgr::getInstance()->selectGetPerm(PERM_EVERYONE,
        &everyone_mask_on, &everyone_mask_off);

    bool selected_Media_editable = false;

    // if perms we got back are valid
    if (valid_owner_perms &&
        valid_group_perms &&
        valid_everyone_perms)
    {
        if ((owner_mask_on & PERM_MODIFY) ||
            (group_mask_on & PERM_MODIFY) ||
            (everyone_mask_on & PERM_MODIFY))
        {
            selected_Media_editable = true;
        }
        else
        {
            // user is NOT allowed to press the RESET button
            selected_Media_editable = false;
        };
    };

    return selected_Media_editable;
}

void FSPanelFace::clearMediaSettings()
{
    LLFloaterMediaSettings::clearValues(false);
}

void FSPanelFace::updateMediaSettings()
{
    bool identical(false);
    std::string base_key("");
    std::string value_str("");
    int value_int = 0;
    bool value_bool = false;
    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();
    // TODO: (CP) refactor this using something clever or boost or both !!

    const LLMediaEntry default_media_data;

    // controls
    U8 value_u8 = default_media_data.getControls();
    struct functor_getter_controls : public LLSelectedTEGetFunctor<U8>
    {
        functor_getter_controls(const LLMediaEntry &entry) : mMediaEntry(entry) {}

        U8 get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getControls();
            }
            return mMediaEntry.getControls();
        };

        const LLMediaEntry &mMediaEntry;

    } func_controls(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_controls, value_u8);
    base_key = std::string(LLMediaEntry::CONTROLS_KEY);
    mMediaSettings[base_key] = value_u8;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // First click (formerly left click)
    value_bool = default_media_data.getFirstClickInteract();
    struct functor_getter_first_click : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_first_click(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getFirstClickInteract();
            }
            return mMediaEntry.getFirstClickInteract();
        };

        const LLMediaEntry &mMediaEntry;
    } func_first_click(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_first_click, value_bool);
    base_key = std::string(LLMediaEntry::FIRST_CLICK_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Home URL
    value_str = default_media_data.getHomeURL();
    struct functor_getter_home_url : public LLSelectedTEGetFunctor<std::string>
    {
        functor_getter_home_url(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        std::string get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getHomeURL();
            }
            return mMediaEntry.getHomeURL();
        };

        const LLMediaEntry &mMediaEntry;

    } func_home_url(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_home_url, value_str);
    base_key = std::string(LLMediaEntry::HOME_URL_KEY);
    mMediaSettings[base_key] = value_str;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Current URL
    value_str = default_media_data.getCurrentURL();
    struct functor_getter_current_url : public LLSelectedTEGetFunctor<std::string>
    {
        functor_getter_current_url(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        std::string get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getCurrentURL();
            }
            return mMediaEntry.getCurrentURL();
        };

        const LLMediaEntry &mMediaEntry;
    } func_current_url(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_current_url, value_str);
    base_key = std::string(LLMediaEntry::CURRENT_URL_KEY);
    mMediaSettings[base_key] = value_str;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Auto zoom
    value_bool = default_media_data.getAutoZoom();
    struct functor_getter_auto_zoom : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_auto_zoom(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getAutoZoom();
            }
            return mMediaEntry.getAutoZoom();
        };

        const LLMediaEntry &mMediaEntry;
    } func_auto_zoom(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_zoom, value_bool);
    base_key = std::string(LLMediaEntry::AUTO_ZOOM_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Auto play
    //value_bool = default_media_data.getAutoPlay();
    // set default to auto play true -- angela EXT-5172
    value_bool = true;
    struct functor_getter_auto_play : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_auto_play(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getAutoPlay();
            }
            //return mMediaEntry.getAutoPlay(); set default to auto play true -- angela  EXT-5172
            return true;
        };

        const LLMediaEntry &mMediaEntry;

    } func_auto_play(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_play, value_bool);
    base_key = std::string(LLMediaEntry::AUTO_PLAY_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;


    // Auto scale
    // set default to auto scale true -- angela EXT-5172
    //value_bool = default_media_data.getAutoScale();
    value_bool = true;
    struct functor_getter_auto_scale : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_auto_scale(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getAutoScale();
            }
            // return mMediaEntry.getAutoScale(); set default to auto scale true -- angela EXT-5172
            return true;
        };

        const LLMediaEntry &mMediaEntry;

    } func_auto_scale(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_scale, value_bool);
    base_key = std::string(LLMediaEntry::AUTO_SCALE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Auto loop
    value_bool = default_media_data.getAutoLoop();
    struct functor_getter_auto_loop : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_auto_loop(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getAutoLoop();
            }
            return mMediaEntry.getAutoLoop();
        };

        const LLMediaEntry &mMediaEntry;

    } func_auto_loop(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_auto_loop, value_bool);
    base_key = std::string(LLMediaEntry::AUTO_LOOP_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // width pixels (if not auto scaled)
    value_int = default_media_data.getWidthPixels();
    struct functor_getter_width_pixels : public LLSelectedTEGetFunctor<int>
    {
        functor_getter_width_pixels(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        int get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getWidthPixels();
            }
            return mMediaEntry.getWidthPixels();
        };

        const LLMediaEntry &mMediaEntry;

    } func_width_pixels(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_width_pixels, value_int);
    base_key = std::string(LLMediaEntry::WIDTH_PIXELS_KEY);
    mMediaSettings[base_key] = value_int;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // height pixels (if not auto scaled)
    value_int = default_media_data.getHeightPixels();
    struct functor_getter_height_pixels : public LLSelectedTEGetFunctor<int>
    {
        functor_getter_height_pixels(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        int get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getHeightPixels();
            }
            return mMediaEntry.getHeightPixels();
        };

        const LLMediaEntry &mMediaEntry;

    } func_height_pixels(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_height_pixels, value_int);
    base_key = std::string(LLMediaEntry::HEIGHT_PIXELS_KEY);
    mMediaSettings[base_key] = value_int;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Enable Alt image
    value_bool = default_media_data.getAltImageEnable();
    struct functor_getter_enable_alt_image : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_enable_alt_image(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getAltImageEnable();
            }
            return mMediaEntry.getAltImageEnable();
        };

        const LLMediaEntry &mMediaEntry;

    } func_enable_alt_image(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_enable_alt_image, value_bool);
    base_key = std::string(LLMediaEntry::ALT_IMAGE_ENABLE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - owner interact
    value_bool = 0 != (default_media_data.getPermsInteract() & LLMediaEntry::PERM_OWNER);
    struct functor_getter_perms_owner_interact : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_owner_interact(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_OWNER));
            }
            return 0 != (mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_OWNER);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_owner_interact(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_owner_interact, value_bool);
    base_key = std::string(LLPanelContents::PERMS_OWNER_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - owner control
    value_bool = 0 != (default_media_data.getPermsControl() & LLMediaEntry::PERM_OWNER);
    struct functor_getter_perms_owner_control : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_owner_control(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_OWNER));
            }
            return 0 != (mMediaEntry.getPermsControl() & LLMediaEntry::PERM_OWNER);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_owner_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_owner_control, value_bool);
    base_key = std::string(LLPanelContents::PERMS_OWNER_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - group interact
    value_bool = 0 != (default_media_data.getPermsInteract() & LLMediaEntry::PERM_GROUP);
    struct functor_getter_perms_group_interact : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_group_interact(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_GROUP));
            }
            return 0 != (mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_GROUP);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_group_interact(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_group_interact, value_bool);
    base_key = std::string(LLPanelContents::PERMS_GROUP_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - group control
    value_bool = 0 != (default_media_data.getPermsControl() & LLMediaEntry::PERM_GROUP);
    struct functor_getter_perms_group_control : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_group_control(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_GROUP));
            }
            return 0 != (mMediaEntry.getPermsControl() & LLMediaEntry::PERM_GROUP);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_group_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_group_control, value_bool);
    base_key = std::string(LLPanelContents::PERMS_GROUP_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - anyone interact
    value_bool = 0 != (default_media_data.getPermsInteract() & LLMediaEntry::PERM_ANYONE);
    struct functor_getter_perms_anyone_interact : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_anyone_interact(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsInteract() & LLMediaEntry::PERM_ANYONE));
            }
            return 0 != (mMediaEntry.getPermsInteract() & LLMediaEntry::PERM_ANYONE);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_anyone_interact(default_media_data);

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&func_perms_anyone_interact, value_bool);
    base_key = std::string(LLPanelContents::PERMS_ANYONE_INTERACT_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // Perms - anyone control
    value_bool = 0 != (default_media_data.getPermsControl() & LLMediaEntry::PERM_ANYONE);
    struct functor_getter_perms_anyone_control : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_perms_anyone_control(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return (0 != (object->getTE(face)->getMediaData()->getPermsControl() & LLMediaEntry::PERM_ANYONE));
            }
            return 0 != (mMediaEntry.getPermsControl() & LLMediaEntry::PERM_ANYONE);
        };

        const LLMediaEntry &mMediaEntry;

    } func_perms_anyone_control(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_perms_anyone_control, value_bool);
    base_key = std::string(LLPanelContents::PERMS_ANYONE_CONTROL_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // security - whitelist enable
    value_bool = default_media_data.getWhiteListEnable();
    struct functor_getter_whitelist_enable : public LLSelectedTEGetFunctor<bool>
    {
        functor_getter_whitelist_enable(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        bool get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getWhiteListEnable();
            }
            return mMediaEntry.getWhiteListEnable();
        };

        const LLMediaEntry &mMediaEntry;

    } func_whitelist_enable(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_whitelist_enable, value_bool);
    base_key = std::string(LLMediaEntry::WHITELIST_ENABLE_KEY);
    mMediaSettings[base_key] = value_bool;
    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;

    // security - whitelist URLs
    std::vector<std::string> value_vector_str = default_media_data.getWhiteList();
    struct functor_getter_whitelist_urls : public LLSelectedTEGetFunctor<std::vector<std::string>>
    {
        functor_getter_whitelist_urls(const LLMediaEntry& entry) : mMediaEntry(entry) {}

        std::vector<std::string> get(LLViewerObject* object, S32 face)
        {
            if (object && object->getTE(face) && object->getTE(face)->getMediaData())
            {
                return object->getTE(face)->getMediaData()->getWhiteList();
            }
            return mMediaEntry.getWhiteList();
        };

        const LLMediaEntry &mMediaEntry;

    } func_whitelist_urls(default_media_data);

    identical = selected_objects->getSelectedTEValue(&func_whitelist_urls, value_vector_str);
    base_key = std::string(LLMediaEntry::WHITELIST_KEY);
    mMediaSettings[base_key].clear();

    std::vector< std::string >::iterator iter = value_vector_str.begin();
    while (iter != value_vector_str.end())
    {
        std::string white_list_url = *iter;
        mMediaSettings[base_key].append(white_list_url);
        ++iter;
    };

    mMediaSettings[base_key + std::string(LLPanelContents::TENTATIVE_SUFFIX)] = !identical;
}

void FSPanelFace::updateMediaTitle()
{
    // only get the media name if we need it
    if (!mNeedMediaTitle)
    {
        return;
    }

    // get plugin impl
    LLPluginClassMedia* media_plugin = mTitleMedia->getMediaPlugin();
    if (media_plugin && mTitleMedia->getCurrentNavUrl() == media_plugin->getNavigateURI())
    {
        // get the media name (asynchronous - must call repeatedly)
        std::string media_title = media_plugin->getMediaName();

        // only replace the title if what we get contains something
        if (!media_title.empty())
        {
            // update the UI widget
            mTitleMediaText->setText(media_title);

            // stop looking for a title when we get one
            mNeedMediaTitle = false;
        }
    }
}

void FSPanelFace::onCommitColor()
{
    sendColor();
}

void FSPanelFace::onCommitShinyColor()
{
    LLSelectedTEMaterial::setSpecularLightColor(this, mShinyColorSwatch->get());
}

void FSPanelFace::onCommitAlpha()
{
    mCtrlColorTransp->setFocus(true);
    sendAlpha();
}

void FSPanelFace::onCancelColor()
{
    LLSelectMgr::getInstance()->selectionRevertColors();
}

void FSPanelFace::onCancelShinyColor()
{
    LLSelectMgr::getInstance()->selectionRevertShinyColors();
}

void FSPanelFace::onSelectColor()
{
    LLSelectMgr::getInstance()->saveSelectedObjectColors();
    sendColor();
}

void FSPanelFace::onSelectShinyColor()
{
    LLSelectedTEMaterial::setSpecularLightColor(this, mShinyColorSwatch->get());
    LLSelectMgr::getInstance()->saveSelectedShinyColors();
}

void FSPanelFace::updateVisibility(LLViewerObject* objectp /* = nullptr */)
{
    S32 materials_media = getCurrentMaterialType();
    bool show_media = materials_media == MATMEDIA_MEDIA;
    bool show_material = materials_media == MATMEDIA_MATERIAL;

    // Shared material controls
    mCheckSyncMaterials->setVisible(show_material || show_media);

    updateAlphaControls();
    // TODO: is this still needed? -Zi
    updateShinyControls();
    updateBumpyControls();

    // Find all faces with same texture
    // TODO: is this still needed? -Zi Probably not but we don't have these buttons yet anyway -Zi
    /*
    getChild<LLUICtrl>("btn_select_same_diff")->setVisible(mTextureCtrl->getVisible());
    getChild<LLUICtrl>("btn_select_same_norm")->setVisible(mBumpyTextureCtrl->getVisible());
    getChild<LLUICtrl>("btn_select_same_spec")->setVisible(mShinyTextureCtrl->getVisible());
    */
}

void FSPanelFace::onCommitBump()
{
    sendBump(mComboBumpiness->getCurrentIndex());
}

void FSPanelFace::onCommitTexGen()
{
    sendTexGen();
}

void FSPanelFace::updateShinyControls(bool is_setting_texture, bool mess_with_shiny_combobox)
{
    LLUUID shiny_texture_ID = mShinyTextureCtrl->getImageAssetID();
    LL_DEBUGS("Materials") << "Shiny texture selected: " << shiny_texture_ID << LL_ENDL;

    if(mess_with_shiny_combobox)
    {
        if (shiny_texture_ID.notNull() && is_setting_texture)
        {
            if (!mComboShininess->itemExists(USE_TEXTURE_LABEL))
            {
                mComboShininess->add(USE_TEXTURE_LABEL);
            }
            mComboShininess->setSimple(USE_TEXTURE_LABEL);
        }
        else
        {
            if (mComboShininess->itemExists(USE_TEXTURE_LABEL))
            {
                mComboShininess->remove(SHINY_TEXTURE);
                mComboShininess->selectFirstItem();
            }
        }
    }
    else
    {
        if (shiny_texture_ID.isNull() && mComboShininess->itemExists(USE_TEXTURE_LABEL))
        {
            mComboShininess->remove(SHINY_TEXTURE);
            mComboShininess->selectFirstItem();
        }
    }

    mShinyColorSwatch->setValid(shiny_texture_ID.notNull());
    LL_DEBUGS("ENABLEDISABLETOOLS") << "mShinyColorSwatch valid " << shiny_texture_ID.notNull() << LL_ENDL;
    mShinyColorSwatch->setFallbackImage(LLUI::getUIImage("locked_image.j2c"));

    gSavedSettings.setBOOL("FSInternalFaceHasBPSpecularMap", shiny_texture_ID.notNull());

    LL_DEBUGS("ENABLEDISABLETOOLS") << "panel_blinn_phong_specular " << shiny_texture_ID.notNull() << LL_ENDL;
}

void FSPanelFace::updateBumpyControls(bool is_setting_texture, bool mess_with_combobox)
{
    LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
    LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;

    if (mess_with_combobox)
    {
        LLUUID bumpy_texture_ID = mBumpyTextureCtrl->getImageAssetID();
        LL_DEBUGS("Materials") << "texture: " << bumpy_texture_ID << (mess_with_combobox ? "" : " do not") << " update combobox" << LL_ENDL;

        if (bumpy_texture_ID.notNull() && is_setting_texture)
        {
            if (!mComboBumpiness->itemExists(USE_TEXTURE_LABEL))
            {
                mComboBumpiness->add(USE_TEXTURE_LABEL);
            }
            mComboBumpiness->setSimple(USE_TEXTURE_LABEL);
        }
        else
        {
            if (mComboBumpiness->itemExists(USE_TEXTURE_LABEL))
            {
                mComboBumpiness->remove(BUMPY_TEXTURE);
                mComboBumpiness->selectFirstItem();
            }
        }
    }

    gSavedSettings.setBOOL("FSInternalFaceHasBPNormalMap", bumpy_texture_ID.notNull());

    LL_DEBUGS("ENABLEDISABLETOOLS") << "panel_blinn_phong_normal " << bumpy_texture_ID.notNull() << LL_ENDL;
}

void FSPanelFace::onCommitShiny()
{
    sendShiny(mComboShininess->getCurrentIndex());
}

void FSPanelFace::updateAlphaControls()
{
    mCtrlMaskCutoff->setEnabled(getCurrentDiffuseAlphaMode() == LLMaterial::DIFFUSE_ALPHA_MODE_MASK);
}

void FSPanelFace::onCommitAlphaMode()
{
    updateAlphaControls();
    LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());
}

void FSPanelFace::onCommitFullbright()
{
    sendFullbright();
}

void FSPanelFace::onCommitHideWater()
{
    if (mCheckHideWater->get())
    {
        LLHandle<LLPanel> handle = getHandle();
        LLNotificationsUtil::add("WaterExclusionSurfacesWarning", LLSD(), LLSD(),
            [handle](const LLSD& notification, const LLSD& response)
        {
            if(FSPanelFace* panel = (FSPanelFace*)handle.get())
            {
                if (LLNotificationsUtil::getSelectedOption(notification, response) == 1)
                {
                    panel->mCheckHideWater->setValue(false);
                    return;
                }
                // apply invisiprim texture and reset related params to set water exclusion surface
                panel->sendBump(0);
                panel->sendShiny(0);
                LLSelectMgr::getInstance()->selectionSetAlphaOnly(1.f);
                LLSelectMgr::getInstance()->selectionSetImage(IMG_ALPHA_GRAD);
                LLSelectedTEMaterial::setDiffuseAlphaMode(panel, LLMaterial::DIFFUSE_ALPHA_MODE_BLEND);
            }
        });
    }
    else
    {
        // reset texture to default plywood
        LLSelectMgr::getInstance()->selectionSetImage(DEFAULT_OBJECT_TEXTURE);
    }
}

void FSPanelFace::onCommitGlow()
{
    mCtrlGlow->setFocus(true);
    sendGlow();
}

//
// PBR
//

// Get a dump of the json representation of the current state of the editor UI
// in GLTF format, excluding transforms as they are not supported in material
// assets. (See also LLGLTFMaterial::sanitizeAssetMaterial())
void FSPanelFace::getGLTFMaterial(LLGLTFMaterial* mat)
{
    mat->mBaseColor = linearColor4(LLColor4(mBaseTintPBR->get()));
    mat->mBaseColor.mV[VALPHA] = mAlphaPBR->get();

    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] = mBaseTexturePBR->getImageAssetID();
    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL] = mNormalTexturePBR->getImageAssetID();
    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS] = mORMTexturePBR->getImageAssetID();
    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE] = mEmissiveTexturePBR->getImageAssetID();

    mat->mEmissiveColor = linearColor4(LLColor4(mEmissiveTintPBR->get()));

    mat->mMetallicFactor = mMetallicFactorPBR->get();
    mat->mRoughnessFactor = mRoughnessFactorPBR->get();

    mat->mDoubleSided = mCheckDoubleSidedPBR->get();
    mat->setAlphaMode(mAlphaModePBR->getValue().asString());
    mat->mAlphaCutoff = (F32)mMaskCutoffPBR->getValue().asReal();
}

bool FSPanelFace::onDragPbr(LLInventoryItem* item)
{
    bool accept = true;
    for (LLObjectSelection::root_iterator iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
        iter != LLSelectMgr::getInstance()->getSelection()->root_end(); iter++)
    {
        LLSelectNode* node = *iter;
        LLViewerObject* obj = node->getObject();
        if (!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
        {
            accept = false;
            break;
        }
    }
    return accept;
}

void FSPanelFace::onCommitPbr()
{
    if (!mMaterialCtrlPBR->getTentative())
    {
        // we grab the item id first, because we want to do a
        // permissions check in the selection manager. ARGH!
        LLUUID id = mMaterialCtrlPBR->getImageItemID();
        if (id.isNull())
        {
            id = mMaterialCtrlPBR->getImageAssetID();
        }
        if (!LLSelectMgr::getInstance()->selectionSetGLTFMaterial(id))
        {
            // If failed to set material, refresh mMaterialCtrlPBR's value
            refresh();
        }
    }
}

void FSPanelFace::onCancelPbr()
{
    LLSelectMgr::getInstance()->selectionRevertGLTFMaterials();
}

void FSPanelFace::onSelectPbr()
{
    LLSelectMgr::getInstance()->saveSelectedObjectTextures();
    onCommitPbr();
}

void FSPanelFace::onCommitPbr(const LLUICtrl* pbr_ctrl)
{
    LL_WARNS("FACEPANELM") << "committed pbr map " << pbr_ctrl->getName() << LL_ENDL;

    if (pbr_ctrl == mBaseTexturePBR)
    {
        mUnsavedChanges |= MATERIAL_BASE_COLOR_TEX_DIRTY;
    }
    else if (pbr_ctrl == mNormalTexturePBR)
    {
        mUnsavedChanges |= MATERIAL_NORMAL_TEX_DIRTY;
    }
    else if (pbr_ctrl == mEmissiveTexturePBR)
    {
        mUnsavedChanges |= MATERIAL_EMISIVE_TEX_DIRTY;
    }
    else if (pbr_ctrl == mORMTexturePBR)
    {
        mUnsavedChanges |= MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY;
    }
    else if (pbr_ctrl == mBaseTintPBR)
    {
        mUnsavedChanges |= MATERIAL_BASE_COLOR_DIRTY;
    }
    else if (pbr_ctrl == mEmissiveTintPBR)
    {
        mUnsavedChanges |= MATERIAL_EMISIVE_COLOR_DIRTY;
    }
    else if (pbr_ctrl == mCheckDoubleSidedPBR)
    {
        mUnsavedChanges |= MATERIAL_DOUBLE_SIDED_DIRTY;
    }
    else if (pbr_ctrl == mAlphaPBR)
    {
        mAlphaPBR->setFocus(true);
        mUnsavedChanges |= MATERIAL_BASE_COLOR_DIRTY;
    }
    else if (pbr_ctrl == mAlphaModePBR)
    {
        mUnsavedChanges |= MATERIAL_ALPHA_MODE_DIRTY;
    }
    else if (pbr_ctrl == mMaskCutoffPBR)
    {
        mMaskCutoffPBR->setFocus(true);
        mUnsavedChanges |= MATERIAL_ALPHA_CUTOFF_DIRTY;
    }
    else if (pbr_ctrl == mMetallicFactorPBR)
    {
        mMetallicFactorPBR->setFocus(true);
        mUnsavedChanges |= MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY;
    }
    else if (pbr_ctrl == mRoughnessFactorPBR)
    {
        mRoughnessFactorPBR->setFocus(true);
        mUnsavedChanges |= MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY;
    }
}

// highlights PBR material parameters that are part of a material override
void FSPanelFace::updatePBROverrideDisplay()
{
    LLColor4 labelTextColor = LLUIColorTable::instance().getColor("LabelTextColor");
    LLColor4 emphasisColor = LLUIColorTable::instance().getColor("EmphasisColor");      // TODO: maybe create distinct color for skins -Zi

    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();

    if (!objectp || !node)
    {
        return;
    }

    // a way to resolve situations where source and target have different amount of faces
    S32 num_tes = llmin((S32)objectp->getNumTEs(), objectp->getNumFaces());
    for (S32 te = 0; te < num_tes; ++te)
    {
        if (!node->isTESelected(te))
        {
            continue;
        }

        LLTextureEntry* tep = objectp->getTE(te);
        if (!tep)
        {
            continue;
        }

        LLGLTFMaterial* override_material = tep->getGLTFMaterialOverride();
        if (!override_material)
        {
            continue;
        }

        LL_DEBUGS("GET_GLTF_MATERIAL_PARAMS")
            << override_material << "\n"
            << override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] << "\n"
            << override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL] << "\n"
            << override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS] << "\n"
            << override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE] << "\n"
            << override_material->mBaseColor << "\n"
            << override_material->mEmissiveColor << "\n"
            << override_material->mAlphaMode << "\n"
            << override_material->mAlphaCutoff << "\n"
            << override_material->mMetallicFactor << "\n"
            << override_material->mRoughnessFactor << "\n"
            << override_material->mDoubleSided << "\n"
            << LL_ENDL;

        mBaseTexturePBR->setLabelColor(override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].isNull() ? labelTextColor : emphasisColor);
        mNormalTexturePBR->setLabelColor(override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL].isNull() ? labelTextColor : emphasisColor);
        mORMTexturePBR->setLabelColor(override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS].isNull() ? labelTextColor : emphasisColor);
        mEmissiveTexturePBR->setLabelColor(override_material->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE].isNull() ? labelTextColor : emphasisColor);

        mBaseTintPBR->setLabelColor(override_material->mBaseColor == LLGLTFMaterial::getDefaultBaseColor() ? labelTextColor : emphasisColor);
        mAlphaPBR->setLabelColor(override_material->mBaseColor == LLGLTFMaterial::getDefaultBaseColor() ? labelTextColor : emphasisColor);
        mLabelAlphaModePBR->setColor(override_material->mAlphaMode == LLGLTFMaterial::getDefaultAlphaMode() ? labelTextColor : emphasisColor);
        mMaskCutoffPBR->setLabelColor(override_material->mAlphaCutoff == LLGLTFMaterial::getDefaultAlphaCutoff() ? labelTextColor : emphasisColor);

        mMetallicFactorPBR->setLabelColor(override_material->mMetallicFactor == LLGLTFMaterial::getDefaultMetallicFactor() ? labelTextColor : emphasisColor);
        mRoughnessFactorPBR->setLabelColor(override_material->mRoughnessFactor == LLGLTFMaterial::getDefaultRoughnessFactor() ? labelTextColor : emphasisColor);

        mEmissiveTintPBR->setLabelColor(override_material->mEmissiveColor == LLGLTFMaterial::getDefaultEmissiveColor() ? labelTextColor : emphasisColor);

        mCheckDoubleSidedPBR->setEnabledColor(override_material->mDoubleSided == LLGLTFMaterial::getDefaultDoubleSided() ? labelTextColor : emphasisColor);
        // refresh checkbox, as LLCheckBoxCtrl does not refresh its label when you change the color
        mCheckDoubleSidedPBR->setEnabled(mCheckDoubleSidedPBR->getEnabled());

        break;
    }
}

void FSPanelFace::onCancelPbr(const LLUICtrl* map_ctrl)
{
    if (map_ctrl == mBaseTexturePBR)
    {
        mRevertedChanges |= MATERIAL_BASE_COLOR_TEX_DIRTY;
    }
    else if (map_ctrl == mNormalTexturePBR)
    {
        mRevertedChanges |= MATERIAL_NORMAL_TEX_DIRTY;
    }
    else if (map_ctrl == mEmissiveTexturePBR)
    {
        mRevertedChanges |= MATERIAL_EMISIVE_TEX_DIRTY;
    }
    else if (map_ctrl == mORMTexturePBR)
    {
        mRevertedChanges |= MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY;
    }
    else if (map_ctrl == mBaseTintPBR)
    {
        mRevertedChanges |= MATERIAL_BASE_COLOR_DIRTY;
    }
    else if (map_ctrl == mEmissiveTintPBR)
    {
        mRevertedChanges |= MATERIAL_EMISIVE_COLOR_DIRTY;
    }
}

void FSPanelFace::onSelectPbr(const LLUICtrl* map_ctrl)
{
    onCommitPbr(map_ctrl);

    struct f : public LLSelectedNodeFunctor
    {
        f(const LLUICtrl* ctrl, S32 dirty_flag) : mCtrl(ctrl), mDirtyFlag(dirty_flag)
        {
        }

        virtual bool apply(LLSelectNode* nodep)
        {
            LLViewerObject* objectp = nodep->getObject();
            if (!objectp)
            {
                return false;
            }
            S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces()); // avatars have TEs but no faces
            for (S32 te = 0; te < num_tes; ++te)
            {
                if (nodep->isTESelected(te) && nodep->mSavedGLTFOverrideMaterials.size() > te)
                {
                    if (nodep->mSavedGLTFOverrideMaterials[te].isNull())
                    {
                        // populate with default values, default values basically mean 'not in use'
                        nodep->mSavedGLTFOverrideMaterials[te] = new LLGLTFMaterial();
                    }

                    switch (mDirtyFlag)
                    {
                    //Textures
                    case MATERIAL_BASE_COLOR_TEX_DIRTY:
                    {
                        nodep->mSavedGLTFOverrideMaterials[te]->setBaseColorId(mCtrl->getValue().asUUID(), true);
                        //update_local_texture(mCtrl, nodep->mSavedGLTFOverrideMaterials[te].get());
                        break;
                    }
                    case MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY:
                    {
                        nodep->mSavedGLTFOverrideMaterials[te]->setOcclusionRoughnessMetallicId(mCtrl->getValue().asUUID(), true);
                        //update_local_texture(mCtrl, nodep->mSavedGLTFOverrideMaterials[te].get());
                        break;
                    }
                    case MATERIAL_EMISIVE_TEX_DIRTY:
                    {
                        nodep->mSavedGLTFOverrideMaterials[te]->setEmissiveId(mCtrl->getValue().asUUID(), true);
                        //update_local_texture(mCtrl, nodep->mSavedGLTFOverrideMaterials[te].get());
                        break;
                    }
                    case MATERIAL_NORMAL_TEX_DIRTY:
                    {
                        nodep->mSavedGLTFOverrideMaterials[te]->setNormalId(mCtrl->getValue().asUUID(), true);
                        //update_local_texture(mCtrl, nodep->mSavedGLTFOverrideMaterials[te].get());
                        break;
                    }
                    // Colors
                    case MATERIAL_BASE_COLOR_DIRTY:
                    {
                        LLColor4 ret = linearColor4(LLColor4(mCtrl->getValue()));
                        // except transparency
                        ret.mV[3] = nodep->mSavedGLTFOverrideMaterials[te]->mBaseColor.mV[3];
                        nodep->mSavedGLTFOverrideMaterials[te]->setBaseColorFactor(ret, true);
                        break;
                    }
                    case MATERIAL_EMISIVE_COLOR_DIRTY:
                    {
                        nodep->mSavedGLTFOverrideMaterials[te]->setEmissiveColorFactor(LLColor3(mCtrl->getValue()), true);
                        break;
                    }
                    default:
                        break;
                    }
                }
            }
            return true;
        }

        const LLUICtrl* mCtrl;
        S32 mDirtyFlag;
    } func(map_ctrl, mUnsavedChanges);

    LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func);
}

bool FSPanelFace::onDragTexture(const LLUICtrl* texture_ctrl, LLInventoryItem* item)
{
    bool accept = true;
    for (LLObjectSelection::root_iterator iter = LLSelectMgr::getInstance()->getSelection()->root_begin();
        iter != LLSelectMgr::getInstance()->getSelection()->root_end(); iter++)
    {
        // applying the texture relies on the tab control showing the correct channel
        if (texture_ctrl == mTextureCtrl)
        {
            selectMatChannel(LLRender::DIFFUSE_MAP);
        }
        else if (texture_ctrl == mBumpyTextureCtrl)
        {
            selectMatChannel(LLRender::NORMAL_MAP);
        }
        else if (texture_ctrl == mShinyTextureCtrl)
        {
            selectMatChannel(LLRender::SPECULAR_MAP);
        }

        LLSelectNode* node = *iter;
        LLViewerObject* obj = node->getObject();
        if (!LLToolDragAndDrop::isInventoryDropAcceptable(obj, item))
        {
            accept = false;
            break;
        }
    }
    return accept;
}

void FSPanelFace::onCommitTexture()
{
    add(LLStatViewer::EDIT_TEXTURE, 1);

    // applying the texture relies on the tab control showing the correct channel
    selectMatChannel(LLRender::DIFFUSE_MAP);

    sendTexture();
}

void FSPanelFace::onSelectTexture()
{
    add(LLStatViewer::EDIT_TEXTURE, 1);

    // applying the texture relies on the tab control showing the correct channel
    selectMatChannel(LLRender::DIFFUSE_MAP);

    LLSelectMgr::getInstance()->saveSelectedObjectTextures();
    sendTexture();

    LLGLenum image_format;
    bool identical_image_format = false;
    bool missing_asset = false;
    LLSelectedTE::getImageFormat(image_format, identical_image_format, missing_asset);

    if (!missing_asset)
    {
        U32 alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
        switch (image_format)
        {
            case GL_RGBA:
            case GL_ALPHA:
            {
                alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_BLEND;
            }
            break;

            case GL_RGB: break;
            default:
            {
                LL_WARNS() << "Unexpected tex format in FSPanelFace, resorting to no alpha" << LL_ENDL;
            }
            break;
        }

        mComboAlphaMode->selectNthItem(alpha_mode);
    }

    LLSelectedTEMaterial::setDiffuseAlphaMode(this, getCurrentDiffuseAlphaMode());

    onTextureSelectionChanged(mTextureCtrl);
}

void FSPanelFace::onCommitNormalTexture()
{
    LLUUID nmap_id = getCurrentNormalMap();

    // applying the texture relies on the tab control showing the correct channel
    selectMatChannel(LLRender::NORMAL_MAP);

    sendBump(nmap_id.isNull() ? 0 : BUMPY_TEXTURE);
    onTextureSelectionChanged(mBumpyTextureCtrl);
}

void FSPanelFace::onCommitSpecularTexture()
{
    // applying the texture relies on the tab control showing the correct channel
    selectMatChannel(LLRender::SPECULAR_MAP);

    sendShiny(SHINY_TEXTURE);
    onTextureSelectionChanged(mShinyTextureCtrl);
}

void FSPanelFace::onCloseTexturePicker()
{
    updateUI();
}

void FSPanelFace::onCancelTexture()
{
    LL_WARNS() << "onCancelTexture" << LL_ENDL;
    LLSelectMgr::getInstance()->selectionRevertTextures();
}

void FSPanelFace::onCancelNormalTexture()
{
    U8 bumpy = 0;
    bool identical_bumpy = false;
    LLSelectedTE::getBumpmap(bumpy, identical_bumpy);
    LLUUID normal_map_id = mBumpyTextureCtrl->getImageAssetID();
    bumpy = normal_map_id.isNull() ? bumpy : BUMPY_TEXTURE;
    sendBump(bumpy);
}

void FSPanelFace::onCancelSpecularTexture()
{
    U8 shiny = 0;
    bool identical_shiny = false;
    LLSelectedTE::getShiny(shiny, identical_shiny);
    LLUUID spec_map_id = mShinyTextureCtrl->getImageAssetID();
    shiny = spec_map_id.isNull() ? shiny : SHINY_TEXTURE;
    sendShiny(shiny);
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to edit existing media settings on a prim or prim face
// TODO: test if there is media on the item and only allow editing if present
// NOTE: there is no actual "Button" Edit Media, but this function is called from
//       onClickBtnAddMedia() where needed, so the naming is probably just old cruft -Zi
void FSPanelFace::onClickBtnEditMedia()
{
    refreshMedia();
    LLFloaterReg::showInstance("media_settings");
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to delete media from a prim or prim face
void FSPanelFace::onClickBtnDeleteMedia()
{
    LLNotificationsUtil::add("DeleteMedia", LLSD(), LLSD(), boost::bind(&FSPanelFace::deleteMediaConfirm, this, _1, _2));
}

//////////////////////////////////////////////////////////////////////////////
// called when a user wants to add media to a prim or prim face
void FSPanelFace::onClickBtnAddMedia()
{
    // check if multiple faces are selected
    if (LLSelectMgr::getInstance()->getSelection()->isMultipleTESelected())
    {
        refreshMedia();
        LLNotificationsUtil::add("MultipleFacesSelected", LLSD(), LLSD(), boost::bind(&FSPanelFace::multipleFacesSelectedConfirm, this, _1, _2));
    }
    else
    {
        onClickBtnEditMedia();
    }
}

bool FSPanelFace::deleteMediaConfirm(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    switch (option)
    {
        case 0:  // "Yes"
            LLSelectMgr::getInstance()->selectionSetMedia(0, LLSD());
            if (LLFloaterReg::instanceVisible("media_settings"))
            {
                LLFloaterReg::hideInstance("media_settings");
            }
            break;

        case 1:  // "No"
        default:
            break;
    }
    return false;
}

bool FSPanelFace::multipleFacesSelectedConfirm(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    switch (option)
    {
        case 0:  // "Yes"
            LLFloaterReg::showInstance("media_settings");
            break;

        case 1:  // "No"
        default:
            break;
    }
    return false;
}

//static
void FSPanelFace::syncOffsetX(FSPanelFace* self, F32 offsetU)
{
    LLSelectedTEMaterial::setNormalOffsetX(self,offsetU);
    LLSelectedTEMaterial::setSpecularOffsetX(self,offsetU);
    self->mCtrlTexOffsetU->forceSetValue(offsetU);
    self->sendTextureInfo();
}

//static
void FSPanelFace::syncOffsetY(FSPanelFace* self, F32 offsetV)
{
    LLSelectedTEMaterial::setNormalOffsetY(self,offsetV);
    LLSelectedTEMaterial::setSpecularOffsetY(self,offsetV);
    self->mCtrlTexOffsetV->forceSetValue(offsetV);
    self->sendTextureInfo();
}

void FSPanelFace::onCommitMaterialBumpyOffsetX()
{
    mCtrlBumpyOffsetU->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetX(this, getCurrentBumpyOffsetU());
    }
    else
    {
        LLSelectedTEMaterial::setNormalOffsetX(this, getCurrentBumpyOffsetU());
    }
}

void FSPanelFace::onCommitMaterialBumpyOffsetY()
{
    mCtrlBumpyOffsetV->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetY(this, getCurrentBumpyOffsetV());
    }
    else
    {
        LLSelectedTEMaterial::setNormalOffsetY(this, getCurrentBumpyOffsetV());
    }
}

void FSPanelFace::onCommitMaterialShinyOffsetX()
{
    mCtrlShinyOffsetU->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetX(this, getCurrentShinyOffsetU());
    }
    else
    {
        LLSelectedTEMaterial::setSpecularOffsetX(this, getCurrentShinyOffsetU());
    }
}

void FSPanelFace::onCommitMaterialShinyOffsetY()
{
    mCtrlShinyOffsetV->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetY(this, getCurrentShinyOffsetV());
    }
    else
    {
        LLSelectedTEMaterial::setSpecularOffsetY(this, getCurrentShinyOffsetV());
    }
}

//static
void FSPanelFace::syncRepeatX(FSPanelFace* self, F32 scaleU)
{
    LLSelectedTEMaterial::setNormalRepeatX(self,scaleU);
    LLSelectedTEMaterial::setSpecularRepeatX(self,scaleU);
    self->sendTextureInfo();
}

//static
void FSPanelFace::syncRepeatY(FSPanelFace* self, F32 scaleV)
{
    LLSelectedTEMaterial::setNormalRepeatY(self,scaleV);
    LLSelectedTEMaterial::setSpecularRepeatY(self,scaleV);
    self->sendTextureInfo();
}

void FSPanelFace::onCommitMaterialBumpyScaleX()
{
    mCtrlBumpyScaleU->setFocus(true);
    F32 bumpy_scale_u = getCurrentBumpyScaleU();
    if (isIdenticalPlanarTexgen())
    {
        bumpy_scale_u *= 0.5f;
    }

    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexScaleU->forceSetValue(LLSD(getCurrentBumpyScaleU()));
        syncRepeatX(this, bumpy_scale_u);
    }
    else
    {
        LLSelectedTEMaterial::setNormalRepeatX(this, bumpy_scale_u);
    }
}

void FSPanelFace::onCommitMaterialBumpyScaleY()
{
    mCtrlBumpyScaleV->setFocus(true);
    F32 bumpy_scale_v = getCurrentBumpyScaleV();
    if (isIdenticalPlanarTexgen())
    {
        bumpy_scale_v *= 0.5f;
    }

    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexScaleV->forceSetValue(LLSD(getCurrentBumpyScaleV()));
        syncRepeatY(this, bumpy_scale_v);
    }
    else
    {
        LLSelectedTEMaterial::setNormalRepeatY(this, bumpy_scale_v);
    }
}

void FSPanelFace::onCommitMaterialShinyScaleX()
{
    mCtrlShinyScaleU->setFocus(true);
    F32 shiny_scale_u = getCurrentShinyScaleU();
    if (isIdenticalPlanarTexgen())
    {
        shiny_scale_u *= 0.5f;
    }

    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexScaleU->forceSetValue(LLSD(getCurrentShinyScaleU()));
        syncRepeatX(this, shiny_scale_u);
    }
    else
    {
        LLSelectedTEMaterial::setSpecularRepeatX(this, shiny_scale_u);
    }
}

void FSPanelFace::onCommitMaterialShinyScaleY()
{
    mCtrlShinyScaleV->setFocus(true);
    F32 shiny_scale_v = getCurrentShinyScaleV();
    if (isIdenticalPlanarTexgen())
    {
        shiny_scale_v *= 0.5f;
    }

    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexScaleV->forceSetValue(LLSD(getCurrentShinyScaleV()));
        syncRepeatY(this, shiny_scale_v);
    }
    else
    {
        LLSelectedTEMaterial::setSpecularRepeatY(this, shiny_scale_v);
    }
}

//static
void FSPanelFace::syncMaterialRot(FSPanelFace* self, F32 rot, int te)
{
    LLSelectedTEMaterial::setNormalRotation(self,rot * DEG_TO_RAD, te);
    LLSelectedTEMaterial::setSpecularRotation(self,rot * DEG_TO_RAD, te);
    self->sendTextureInfo();
}

void FSPanelFace::onCommitMaterialBumpyRot()
{
    mCtrlBumpyRot->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexRot->forceSetValue(LLSD(getCurrentBumpyRot()));
        syncMaterialRot(this, getCurrentBumpyRot());
    }
    else
    {
        if (mCheckPlanarAlign->getValue().asBoolean())
        {
            LLFace* last_face = NULL;
            bool identical_face = false;
            LLSelectedTE::getFace(last_face, identical_face);
            FSPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
            LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
        }
        else
        {
            LLSelectedTEMaterial::setNormalRotation(this, getCurrentBumpyRot() * DEG_TO_RAD);
        }
    }
}

void FSPanelFace::onCommitMaterialShinyRot()
{
    mCtrlShinyRot->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        mCtrlTexRot->forceSetValue(LLSD(getCurrentShinyRot()));
        syncMaterialRot(this, getCurrentShinyRot());
    }
    else
    {
        if (mCheckPlanarAlign->getValue().asBoolean())
        {
            LLFace* last_face = NULL;
            bool identical_face = false;
            LLSelectedTE::getFace(last_face, identical_face);
            FSPanelFaceSetAlignedTEFunctor setfunc(this, last_face);
            LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
        }
        else
        {
            LLSelectedTEMaterial::setSpecularRotation(this, getCurrentShinyRot() * DEG_TO_RAD);
        }
    }
}

void FSPanelFace::onCommitMaterialGloss()
{
    mCtrlGlossiness->setFocus(true);
    LLSelectedTEMaterial::setSpecularLightExponent(this, getCurrentGlossiness());
}

void FSPanelFace::onCommitMaterialEnv()
{
    mCtrlEnvironment->setFocus(true);
    LLSelectedTEMaterial::setEnvironmentIntensity(this, getCurrentEnvIntensity());
}

void FSPanelFace::onCommitMaterialMaskCutoff()
{
    mCtrlMaskCutoff->setFocus(true);
    LLSelectedTEMaterial::setAlphaMaskCutoff(this, getCurrentAlphaMaskCutoff());
}

void FSPanelFace::onCommitTextureScaleX()
{
    mCtrlTexScaleU->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        F32 bumpy_scale_u = (F32)mCtrlTexScaleU->getValue().asReal();
        if (isIdenticalPlanarTexgen())
        {
            bumpy_scale_u *= 0.5f;
        }
        syncRepeatX(this, bumpy_scale_u);
    }
    else
    {
        sendTextureInfo();
    }
    updateUI(true);
}

void FSPanelFace::onCommitTextureScaleY()
{
    mCtrlTexScaleV->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        F32 bumpy_scale_v = (F32)mCtrlTexScaleV->getValue().asReal();
        if (isIdenticalPlanarTexgen())
        {
            bumpy_scale_v *= 0.5f;
        }
        syncRepeatY(this, bumpy_scale_v);
    }
    else
    {
        sendTextureInfo();
    }
    updateUI(true);
}

void FSPanelFace::onCommitTextureRot()
{
    mCtrlTexRot->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncMaterialRot(this, (F32)mCtrlTexRot->getValue().asReal());
    }
    else
    {
        sendTextureInfo();
    }
    updateUI(true);
}


void FSPanelFace::onCommitTextureOffsetX()
{
    mCtrlTexOffsetU->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetX(this, (F32)mCtrlTexOffsetU->getValue().asReal());
    }
    else
    {
        sendTextureInfo();
    }
    updateUI(true);
}

void FSPanelFace::onCommitTextureOffsetY()
{
    mCtrlTexOffsetV->setFocus(true);
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        syncOffsetY(this, (F32)mCtrlTexOffsetV->getValue().asReal());
    }
    else
    {
        sendTextureInfo();
    }
    updateUI(true);
}

// Commit the number of repeats per meter
void FSPanelFace::onCommitRepeatsPerMeter()
{
    mCtrlRpt->setFocus(true);
    S32 materials_media = getCurrentMaterialType();
    LLRender::eTexIndex material_channel = LLRender::DIFFUSE_MAP;
    LLGLTFMaterial::TextureInfo material_type = LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR;

    if (materials_media == MATMEDIA_PBR)
    {
        material_channel = getCurrentPBRChannel();
        material_type    = getCurrentPBRType(material_channel);
    }

    if (materials_media == MATMEDIA_MATERIAL)
    {
        material_channel = getCurrentMatChannel();
    }

    F32 repeats_per_meter = (F32)mCtrlRpt->getValue().asReal();

    F32 obj_scale_s = 1.0f;
    F32 obj_scale_t = 1.0f;

    bool identical_scale_s = false;
    bool identical_scale_t = false;

    LLSelectedTE::getObjectScaleS(obj_scale_s, identical_scale_s);
    LLSelectedTE::getObjectScaleT(obj_scale_t, identical_scale_t);

    if (mCheckSyncMaterials->isAvailable() && gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        LLSelectMgr::getInstance()->selectionTexScaleAutofit(repeats_per_meter);

        mCtrlBumpyScaleU->setValue(obj_scale_s * repeats_per_meter);
        mCtrlBumpyScaleV->setValue(obj_scale_t * repeats_per_meter);

        LLSelectedTEMaterial::selectionNormalScaleAutofit(this, repeats_per_meter);

        mCtrlShinyScaleU->setValue(obj_scale_s * repeats_per_meter);
        mCtrlShinyScaleV->setValue(obj_scale_t * repeats_per_meter);

        LLSelectedTEMaterial::selectionSpecularScaleAutofit(this, repeats_per_meter);
    }
    else
    {
        switch (material_channel)
        {
            case LLRender::DIFFUSE_MAP:
            {
                LLSelectMgr::getInstance()->selectionTexScaleAutofit(repeats_per_meter);
            }
            break;

            case LLRender::NORMAL_MAP:
            {
                mCtrlBumpyScaleU->setValue(obj_scale_s * repeats_per_meter);
                mCtrlBumpyScaleV->setValue(obj_scale_t * repeats_per_meter);

                LLSelectedTEMaterial::selectionNormalScaleAutofit(this, repeats_per_meter);
            }
            break;

            case LLRender::SPECULAR_MAP:
            {
                mCtrlShinyScaleU->setValue(obj_scale_s * repeats_per_meter);
                mCtrlShinyScaleV->setValue(obj_scale_t * repeats_per_meter);

                LLSelectedTEMaterial::selectionSpecularScaleAutofit(this, repeats_per_meter);
            }
            break;

            case LLRender::BASECOLOR_MAP:
            case LLRender::METALLIC_ROUGHNESS_MAP:
            case LLRender::GLTF_NORMAL_MAP:
            case LLRender::EMISSIVE_MAP:
            case LLRender::NUM_TEXTURE_CHANNELS:
            {
                updateGLTFTextureTransformWithScale(material_type, [&](LLGLTFMaterial::TextureTransform* new_transform, F32 scale_s, F32 scale_t)
                {
                    new_transform->mScale.mV[VX] = scale_s * repeats_per_meter;
                    new_transform->mScale.mV[VY] = scale_t * repeats_per_meter;
                });
            }
            break;

            default:
                llassert(false);
                break;
        }
    }

    // vertical scale and repeats per meter depends on each other, so force set on changes
    updateUI(true);
}

struct FSPanelFaceSetMediaFunctor : public LLSelectedTEFunctor
{
    virtual bool apply(LLViewerObject* object, S32 te)
    {
        viewer_media_t pMediaImpl;

        const LLTextureEntry &tep = object->getTEref(te);
        const LLMediaEntry* mep = tep.hasMedia() ? tep.getMediaData() : NULL;
        if (mep)
        {
            pMediaImpl = LLViewerMedia::getInstance()->getMediaImplFromTextureID(mep->getMediaID());
        }

        if (pMediaImpl.isNull())
        {
            // If we didn't find face media for this face, check whether this face is showing parcel media.
            pMediaImpl = LLViewerMedia::getInstance()->getMediaImplFromTextureID(tep.getID());
        }

        if (pMediaImpl.notNull())
        {
            LLPluginClassMedia *media = pMediaImpl->getMediaPlugin();
            if(media)
            {
                S32 media_width = media->getWidth();
                S32 media_height = media->getHeight();
                S32 texture_width = media->getTextureWidth();
                S32 texture_height = media->getTextureHeight();
                F32 scale_s = (F32)media_width / (F32)texture_width;
                F32 scale_t = (F32)media_height / (F32)texture_height;

                // set scale and adjust offset
                object->setTEScaleS( te, scale_s );
                object->setTEScaleT( te, scale_t ); // don't need to flip Y anymore since QT does this for us now.
                object->setTEOffsetS( te, -( 1.0f - scale_s ) / 2.0f );
                object->setTEOffsetT( te, -( 1.0f - scale_t ) / 2.0f );
            }
        }
        return true;
    };
};

void FSPanelFace::onClickAutoFix()
{
    FSPanelFaceSetMediaFunctor setfunc;
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);

    FSPanelFaceSendFunctor sendfunc;
    LLSelectMgr::getInstance()->getSelection()->applyToObjects(&sendfunc);
}

void FSPanelFace::onAlignTexture()
{
    alignTextureLayer();
}

void FSPanelFace::onClickBtnSavePBR()
{
    LLMaterialEditor::saveObjectsMaterialAs();
}

enum EPasteMode
{
    PASTE_COLOR,
    PASTE_TEXTURE
};

struct FSPanelFacePasteTexFunctor : public LLSelectedTEFunctor
{
    public:
        FSPanelFacePasteTexFunctor(FSPanelFace* panel, EPasteMode mode) :
            mPanelFace(panel),
            mMode(mode)
        {
        }

        virtual bool apply(LLViewerObject* objectp, S32 te)
        {
            switch(mMode)
            {
                case PASTE_COLOR:
                    mPanelFace->onPasteColor(objectp, te);
                    break;

                case PASTE_TEXTURE:
                    mPanelFace->onPasteTexture(objectp, te);
                    break;
            }
            return true;
        }

    private:
        FSPanelFace *mPanelFace;
        EPasteMode mMode;
};

struct FSPanelFaceUpdateFunctor : public LLSelectedObjectFunctor
{
    public:
        FSPanelFaceUpdateFunctor(bool update_media) :
            mUpdateMedia(update_media)
        {
        }

        virtual bool apply(LLViewerObject* object)
        {
            object->sendTEUpdate();

            if (mUpdateMedia)
            {
                LLVOVolume *vo = dynamic_cast<LLVOVolume*>(object);
                if (vo && vo->hasMedia())
                {
                    vo->sendMediaDataUpdate();
                }
            }
            return true;
        }

    private:
        bool mUpdateMedia;
};

struct FSPanelFaceNavigateHomeFunctor : public LLSelectedTEFunctor
{
    virtual bool apply(LLViewerObject* objectp, S32 te)
    {
        if (objectp && objectp->getTE(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            const LLMediaEntry *media_data = tep->getMediaData();
            if (media_data)
            {
                if (media_data->getCurrentURL().empty() && media_data->getAutoPlay())
                {
                    viewer_media_t media_impl =
                        LLViewerMedia::getInstance()->getMediaImplFromTextureID(tep->getMediaData()->getMediaID());
                    if (media_impl)
                    {
                        media_impl->navigateHome();
                    }
                }
            }
        }
        return true;
    }
};

void FSPanelFace::onCopyColor()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();

    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        return;
    }

    if (mClipboardParams.has("color"))
    {
        mClipboardParams["color"].clear();
    }
    else
    {
        mClipboardParams["color"] = LLSD::emptyArray();
    }

    std::map<LLUUID, LLUUID> asset_item_map;

    // a way to resolve situations where source and target have different amount of faces
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    mClipboardParams["color_all_tes"] = (num_tes != 1) || (LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool());
    for (S32 te = 0; te < num_tes; ++te)
    {
        if (node->isTESelected(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            if (tep)
            {
                LLSD te_data;

                // asLLSD() includes media
                te_data["te"] = tep->asLLSD(); // Note: includes a lot more than just color/alpha/glow

                mClipboardParams["color"].append(te_data);
            }
        }
    }
}

void FSPanelFace::onPasteColor()
{
    if (!mClipboardParams.has("color"))
    {
        return;
    }

    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();

    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1)
    {
        // not supposed to happen
        LL_WARNS() << "Failed to paste color due to missing or wrong selection" << LL_ENDL;
        return;
    }

    bool face_selection_mode = LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool();
    LLSD &clipboard = mClipboardParams["color"]; // array
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    S32 compare_tes = num_tes;

    if (face_selection_mode)
    {
        compare_tes = 0;
        for (S32 te = 0; te < num_tes; ++te)
        {
            if (node->isTESelected(te))
            {
                compare_tes++;
            }
        }
    }

    // we can copy if single face was copied in edit face mode or if face count matches
    if (!((clipboard.size() == 1) && mClipboardParams["color_all_tes"].asBoolean())
        && compare_tes != clipboard.size())
    {
        LLSD notif_args;
        if (face_selection_mode)
        {
            static std::string reason = getString("paste_error_face_selection_mismatch");
            notif_args["REASON"] = reason;
        }
        else
        {
            static std::string reason = getString("paste_error_object_face_count_mismatch");
            notif_args["REASON"] = reason;
        }
        LLNotificationsUtil::add("FacePasteFailed", notif_args);
        return;
    }

    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

    FSPanelFacePasteTexFunctor paste_func(this, PASTE_COLOR);
    selected_objects->applyToTEs(&paste_func);

    FSPanelFaceUpdateFunctor sendfunc(false);
    selected_objects->applyToObjects(&sendfunc);
}

void FSPanelFace::onPasteColor(LLViewerObject* objectp, S32 te)
{
    LLSD te_data;
    LLSD &clipboard = mClipboardParams["color"]; // array
    if ((clipboard.size() == 1) && mClipboardParams["color_all_tes"].asBoolean())
    {
        te_data = *(clipboard.beginArray());
    }
    else if (clipboard[te])
    {
        te_data = clipboard[te];
    }
    else
    {
        return;
    }

    LLTextureEntry* tep = objectp->getTE(te);
    if (tep)
    {
        if (te_data.has("te"))
        {
            // Color / Alpha
            if (te_data["te"].has("colors"))
            {
                LLColor4 color = tep->getColor();

                LLColor4 clip_color;
                clip_color.setValue(te_data["te"]["colors"]);

                // Color
                color.mV[VRED] = clip_color.mV[VRED];
                color.mV[VGREEN] = clip_color.mV[VGREEN];
                color.mV[VBLUE] = clip_color.mV[VBLUE];

                // Alpha
                color.mV[VALPHA] = clip_color.mV[VALPHA];

                objectp->setTEColor(te, color);
            }

            // Color/fullbright
            if (te_data["te"].has("fullbright"))
            {
                objectp->setTEFullbright(te, te_data["te"]["fullbright"].asInteger());
            }

            // Glow
            if (te_data["te"].has("glow"))
            {
                objectp->setTEGlow(te, (F32)te_data["te"]["glow"].asReal());
            }
        }
    }
}

void FSPanelFace::onCopyTexture()
{
    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();

    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1
        || !LLMaterialEditor::canClipboardObjectsMaterial())
    {
        return;
    }

    if (mClipboardParams.has("texture"))
    {
        mClipboardParams["texture"].clear();
    }
    else
    {
        mClipboardParams["texture"] = LLSD::emptyArray();
    }

    std::map<LLUUID, LLUUID> asset_item_map;

    // a way to resolve situations where source and target have different amount of faces
    S32 num_tes = llmin((S32)objectp->getNumTEs(), objectp->getNumFaces());
    mClipboardParams["texture_all_tes"] = (num_tes != 1) || (LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool());
    for (S32 te = 0; te < num_tes; ++te)
    {
        if (node->isTESelected(te))
        {
            LLTextureEntry* tep = objectp->getTE(te);
            if (tep)
            {
                LLSD te_data;

                // asLLSD() includes media
                te_data["te"] = tep->asLLSD();
                te_data["te"]["shiny"] = tep->getShiny();
                te_data["te"]["bumpmap"] = tep->getBumpmap();
                te_data["te"]["bumpshiny"] = tep->getBumpShiny();
                te_data["te"]["bumpfullbright"] = tep->getBumpShinyFullbright();
                te_data["te"]["texgen"] = tep->getTexGen();
                te_data["te"]["pbr"] = objectp->getRenderMaterialID(te);
                if (tep->getGLTFMaterialOverride() != nullptr)
                {
                    te_data["te"]["pbr_override"] = tep->getGLTFMaterialOverride()->asJSON();
                }

                if (te_data["te"].has("imageid"))
                {
                    LLUUID item_id;
                    LLUUID id = te_data["te"]["imageid"].asUUID();
                    bool from_library = get_is_predefined_texture(id);
                    bool full_perm = from_library;

                    if (!full_perm
                        && objectp->permCopy()
                        && objectp->permTransfer()
                        && objectp->permModify())
                    {
                        // If agent created this object and nothing is limiting permissions, mark as full perm
                        // If agent was granted permission to edit objects owned and created by somebody else, mark full perm
                        // This check is not perfect since we can't figure out whom textures belong to so this ended up restrictive
                        std::string creator_app_link;
                        LLUUID creator_id;
                        LLSelectMgr::getInstance()->selectGetCreator(creator_id, creator_app_link);
                        full_perm = objectp->mOwnerID == creator_id;
                    }

                    if (id.notNull() && !full_perm)
                    {
                        std::map<LLUUID, LLUUID>::iterator iter = asset_item_map.find(id);
                        if (iter != asset_item_map.end())
                        {
                            item_id = iter->second;
                        }
                        else
                        {
                            // What this does is simply searches inventory for item with same asset id,
                            // as result it is highly unreliable, leaves little control to user, borderline hack
                            // but there are little options to preserve permissions - multiple inventory
                            // items might reference same asset and inventory search is expensive.
                            bool no_transfer = false;
                            if (objectp->getInventoryItemByAsset(id))
                            {
                                no_transfer = !objectp->getInventoryItemByAsset(id)->getIsFullPerm();
                            }
                            item_id = get_copy_free_item_by_asset_id(id, no_transfer);
                            // record value to avoid repeating inventory search when possible
                            asset_item_map[id] = item_id;
                        }
                    }

                    if (item_id.notNull() && gInventory.isObjectDescendentOf(item_id, gInventory.getLibraryRootFolderID()))
                    {
                        full_perm = true;
                        from_library = true;
                    }

                    // TODO: why scope this? -Zi
                    {
                        te_data["te"]["itemfullperm"] = full_perm;
                        te_data["te"]["fromlibrary"] = from_library;

                        // If full permission object, texture is free to copy,
                        // but otherwise we need to check inventory and extract permissions
                        //
                        // Normally we care only about restrictions for current user and objects
                        // don't inherit any 'next owner' permissions from texture, so there is
                        // no need to record item id if full_perm==true
                        if (!full_perm && !from_library && item_id.notNull())
                        {
                            LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                            if (itemp)
                            {
                                LLPermissions item_permissions = itemp->getPermissions();
                                if (item_permissions.allowOperationBy(PERM_COPY,
                                    gAgent.getID(),
                                    gAgent.getGroupID()))
                                {
                                    te_data["te"]["imageitemid"] = item_id;
                                    te_data["te"]["itemfullperm"] = itemp->getIsFullPerm();
                                    if (!itemp->isFinished())
                                    {
                                        // needed for dropTextureAllFaces
                                        LLInventoryModelBackgroundFetch::instance().start(item_id, false);
                                    }
                                }
                            }
                        }
                    }
                }

                LLMaterialPtr material_ptr = tep->getMaterialParams();
                if (!material_ptr.isNull())
                {
                    LLSD mat_data;

                    mat_data["NormMap"] = material_ptr->getNormalID();
                    mat_data["SpecMap"] = material_ptr->getSpecularID();

                    mat_data["NormRepX"] = material_ptr->getNormalRepeatX();
                    mat_data["NormRepY"] = material_ptr->getNormalRepeatY();
                    mat_data["NormOffX"] = material_ptr->getNormalOffsetX();
                    mat_data["NormOffY"] = material_ptr->getNormalOffsetY();
                    mat_data["NormRot"] = material_ptr->getNormalRotation();

                    mat_data["SpecRepX"] = material_ptr->getSpecularRepeatX();
                    mat_data["SpecRepY"] = material_ptr->getSpecularRepeatY();
                    mat_data["SpecOffX"] = material_ptr->getSpecularOffsetX();
                    mat_data["SpecOffY"] = material_ptr->getSpecularOffsetY();
                    mat_data["SpecRot"] = material_ptr->getSpecularRotation();

                    mat_data["SpecColor"] = material_ptr->getSpecularLightColor().getValue();
                    mat_data["SpecExp"] = material_ptr->getSpecularLightExponent();
                    mat_data["EnvIntensity"] = material_ptr->getEnvironmentIntensity();
                    mat_data["AlphaMaskCutoff"] = material_ptr->getAlphaMaskCutoff();
                    mat_data["DiffuseAlphaMode"] = material_ptr->getDiffuseAlphaMode();

                    // Replace no-copy textures, destination texture will get used instead if available
                    if (mat_data.has("NormMap"))
                    {
                        LLUUID id = mat_data["NormMap"].asUUID();
                        if (id.notNull() && !get_can_copy_texture(id))
                        {
                            mat_data["NormMap"] = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
                            mat_data["NormMapNoCopy"] = true;
                        }
                    }
                    if (mat_data.has("SpecMap"))
                    {
                        LLUUID id = mat_data["SpecMap"].asUUID();
                        if (id.notNull() && !get_can_copy_texture(id))
                        {
                            mat_data["SpecMap"] = LLUUID(gSavedSettings.getString("DefaultObjectTexture"));
                            mat_data["SpecMapNoCopy"] = true;
                        }
                    }

                    te_data["material"] = mat_data;
                }

                mClipboardParams["texture"].append(te_data);
            }
        }
    }
}

void FSPanelFace::onPasteTexture()
{
    if (!mClipboardParams.has("texture"))
    {
        return;
    }

    LLViewerObject* objectp = LLSelectMgr::getInstance()->getSelection()->getFirstObject();

    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    S32 selected_count = LLSelectMgr::getInstance()->getSelection()->getObjectCount();

    if (!objectp || !node
        || objectp->getPCode() != LL_PCODE_VOLUME
        || !objectp->permModify()
        || objectp->isPermanentEnforced()
        || selected_count > 1
        || !LLMaterialEditor::canClipboardObjectsMaterial())
    {
        // not supposed to happen
        LL_WARNS() << "Failed to paste texture due to missing or wrong selection" << LL_ENDL;
        return;
    }

    bool face_selection_mode = LLToolFace::getInstance() == LLToolMgr::getInstance()->getCurrentTool();
    LLSD &clipboard = mClipboardParams["texture"]; // array
    S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
    S32 compare_tes = num_tes;

    if (face_selection_mode)
    {
        compare_tes = 0;
        for (S32 te = 0; te < num_tes; ++te)
        {
            if (node->isTESelected(te))
            {
                compare_tes++;
            }
        }
    }

    // we can copy if single face was copied in edit face mode or if face count matches
    if (!((clipboard.size() == 1) && mClipboardParams["texture_all_tes"].asBoolean())
        && compare_tes != clipboard.size())
    {
        LLSD notif_args;
        if (face_selection_mode)
        {
            static std::string reason = getString("paste_error_face_selection_mismatch");
            notif_args["REASON"] = reason;
        }
        else
        {
            static std::string reason = getString("paste_error_object_face_count_mismatch");
            notif_args["REASON"] = reason;
        }

        LLNotificationsUtil::add("FacePasteFailed", notif_args);
        return;
    }

    bool full_perm_object = true;

    LLSD::array_const_iterator iter = clipboard.beginArray();
    LLSD::array_const_iterator end = clipboard.endArray();

    for (; iter != end; ++iter)
    {
        const LLSD& te_data = *iter;
        if (te_data.has("te") && te_data["te"].has("imageid"))
        {
            bool full_perm = te_data["te"].has("itemfullperm") && te_data["te"]["itemfullperm"].asBoolean();
            full_perm_object &= full_perm;
            if (!full_perm)
            {
                if (te_data["te"].has("imageitemid"))
                {
                    LLUUID item_id = te_data["te"]["imageitemid"].asUUID();
                    if (item_id.notNull())
                    {
                        LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                        if (!itemp)
                        {
                            // image might be in object's inventory, but it can be not up to date
                            LLSD notif_args;
                            static std::string reason = getString("paste_error_inventory_not_found");
                            notif_args["REASON"] = reason;
                            LLNotificationsUtil::add("FacePasteFailed", notif_args);
                            return;
                        }
                    }
                }
                else
                {
                    // Item was not found on 'copy' stage
                    // Since this happened at copy, might be better to either show this
                    // at copy stage or to drop clipboard here
                    LLSD notif_args;
                    static std::string reason = getString("paste_error_inventory_not_found");
                    notif_args["REASON"] = reason;
                    LLNotificationsUtil::add("FacePasteFailed", notif_args);
                    return;
                }
            }
        }
    }

    if (!full_perm_object)
    {
        LLNotificationsUtil::add("FacePasteTexturePermissions");
    }

    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

    FSPanelFacePasteTexFunctor paste_func(this, PASTE_TEXTURE);
    selected_objects->applyToTEs(&paste_func);

    FSPanelFaceUpdateFunctor sendfunc(true);
    selected_objects->applyToObjects(&sendfunc);

    LLGLTFMaterialList::flushUpdates();

    FSPanelFaceNavigateHomeFunctor navigate_home_func;
    selected_objects->applyToTEs(&navigate_home_func);
}

void FSPanelFace::onPasteTexture(LLViewerObject* objectp, S32 te)
{
    LLSD te_data;
    LLSD &clipboard = mClipboardParams["texture"]; // array
    if ((clipboard.size() == 1) && mClipboardParams["texture_all_tes"].asBoolean())
    {
        te_data = *(clipboard.beginArray());
    }
    else if (clipboard[te])
    {
        te_data = clipboard[te];
    }
    else
    {
        return;
    }

    LLTextureEntry* tep = objectp->getTE(te);
    if (tep)
    {
        if (te_data.has("te"))
        {
            // Texture
            bool full_perm = te_data["te"].has("itemfullperm") && te_data["te"]["itemfullperm"].asBoolean();
            bool from_library = te_data["te"].has("fromlibrary") && te_data["te"]["fromlibrary"].asBoolean();
            if (te_data["te"].has("imageid"))
            {
                const LLUUID& imageid = te_data["te"]["imageid"].asUUID(); //texture or asset id
                LLViewerInventoryItem* itemp_res = NULL;

                if (te_data["te"].has("imageitemid"))
                {
                    LLUUID item_id = te_data["te"]["imageitemid"].asUUID();
                    if (item_id.notNull())
                    {
                        LLViewerInventoryItem* itemp = gInventory.getItem(item_id);
                        if (itemp && itemp->isFinished())
                        {
                            // dropTextureAllFaces will fail if incomplete
                            itemp_res = itemp;
                        }
                        else
                        {
                            // Theoretically shouldn't happen, but if it does happen, we
                            // might need to add a notification to user that paste will fail
                            // since inventory isn't fully loaded
                            LL_WARNS() << "Item " << item_id << " is incomplete, paste might fail silently." << LL_ENDL;
                        }
                    }
                }
                // for case when item got removed from inventory after we pressed 'copy'
                // or texture got pasted into previous object
                if (!itemp_res && !full_perm)
                {
                    // Due to checks for imageitemid in FSPanelFace::onPasteTexture() this should no longer be reachable.
                    LL_INFOS() << "Item " << te_data["te"]["imageitemid"].asUUID() << " no longer in inventory." << LL_ENDL;
                    // Todo: fix this, we are often searching same texture multiple times (equal to number of faces)
                    // Perhaps just mPanelFace->onPasteTexture(objectp, te, &asset_to_item_id_map); ? Not pretty, but will work
                    LLViewerInventoryCategory::cat_array_t cats;
                    LLViewerInventoryItem::item_array_t items;
                    LLAssetIDMatches asset_id_matches(imageid);
                    gInventory.collectDescendentsIf(LLUUID::null,
                        cats,
                        items,
                        LLInventoryModel::INCLUDE_TRASH,
                        asset_id_matches);

                    // Extremely unreliable and performance unfriendly.
                    // But we need this to check permissions and it is how texture control finds items
                    for (S32 i = 0; i < items.size(); i++)
                    {
                        LLViewerInventoryItem* itemp = items[i];
                        if (itemp && itemp->isFinished())
                        {
                            // dropTextureAllFaces will fail if incomplete
                            LLPermissions item_permissions = itemp->getPermissions();
                            if (item_permissions.allowOperationBy(PERM_COPY,
                                gAgent.getID(),
                                gAgent.getGroupID()))
                            {
                                itemp_res = itemp;
                                break; // first match
                            }
                        }
                    }
                }

                if (itemp_res)
                {
                    if (te == -1) // all faces
                    {
                        LLToolDragAndDrop::dropTextureAllFaces(objectp,
                            itemp_res,
                            from_library ? LLToolDragAndDrop::SOURCE_LIBRARY : LLToolDragAndDrop::SOURCE_AGENT,
                            LLUUID::null,
                            false);
                    }
                    else // one face
                    {
                        LLToolDragAndDrop::dropTextureOneFace(objectp,
                            te,
                            itemp_res,
                            from_library ? LLToolDragAndDrop::SOURCE_LIBRARY : LLToolDragAndDrop::SOURCE_AGENT,
                            LLUUID::null,
                            false,
                            0);
                    }
                }
                // not an inventory item or no complete items
                else if (full_perm)
                {
                    // Either library, local or existed as fullperm when user made a copy
                    LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(imageid, FTT_DEFAULT, true, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
                    objectp->setTEImage(U8(te), image);
                }
            }

            if (te_data["te"].has("bumpmap"))
            {
                objectp->setTEBumpmap(te, (U8)te_data["te"]["bumpmap"].asInteger());
            }
            if (te_data["te"].has("bumpshiny"))
            {
                objectp->setTEBumpShiny(te, (U8)te_data["te"]["bumpshiny"].asInteger());
            }
            if (te_data["te"].has("bumpfullbright"))
            {
                objectp->setTEBumpShinyFullbright(te, (U8)te_data["te"]["bumpfullbright"].asInteger());
            }
            if (te_data["te"].has("texgen"))
            {
                objectp->setTETexGen(te, (U8)te_data["te"]["texgen"].asInteger());
            }

            // PBR/GLTF
            if (te_data["te"].has("pbr"))
            {
                objectp->setRenderMaterialID(te, te_data["te"]["pbr"].asUUID(), false /*managing our own update*/);
                tep->setGLTFRenderMaterial(nullptr);
                tep->setGLTFMaterialOverride(nullptr);

                LLSD override_data;
                override_data["object_id"] = objectp->getID();
                override_data["side"] = te;
                if (te_data["te"].has("pbr_override"))
                {
                    override_data["gltf_json"] = te_data["te"]["pbr_override"];
                }
                else
                {
                    override_data["gltf_json"] = "";
                }

                override_data["asset_id"] = te_data["te"]["pbr"].asUUID();

                LLGLTFMaterialList::queueUpdate(override_data);
            }
            else
            {
                objectp->setRenderMaterialID(te, LLUUID::null, false /*send in bulk later*/ );
                tep->setGLTFRenderMaterial(nullptr);
                tep->setGLTFMaterialOverride(nullptr);

                // blank out most override data on the server
                LLGLTFMaterialList::queueApply(objectp, te, LLUUID::null);
            }

            // Texture map
            if (te_data["te"].has("scales") && te_data["te"].has("scalet"))
            {
                objectp->setTEScale(te, (F32)te_data["te"]["scales"].asReal(), (F32)te_data["te"]["scalet"].asReal());
            }
            if (te_data["te"].has("offsets") && te_data["te"].has("offsett"))
            {
                objectp->setTEOffset(te, (F32)te_data["te"]["offsets"].asReal(), (F32)te_data["te"]["offsett"].asReal());
            }
            if (te_data["te"].has("imagerot"))
            {
                objectp->setTERotation(te, (F32)te_data["te"]["imagerot"].asReal());
            }

            // Media
            if (te_data["te"].has("media_flags"))
            {
                U8 media_flags = te_data["te"]["media_flags"].asInteger();
                objectp->setTEMediaFlags(te, media_flags);
                LLVOVolume *vo = dynamic_cast<LLVOVolume*>(objectp);
                if (vo && te_data["te"].has(LLTextureEntry::TEXTURE_MEDIA_DATA_KEY))
                {
                    vo->syncMediaData(te, te_data["te"][LLTextureEntry::TEXTURE_MEDIA_DATA_KEY], true/*merge*/, true/*ignore_agent*/);
                }
            }
            else
            {
                // Keep media flags on destination unchanged
            }
        }

        if (te_data.has("material"))
        {
            LLUUID object_id = objectp->getID();

            // Normal
            // Replace placeholders with target's
            if (te_data["material"].has("NormMapNoCopy"))
            {
                LLMaterialPtr material = tep->getMaterialParams();
                if (material.notNull())
                {
                    LLUUID id = material->getNormalID();
                    if (id.notNull())
                    {
                        te_data["material"]["NormMap"] = id;
                    }
                }
            }

            LLSelectedTEMaterial::setNormalID(this, te_data["material"]["NormMap"].asUUID(), te, object_id);
            LLSelectedTEMaterial::setNormalRepeatX(this, (F32)te_data["material"]["NormRepX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalRepeatY(this, (F32)te_data["material"]["NormRepY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalOffsetX(this, (F32)te_data["material"]["NormOffX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalOffsetY(this, (F32)te_data["material"]["NormOffY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setNormalRotation(this, (F32)te_data["material"]["NormRot"].asReal(), te, object_id);

            // Specular
            // Replace placeholders with target's
            if (te_data["material"].has("SpecMapNoCopy"))
            {
                LLMaterialPtr material = tep->getMaterialParams();
                if (material.notNull())
                {
                    LLUUID id = material->getSpecularID();
                    if (id.notNull())
                    {
                        te_data["material"]["SpecMap"] = id;
                    }
                }
            }

            LLSelectedTEMaterial::setSpecularID(this, te_data["material"]["SpecMap"].asUUID(), te, object_id);
            LLSelectedTEMaterial::setSpecularRepeatX(this, (F32)te_data["material"]["SpecRepX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularRepeatY(this, (F32)te_data["material"]["SpecRepY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularOffsetX(this, (F32)te_data["material"]["SpecOffX"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularOffsetY(this, (F32)te_data["material"]["SpecOffY"].asReal(), te, object_id);
            LLSelectedTEMaterial::setSpecularRotation(this, (F32)te_data["material"]["SpecRot"].asReal(), te, object_id);
            LLColor4U spec_color(te_data["material"]["SpecColor"]);
            LLSelectedTEMaterial::setSpecularLightColor(this, spec_color, te);
            LLSelectedTEMaterial::setSpecularLightExponent(this, (U8)te_data["material"]["SpecExp"].asInteger(), te, object_id);
            LLSelectedTEMaterial::setEnvironmentIntensity(this, (U8)te_data["material"]["EnvIntensity"].asInteger(), te, object_id);
            LLSelectedTEMaterial::setDiffuseAlphaMode(this, (U8)te_data["material"]["DiffuseAlphaMode"].asInteger(), te, object_id);
            LLSelectedTEMaterial::setAlphaMaskCutoff(this, (U8)te_data["material"]["AlphaMaskCutoff"].asInteger(), te, object_id);

            if (te_data.has("te") && te_data["te"].has("shiny"))
            {
                objectp->setTEShiny(te, (U8)te_data["te"]["shiny"].asInteger());
            }
        }
    }
}

void FSPanelFace::onCopyFaces()
{
    onCopyTexture();
    onCopyColor();
    mBtnPasteFaces->setEnabled(!mClipboardParams.emptyMap() && (mClipboardParams.has("color") || mClipboardParams.has("texture")));
}

void FSPanelFace::onPasteFaces()
{
    LLObjectSelectionHandle selected_objects = LLSelectMgr::getInstance()->getSelection();

    FSPanelFacePasteTexFunctor paste_texture_func(this, PASTE_TEXTURE);
    selected_objects->applyToTEs(&paste_texture_func);

    FSPanelFacePasteTexFunctor paste_color_func(this, PASTE_COLOR);
    selected_objects->applyToTEs(&paste_color_func);

    FSPanelFaceUpdateFunctor sendfunc(true);
    selected_objects->applyToObjects(&sendfunc);

    FSPanelFaceNavigateHomeFunctor navigate_home_func;
    selected_objects->applyToTEs(&navigate_home_func);
}

void FSPanelFace::onCommitPlanarAlign()
{
    getState();
    sendTextureInfo();
}

void FSPanelFace::updateGLTFTextureTransform(const LLGLTFMaterial::TextureInfo texture_info, std::function<void(LLGLTFMaterial::TextureTransform*)> edit)
{
    if (texture_info == LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT)
    {
        updateSelectedGLTFMaterials([&](LLGLTFMaterial* new_override)
        {
            for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
            {
                LLGLTFMaterial::TextureTransform& new_transform = new_override->mTextureTransform[(LLGLTFMaterial::TextureInfo)i];
                edit(&new_transform);
            }
        });
    }
    else
    {
        updateSelectedGLTFMaterials([&](LLGLTFMaterial* new_override)
        {
            LLGLTFMaterial::TextureTransform& new_transform = new_override->mTextureTransform[texture_info];
            edit(&new_transform);
        });
    }
}

void FSPanelFace::updateGLTFTextureTransformWithScale(const LLGLTFMaterial::TextureInfo texture_info, std::function<void(LLGLTFMaterial::TextureTransform*, const F32, const F32)> edit)
{
    if (texture_info == LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT)
    {
        updateSelectedGLTFMaterialsWithScale([&](LLGLTFMaterial* new_override, const F32 scale_s, const F32 scale_t)
        {
            for (U32 i = 0; i < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++i)
            {
                LLGLTFMaterial::TextureTransform& new_transform = new_override->mTextureTransform[(LLGLTFMaterial::TextureInfo)i];
                edit(&new_transform, scale_s, scale_t);
            }
        });
    }
    else
    {
        updateSelectedGLTFMaterialsWithScale([&](LLGLTFMaterial* new_override, const F32 scale_s, const F32 scale_t)
        {
            LLGLTFMaterial::TextureTransform& new_transform = new_override->mTextureTransform[texture_info];
            edit(&new_transform, scale_s, scale_t);
        });
    }
}

void FSPanelFace::setMaterialOverridesFromSelection()
{
    // TODO: move to .h -Zi
    static std::map<LLGLTFMaterial::TextureInfo, std::string> spinner_suffixes{
        { LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_BASE_COLOR, "_Base" },
        { LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_NORMAL, "_Normal" },
        { LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS, "_Metallic" },
        { LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_EMISSIVE, "_Emissive" }
    };

    bool read_transform = true;
    LLGLTFMaterial::TextureTransform transform;
    bool scale_u_same = true;
    bool scale_v_same = true;
    bool rotation_same = true;
    bool offset_u_same = true;
    bool offset_v_same = true;

    // Always iterate over the whole set of texture channels -Zi
    for (U32 i = 0; i < LLGLTFMaterial::TextureInfo::GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        const std::string& spinner_suffix = spinner_suffixes[(LLGLTFMaterial::TextureInfo) i];

        LLGLTFMaterial::TextureTransform this_transform;

        bool this_scale_u_same = true;
        bool this_scale_v_same = true;
        bool this_rotation_same = true;
        bool this_offset_u_same = true;
        bool this_offset_v_same = true;

        readSelectedGLTFMaterial<float>([&](const LLGLTFMaterial* mat)
        {
            return mat ? mat->mTextureTransform[i].mScale[VX] : 0.f;
        }, this_transform.mScale[VX], this_scale_u_same, true, 1e-3f);

        readSelectedGLTFMaterial<float>([&](const LLGLTFMaterial* mat)
        {
            return mat ? mat->mTextureTransform[i].mScale[VY] : 0.f;
        }, this_transform.mScale[VY], this_scale_v_same, true, 1e-3f);

        readSelectedGLTFMaterial<float>([&](const LLGLTFMaterial* mat)
        {
            return mat ? mat->mTextureTransform[i].mRotation : 0.f;
        }, this_transform.mRotation, this_rotation_same, true, 1e-3f);

        readSelectedGLTFMaterial<float>([&](const LLGLTFMaterial* mat)
        {
            return mat ? mat->mTextureTransform[i].mOffset[VX] : 0.f;
        }, this_transform.mOffset[VX], this_offset_u_same, true, 1e-3f);

        readSelectedGLTFMaterial<float>([&](const LLGLTFMaterial* mat)
        {
            return mat ? mat->mTextureTransform[i].mOffset[VY] : 0.f;
        }, this_transform.mOffset[VY], this_offset_v_same, true, 1e-3f);

        scale_u_same = scale_u_same && this_scale_u_same;
        scale_v_same = scale_v_same && this_scale_v_same;
        rotation_same = rotation_same && this_rotation_same;
        offset_u_same = offset_u_same && this_offset_u_same;
        offset_v_same = offset_v_same && this_offset_v_same;

        if (read_transform)
        {
            read_transform = false;
            transform = this_transform;
        }
        else
        {
            scale_u_same = scale_u_same && (this_transform.mScale[VX] == transform.mScale[VX]);
            scale_v_same = scale_v_same && (this_transform.mScale[VY] == transform.mScale[VY]);
            rotation_same = rotation_same && (this_transform.mRotation == transform.mRotation);
            offset_u_same = offset_u_same && (this_transform.mOffset[VX] == transform.mOffset[VX]);
            offset_v_same = offset_v_same && (this_transform.mOffset[VY] == transform.mOffset[VY]);
        }

        LLUICtrl* gltfCtrlTextureScaleU = findChild<LLUICtrl>("gltfTextureScaleU" + spinner_suffix);
        LLUICtrl* gltfCtrlTextureScaleV = findChild<LLUICtrl>("gltfTextureScaleV" + spinner_suffix);
        LLUICtrl* gltfCtrlTextureRotation = findChild<LLUICtrl>("gltfTextureRotation" + spinner_suffix);
        LLUICtrl* gltfCtrlTextureOffsetU = findChild<LLUICtrl>("gltfTextureOffsetU" + spinner_suffix);
        LLUICtrl* gltfCtrlTextureOffsetV = findChild<LLUICtrl>("gltfTextureOffsetV" + spinner_suffix);

        gltfCtrlTextureScaleU->setValue(this_transform.mScale[VX]);
        gltfCtrlTextureScaleV->setValue(this_transform.mScale[VY]);
        gltfCtrlTextureRotation->setValue(this_transform.mRotation * RAD_TO_DEG);
        gltfCtrlTextureOffsetU->setValue(this_transform.mOffset[VX]);
        gltfCtrlTextureOffsetV->setValue(this_transform.mOffset[VY]);
    }

    LLUICtrl* gltfCtrlTextureScaleU = findChild<LLUICtrl>("gltfTextureScaleU_All");
    LLUICtrl* gltfCtrlTextureScaleV = findChild<LLUICtrl>("gltfTextureScaleV_All");
    LLUICtrl* gltfCtrlTextureRotation = findChild<LLUICtrl>("gltfTextureRotation_All");
    LLUICtrl* gltfCtrlTextureOffsetU = findChild<LLUICtrl>("gltfTextureOffsetU_All");
    LLUICtrl* gltfCtrlTextureOffsetV = findChild<LLUICtrl>("gltfTextureOffsetV_All");

    gltfCtrlTextureScaleU->setValue(transform.mScale[VX]);
    gltfCtrlTextureScaleV->setValue(transform.mScale[VY]);
    gltfCtrlTextureRotation->setValue(transform.mRotation * RAD_TO_DEG);
    gltfCtrlTextureOffsetU->setValue(transform.mOffset[VX]);
    gltfCtrlTextureOffsetV->setValue(transform.mOffset[VY]);

    gltfCtrlTextureScaleU->setTentative(!scale_u_same);
    gltfCtrlTextureScaleV->setTentative(!scale_v_same);
    gltfCtrlTextureRotation->setTentative(!rotation_same);
    gltfCtrlTextureOffsetU->setTentative(!offset_u_same);
    gltfCtrlTextureOffsetV->setTentative(!offset_v_same);

    // Fixes some UI desync
    if (getCurrentMaterialType() == MATMEDIA_PBR)
    {
        F32 repeats = 1.f;
        bool identical = false;
        getSelectedGLTFMaterialMaxRepeats(getPBRDropChannel(), repeats, identical);
        mCtrlRpt->forceSetValue(repeats);
        mCtrlRpt->setTentative(!identical);
    }
}

void FSPanelFace::Selection::connect()
{
    if (!mSelectConnection.connected())
    {
        mSelectConnection = LLSelectMgr::instance().mUpdateSignal.connect(boost::bind(&FSPanelFace::Selection::onSelectionChanged, this));
    }
}

bool FSPanelFace::Selection::update()
{
    const bool changed = mChanged || compareSelection();
    mChanged = false;
    return changed;
}

void FSPanelFace::Selection::onSelectedObjectUpdated(const LLUUID& object_id, S32 side)
{
    if (object_id == mSelectedObjectID)
    {
        if (side == mLastSelectedSide)
        {
            mChanged = true;
        }
        else if (mLastSelectedSide == -1) // if last selected face was deselected
        {
            LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
            if (node && node->isTESelected(side))
            {
                mChanged = true;
            }
        }
    }
}

bool FSPanelFace::Selection::compareSelection()
{
    if (!mNeedsSelectionCheck)
    {
        return false;
    }

    mNeedsSelectionCheck = false;

    const S32 old_object_count = mSelectedObjectCount;
    const S32 old_te_count = mSelectedTECount;
    const LLUUID old_object_id = mSelectedObjectID;
    const S32 old_side = mLastSelectedSide;

    LLObjectSelectionHandle selection = LLSelectMgr::getInstance()->getSelection();
    LLSelectNode* node = selection->getFirstNode();
    if (node)
    {
        LLViewerObject* object = node->getObject();
        mSelectedObjectCount = selection->getObjectCount();
        mSelectedTECount = selection->getTECount();
        mSelectedObjectID = object->getID();
        mLastSelectedSide = node->getLastSelectedTE();
    }
    else
    {
        mSelectedObjectCount = 0;
        mSelectedTECount = 0;
        mSelectedObjectID = LLUUID::null;
        mLastSelectedSide = -1;
    }

    const bool selection_changed =
        old_object_count != mSelectedObjectCount
        || old_te_count != mSelectedTECount
        || old_object_id != mSelectedObjectID
        || old_side != mLastSelectedSide;

    mChanged = mChanged || selection_changed;

    return selection_changed;
}

void FSPanelFace::onCommitGLTFUVSpinner(LLUICtrl* ctrl, const LLSD& user_data)
{
    // TODO: put into .h -Zi
    static std::map<std::string, LLGLTFMaterial::TextureInfo> types =
    {
        { "all",        LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT },
        { "base",       LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR },
        { "normal",     LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL },
        { "metallic",   LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS },
        { "emissive",   LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE }
    };

    std::string user_data_string = user_data.asString();

    if (!types.count(user_data_string))
    {
        LL_WARNS() << "Unknown PBR channel " << user_data_string << LL_ENDL;
        return;
    }

    const LLGLTFMaterial::TextureInfo pbr_channel = types[user_data.asString()];

    const std::string& spinner_name = ctrl->getName();
    const float value = (F32)ctrl->getValue().asReal();
    ctrl->setFocus(true);

    if (LLStringUtil::startsWith(spinner_name, "gltfTextureScaleU"))
    {
        updateGLTFTextureTransform(pbr_channel, [&](LLGLTFMaterial::TextureTransform* new_transform)
        {
            new_transform->mScale.mV[VX] = value;
        });
    }
    else if (LLStringUtil::startsWith(spinner_name, "gltfTextureScaleV"))
    {
        updateGLTFTextureTransform(pbr_channel, [&](LLGLTFMaterial::TextureTransform* new_transform)
        {
            new_transform->mScale.mV[VY] = value;
        });
    }
    else if (LLStringUtil::startsWith(spinner_name, "gltfTextureRotation"))
    {
        updateGLTFTextureTransform(pbr_channel, [&](LLGLTFMaterial::TextureTransform* new_transform)
        {
            new_transform->mRotation = value * DEG_TO_RAD;
        });
    }
    else if (LLStringUtil::startsWith(spinner_name, "gltfTextureOffsetU"))
    {
        updateGLTFTextureTransform(pbr_channel, [&](LLGLTFMaterial::TextureTransform* new_transform)
        {
            new_transform->mOffset.mV[VX] = value;
        });
    }
    else if (LLStringUtil::startsWith(spinner_name, "gltfTextureOffsetV"))
    {
        updateGLTFTextureTransform(pbr_channel, [&](LLGLTFMaterial::TextureTransform* new_transform)
        {
            new_transform->mOffset.mV[VY] = value;
        });
    }
}

// selection inside the texture/material picker changed
void FSPanelFace::onTextureSelectionChanged(LLTextureCtrl* texture_ctrl)
{
    LL_DEBUGS("Materials") << "control " << texture_ctrl->getName() << LL_ENDL;

    LLInventoryItem* itemp = gInventory.getItem(texture_ctrl->getImageItemID());
    if (!itemp)
    {
        // no inventory asset available, i.e. could be "Blank"
        return;
    }

    LL_WARNS("Materials") << "item inventory id " << itemp->getUUID() << " - item asset " << itemp->getAssetUUID() << LL_ENDL;

    LLUUID obj_owner_id;
    std::string obj_owner_name;
    LLSelectMgr::instance().selectGetOwner(obj_owner_id, obj_owner_name);

    LLSaleInfo sale_info;
    LLSelectMgr::instance().selectGetSaleInfo(sale_info);

    bool can_copy = itemp->getPermissions().allowCopyBy(gAgentID); // do we have perm to copy this texture?
    bool can_transfer = itemp->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID); // do we have perm to transfer this texture?
    bool is_object_owner = gAgentID == obj_owner_id; // does object for which we are going to apply texture belong to the agent?
    bool not_for_sale = !sale_info.isForSale(); // is object for which we are going to apply texture not for sale?

    if (can_copy && can_transfer)
    {
        texture_ctrl->setCanApply(true, true);
        return;
    }

    // if texture has (no-transfer) attribute it can be applied only for object which we own and is not for sale
    texture_ctrl->setCanApply(false, can_transfer ? true : is_object_owner && not_for_sale);

    if (gSavedSettings.getBOOL("TextureLivePreview"))
    {
        LLNotificationsUtil::add("LivePreviewUnavailable");
    }
}

// selection inside the texture/material picker changed
void FSPanelFace::onPbrSelectionChanged(LLInventoryItem* itemp)
{
    LLUUID obj_owner_id;
    std::string obj_owner_name;
    LLSelectMgr::instance().selectGetOwner(obj_owner_id, obj_owner_name);

    LLSaleInfo sale_info;
    LLSelectMgr::instance().selectGetSaleInfo(sale_info);

    bool can_copy = itemp->getPermissions().allowCopyBy(gAgentID); // do we have perm to copy this material?
    bool can_transfer = itemp->getPermissions().allowOperationBy(PERM_TRANSFER, gAgentID); // do we have perm to transfer this material?
    bool can_modify = itemp->getPermissions().allowOperationBy(PERM_MODIFY, gAgentID); // do we have perm to transfer this material?
    bool is_object_owner = gAgentID == obj_owner_id; // does object for which we are going to apply material belong to the agent?
    bool not_for_sale = !sale_info.isForSale(); // is object for which we are going to apply material not for sale?

    if (can_copy && can_transfer && can_modify)
    {
        mMaterialCtrlPBR->setCanApply(true, true);
        return;
    }

    // if material has (no-transfer) attribute it can be applied only for object which we own and is not for sale
    mMaterialCtrlPBR->setCanApply(false, can_transfer ? true : is_object_owner && not_for_sale);

    if (gSavedSettings.getBOOL("TextureLivePreview"))
    {
        LLNotificationsUtil::add("LivePreviewUnavailablePBR");
    }
}

bool FSPanelFace::isIdenticalPlanarTexgen()
{
    LLTextureEntry::e_texgen selected_texgen = LLTextureEntry::TEX_GEN_DEFAULT;
    bool identical_texgen = false;
    LLSelectedTE::getTexGen(selected_texgen, identical_texgen);
    return (identical_texgen && (selected_texgen == LLTextureEntry::TEX_GEN_PLANAR));
}

bool FSPanelFace::isMediaTexSelected()
{
    LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode();
    if (LLViewerObject* objectp = node->getObject())
    {
        S32 num_tes = llmin((S32)objectp->getNumTEs(), (S32)objectp->getNumFaces());
        for (S32 te = 0; te < num_tes; ++te)
        {
            if (node->isTESelected(te))
            {
                if (objectp->getTE(te) && objectp->getTE(te)->hasMedia())
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void FSPanelFace::LLSelectedTE::getFace(LLFace*& face_to_return, bool& identical_face)
{
    struct LLSelectedTEGetFace : public LLSelectedTEGetFunctor<LLFace *>
    {
        LLFace* get(LLViewerObject* object, S32 te)
        {
            return (object->mDrawable) ? object->mDrawable->getFace(te): NULL;
        }
    } get_te_face_func;

    identical_face = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue(&get_te_face_func, face_to_return, false, (LLFace*)nullptr);
}

void FSPanelFace::LLSelectedTE::getImageFormat(LLGLenum& image_format_to_return, bool& identical_face, bool& missing_asset)
{
    struct LLSelectedTEGetmatId : public LLSelectedTEFunctor
    {
        LLSelectedTEGetmatId()
            : mImageFormat(GL_RGB)
            , mIdentical(true)
            , mMissingAsset(false)
            , mFirstRun(true)
        {
        }
        bool apply(LLViewerObject* object, S32 te_index) override
        {
            LLViewerTexture* image = object ? object->getTEImage(te_index) : nullptr;
            LLGLenum format = GL_RGB;
            bool missing = false;
            if (image)
            {
                format = image->getPrimaryFormat();
                missing = image->isMissingAsset();
            }

            if (mFirstRun)
            {
                mFirstRun = false;
                mImageFormat = format;
                mMissingAsset = missing;
            }
            else
            {
                mIdentical &= (mImageFormat == format);
                mIdentical &= (mMissingAsset == missing);
            }
            return true;
        }
        LLGLenum mImageFormat;
        bool mIdentical;
        bool mMissingAsset;
        bool mFirstRun;
    } func;
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&func);

    image_format_to_return = func.mImageFormat;
    identical_face = func.mIdentical;
    missing_asset = func.mMissingAsset;
}

void FSPanelFace::LLSelectedTE::getTexId(LLUUID& id, bool& identical)
{
    struct LLSelectedTEGetTexId : public LLSelectedTEGetFunctor<LLUUID>
    {
        LLUUID get(LLViewerObject* object, S32 te_index)
        {
            LLTextureEntry *te = object->getTE(te_index);
            if (te)
            {
                if ((te->getID() == IMG_USE_BAKED_EYES) || (te->getID() == IMG_USE_BAKED_HAIR) || (te->getID() == IMG_USE_BAKED_HEAD) || (te->getID() == IMG_USE_BAKED_LOWER) || (te->getID() == IMG_USE_BAKED_SKIRT) || (te->getID() == IMG_USE_BAKED_UPPER)
                    || (te->getID() == IMG_USE_BAKED_LEFTARM) || (te->getID() == IMG_USE_BAKED_LEFTLEG) || (te->getID() == IMG_USE_BAKED_AUX1) || (te->getID() == IMG_USE_BAKED_AUX2) || (te->getID() == IMG_USE_BAKED_AUX3))
                {
                    return te->getID();
                }
            }

            LLUUID id;
            LLViewerTexture* image = object->getTEImage(te_index);
            if (image)
            {
                id = image->getID();
            }

            if (!id.isNull() && LLViewerMedia::getInstance()->textureHasMedia(id))
            {
                if (te)
                {
                    LLViewerTexture* tex = te->getID().notNull() ? gTextureList.findImage(te->getID(), TEX_LIST_STANDARD) : NULL;
                    if(!tex)
                    {
                        tex = LLViewerFetchedTexture::sDefaultImagep;
                    }
                    if (tex)
                    {
                        id = tex->getID();
                    }
                }
            }
            return id;
        }
    } func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, id );
}

void FSPanelFace::LLSelectedTEMaterial::getCurrent(LLMaterialPtr& material_ptr, bool& identical_material)
{
    struct MaterialFunctor : public LLSelectedTEGetFunctor<LLMaterialPtr>
    {
        LLMaterialPtr get(LLViewerObject* object, S32 te_index)
        {
            return object->getTEref(te_index).getMaterialParams();
        }
    } func;

    identical_material = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &func, material_ptr);
}

void FSPanelFace::LLSelectedTEMaterial::getMaxSpecularRepeats(F32& repeats, bool& identical)
{
    struct LLSelectedTEGetMaxSpecRepeats : public LLSelectedTEGetFunctor<F32>
    {
        F32 get(LLViewerObject* object, S32 face)
        {
            LLMaterial* mat = object->getTEref(face).getMaterialParams().get();
            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            F32 repeats_s = 1.0f;
            F32 repeats_t = 1.0f;
            if (mat)
            {
                mat->getSpecularRepeat(repeats_s, repeats_t);
                repeats_s /= object->getScale().mV[s_axis];
                repeats_t /= object->getScale().mV[t_axis];
            }
            return llmax(repeats_s, repeats_t);
        }

    } max_spec_repeats_func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_spec_repeats_func, repeats);
}

void FSPanelFace::LLSelectedTEMaterial::getMaxNormalRepeats(F32& repeats, bool& identical)
{
    struct LLSelectedTEGetMaxNormRepeats : public LLSelectedTEGetFunctor<F32>
    {
        F32 get(LLViewerObject* object, S32 face)
        {
            LLMaterial* mat = object->getTEref(face).getMaterialParams().get();
            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            F32 repeats_s = 1.0f;
            F32 repeats_t = 1.0f;
            if (mat)
            {
                mat->getNormalRepeat(repeats_s, repeats_t);
                repeats_s /= object->getScale().mV[s_axis];
                repeats_t /= object->getScale().mV[t_axis];
            }
            return llmax(repeats_s, repeats_t);
        }

    } max_norm_repeats_func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_norm_repeats_func, repeats);
}

void FSPanelFace::LLSelectedTEMaterial::getCurrentDiffuseAlphaMode(U8& diffuse_alpha_mode, bool& identical, const bool diffuse_texture_has_alpha)
{
    struct LLSelectedTEGetDiffuseAlphaMode : public LLSelectedTEGetFunctor<U8>
    {
        LLSelectedTEGetDiffuseAlphaMode() : _isAlpha(false) {}
        LLSelectedTEGetDiffuseAlphaMode(bool diffuse_texture_has_alpha) : _isAlpha(diffuse_texture_has_alpha) {}
        virtual ~LLSelectedTEGetDiffuseAlphaMode() {}

        U8 get(LLViewerObject* object, S32 face)
        {
            U8 diffuse_mode = _isAlpha ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

            LLTextureEntry* tep = object->getTE(face);
            if (tep)
            {
                LLMaterial* mat = tep->getMaterialParams().get();
                if (mat)
                {
                    diffuse_mode = mat->getDiffuseAlphaMode();
                }
            }

            return diffuse_mode;
        }

        bool _isAlpha; // whether or not the diffuse texture selected contains alpha information
    } get_diff_mode(diffuse_texture_has_alpha);

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &get_diff_mode, diffuse_alpha_mode);
}

void FSPanelFace::LLSelectedTEMaterial::selectionNormalScaleAutofit(FSPanelFace* panel_face, F32 repeats_per_meter)
{
    struct f : public LLSelectedTEFunctor
    {
        FSPanelFace* mFacePanel;
        F32 mRepeatsPerMeter;
        f(FSPanelFace* face_panel, const F32& repeats_per_meter) : mFacePanel(face_panel), mRepeatsPerMeter(repeats_per_meter) {}
        bool apply(LLViewerObject* object, S32 te)
        {
            if (object->permModify())
            {
                // Compute S,T to axis mapping
                U32 s_axis, t_axis;
                if (!LLPrimitive::getTESTAxes(te, &s_axis, &t_axis))
                    return true;

                F32 new_s = object->getScale().mV[s_axis] * mRepeatsPerMeter;
                F32 new_t = object->getScale().mV[t_axis] * mRepeatsPerMeter;

                setNormalRepeatX(mFacePanel, new_s, te);
                setNormalRepeatY(mFacePanel, new_t, te);
            }
            return true;
        }
    } setfunc(panel_face, repeats_per_meter);
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
}

void FSPanelFace::LLSelectedTEMaterial::selectionSpecularScaleAutofit(FSPanelFace* panel_face, F32 repeats_per_meter)
{
    struct f : public LLSelectedTEFunctor
    {
        FSPanelFace* mFacePanel;
        F32 mRepeatsPerMeter;
        f(FSPanelFace* face_panel, const F32& repeats_per_meter) : mFacePanel(face_panel), mRepeatsPerMeter(repeats_per_meter) {}
        bool apply(LLViewerObject* object, S32 te)
        {
            if (object->permModify())
            {
                // Compute S,T to axis mapping
                U32 s_axis, t_axis;
                if (!LLPrimitive::getTESTAxes(te, &s_axis, &t_axis))
                    return true;

                F32 new_s = object->getScale().mV[s_axis] * mRepeatsPerMeter;
                F32 new_t = object->getScale().mV[t_axis] * mRepeatsPerMeter;

                setSpecularRepeatX(mFacePanel, new_s, te);
                setSpecularRepeatY(mFacePanel, new_t, te);
            }
            return true;
        }
    } setfunc(panel_face, repeats_per_meter);
    LLSelectMgr::getInstance()->getSelection()->applyToTEs(&setfunc);
}

void FSPanelFace::LLSelectedTE::getObjectScaleS(F32& scale_s, bool& identical)
{
    struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
    {
        F32 get(LLViewerObject* object, S32 face)
        {
            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            return object->getScale().mV[s_axis];
        }

    } scale_s_func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_s_func, scale_s );
}

void FSPanelFace::LLSelectedTE::getObjectScaleT(F32& scale_t, bool& identical)
{
    struct LLSelectedTEGetObjectScaleS : public LLSelectedTEGetFunctor<F32>
    {
        F32 get(LLViewerObject* object, S32 face)
        {
            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            return object->getScale().mV[t_axis];
        }

    } scale_t_func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &scale_t_func, scale_t );
}

void FSPanelFace::LLSelectedTE::getMaxDiffuseRepeats(F32& repeats, bool& identical)
{
    struct LLSelectedTEGetMaxDiffuseRepeats : public LLSelectedTEGetFunctor<F32>
    {
        F32 get(LLViewerObject* object, S32 face)
        {
            U32 s_axis = VX;
            U32 t_axis = VY;
            LLPrimitive::getTESTAxes(face, &s_axis, &t_axis);
            F32 repeats_s = object->getTEref(face).mScaleS / object->getScale().mV[s_axis];
            F32 repeats_t = object->getTEref(face).mScaleT / object->getScale().mV[t_axis];
            return llmax(repeats_s, repeats_t);
        }

    } max_diff_repeats_func;

    identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &max_diff_repeats_func, repeats );
}

void FSPanelFace::onClickMapsSync()
{
    getState();
    if (gSavedSettings.getBOOL("SyncMaterialSettings"))
    {
        alignMaterialsProperties();
    }
}

// used to be called from two places but onCommitFlip() now does it a different way
// so we end up with this still being a function but strictly it could be moved
// into onClickMapsSync() -Zi
void FSPanelFace::alignMaterialsProperties()
{
    // <FS:TS> FIRE-11911: Synchronize materials doesn't work with planar textures
    //  Don't even try to do the alignment if we wind up here and planar is enabled.
    if (mCheckPlanarAlign->getValue().asBoolean())
    {
        return;
    }
    // </FS:TS> FIRE-11911

    F32 tex_scale_u =  getCurrentTextureScaleU();
    F32 tex_scale_v =  getCurrentTextureScaleV();
    F32 tex_offset_u = getCurrentTextureOffsetU();
    F32 tex_offset_v = getCurrentTextureOffsetV();
    F32 tex_rot =      getCurrentTextureRot();

    //<FS:TS> FIRE-12275: Material offset not working correctly
    //  Since the server cannot store negative offsets for materials
    //  textures, we normalize them to equivalent positive values here.
    tex_offset_u = (tex_offset_u < 0.0f) ? 1.0f + tex_offset_u : tex_offset_u;
    tex_offset_v = (tex_offset_v < 0.0f) ? 1.0f + tex_offset_v : tex_offset_v;
    //</FS:TS> FIRE-12275

    //<FS:TS> FIRE-12831: Negative rotations revert to zero
    //  The same goes for rotations as for offsets.
    tex_rot = (tex_rot < 0.0f) ? 360.0f + tex_rot : tex_rot;
    //</FS:TS> FIRE-12831

    mCtrlShinyScaleU->setValue(tex_scale_u);
    mCtrlShinyScaleV->setValue(tex_scale_v);
    mCtrlShinyOffsetU->setValue(tex_offset_u);
    mCtrlShinyOffsetV->setValue(tex_offset_v);
    mCtrlShinyRot->setValue(tex_rot);

    // What is this abomination!? -Zi
    LLSelectedTEMaterial::setSpecularRepeatX(this, tex_scale_u);
    LLSelectedTEMaterial::setSpecularRepeatY(this, tex_scale_v);
    LLSelectedTEMaterial::setSpecularOffsetX(this, tex_offset_u);
    LLSelectedTEMaterial::setSpecularOffsetY(this, tex_offset_v);
    LLSelectedTEMaterial::setSpecularRotation(this, tex_rot * DEG_TO_RAD);

    mCtrlBumpyScaleU->setValue(tex_scale_u);
    mCtrlBumpyScaleV->setValue(tex_scale_v);
    mCtrlBumpyOffsetU->setValue(tex_offset_u);
    mCtrlBumpyOffsetV->setValue(tex_offset_v);
    mCtrlBumpyRot->setValue(tex_rot);

    // What is this abomination!? -Zi
    LLSelectedTEMaterial::setNormalRepeatX(this, tex_scale_u);
    LLSelectedTEMaterial::setNormalRepeatY(this, tex_scale_v);
    LLSelectedTEMaterial::setNormalOffsetX(this, tex_offset_u);
    LLSelectedTEMaterial::setNormalOffsetY(this, tex_offset_v);
    LLSelectedTEMaterial::setNormalRotation(this, tex_rot * DEG_TO_RAD);
}

void FSPanelFace::onCommitFlip(const LLSD& user_data)
{
    LL_WARNS() << "onCommitFlip" << LL_ENDL;
    std::string control_name = user_data.asString();
    if (control_name.empty())
    {
        LL_WARNS() << "empty user data!" << LL_ENDL;
        return;
    }

    LL_WARNS() << "control name: " << control_name << LL_ENDL;

    LLSpinCtrl* spinner = findChild<LLSpinCtrl>(control_name);
    if (spinner)
    {
        // TODO: compensate for normal/specular map doubling of values in planar mapping mode -Zi
        F32 value = -(F32)(spinner->getValue().asReal());
        spinner->setValue(value);
        spinner->forceEditorCommit();
    }
    else
    {
        LL_WARNS() << "spinner not found: " << control_name << LL_ENDL;
    }
}

void FSPanelFace::changePrecision(S32 decimal_precision)
{
    mCtrlTexScaleU->setPrecision(decimal_precision);
    mCtrlTexScaleV->setPrecision(decimal_precision);
    mCtrlBumpyScaleU->setPrecision(decimal_precision);
    mCtrlBumpyScaleV->setPrecision(decimal_precision);
    mCtrlShinyScaleU->setPrecision(decimal_precision);
    mCtrlShinyScaleV->setPrecision(decimal_precision);
    mCtrlTexOffsetU->setPrecision(decimal_precision);
    mCtrlTexOffsetV->setPrecision(decimal_precision);
    mCtrlBumpyOffsetU->setPrecision(decimal_precision);
    mCtrlBumpyOffsetV->setPrecision(decimal_precision);
    mCtrlShinyOffsetU->setPrecision(decimal_precision);
    mCtrlShinyOffsetV->setPrecision(decimal_precision);
    mCtrlTexRot->setPrecision(decimal_precision);
    mCtrlBumpyRot->setPrecision(decimal_precision);
    mCtrlShinyRot->setPrecision(decimal_precision);
    mCtrlRpt->setPrecision(decimal_precision);
    // TODO: add PBR spinners -Zi
}

void FSPanelFace::onShowFindAllButton(LLUICtrl* ctrl, const LLSD& user_data)
{
    findChildView(user_data.asStringRef())->setVisible(true);
}

void FSPanelFace::onHideFindAllButton(LLUICtrl* ctrl, const LLSD& user_data)
{
    findChildView(user_data.asStringRef())->setVisible(false);
}

// Find all faces with same texture
void FSPanelFace::onClickBtnSelectSameTexture(const LLUICtrl* ctrl, const LLSD& user_data)
{
    std::unordered_set<LLViewerObject*> objects;

    // get a list of all linksets where at least one face is selected
    for (auto iter = LLSelectMgr::getInstance()->getSelection()->valid_begin();
        iter != LLSelectMgr::getInstance()->getSelection()->valid_end(); iter++)
    {
        objects.insert((*iter)->getObject()->getRootEdit());
    }

    // clean out the selection
    LLSelectMgr::getInstance()->deselectAll();

    // select all faces of all linksets that were found before
    LLObjectSelectionHandle handle;
    for(auto objectp : objects)
    {
        handle = LLSelectMgr::getInstance()->selectObjectAndFamily(objectp, true, false);
    }

    // get the texture channel we are interested in - using the first letter is sufficient
    // and makes things faster in the if () blocks
    char channel = user_data.asStringRef()[0];

    // grab the texture ID from the texture selector
    LLTextureCtrl* texture_control = mTextureCtrl;
    if (channel == 'n')
    {
        texture_control = mBumpyTextureCtrl;
    }
    else if (channel == 's')
    {
        texture_control = mShinyTextureCtrl;
    }
    else if (channel == 'g')
    {
        texture_control = mMaterialCtrlPBR;
    }

    LLUUID id = texture_control->getImageAssetID();

    // go through all selected links in all selected linksets
    for (auto iter = handle->begin(); iter != handle->end(); iter++)
    {
        LLSelectNode* node = *iter;
        LLViewerObject* objectp = node->getObject();

        U8 te_count = objectp->getNumTEs();

        for (U8 i = 0; i < te_count; i++)
        {
            LLUUID image_id;
            if (channel == 'd')
            {
                image_id = objectp->getTEImage(i)->getID();
            }
            else if (channel == 'g')
            {
                image_id = objectp->getRenderMaterialID(i);
            }
            else
            {
                const LLMaterialPtr mat = objectp->getTEref(i).getMaterialParams();
                if (mat.notNull())
                {
                    if (channel == 'n')
                    {
                        image_id = mat->getNormalID();
                    }
                    else if (channel == 's')
                    {
                        image_id = mat->getSpecularID();
                    }
                }
            }

            // deselect all faces that use a different texture UUID
            if (image_id != id)
            {
                objectp->setTESelected(i, false);
                node->selectTE(i, false);
            }
        }
    }
}

//
// convenience functions around tab containers -Zi
//

S32 FSPanelFace::getCurrentMaterialType() const
{
    const std::string& tab_name = mTabsPBRMatMedia->getCurrentPanel()->getName();

    if (tab_name == "panel_material_type_pbr")
    {
        return MATMEDIA_PBR;
    }
    if (tab_name == "panel_material_type_media")
    {
        return MATMEDIA_MEDIA;
    }
    return MATMEDIA_MATERIAL;
}

LLRender::eTexIndex FSPanelFace::getCurrentMatChannel() const
{
    const std::string& tab_name = mTabsMatChannel->getCurrentPanel()->getName();

    if (tab_name == "panel_blinn_phong_diffuse")
    {
        return LLRender::DIFFUSE_MAP;
    }

    if (tab_name == "panel_blinn_phong_normal")
    {
        if (getCurrentNormalMap().notNull())
        {
            return LLRender::NORMAL_MAP;
        }
    }
    else if (tab_name == "panel_blinn_phong_specular")
    {
        if (getCurrentSpecularMap().notNull())
        {
            return LLRender::SPECULAR_MAP;
        }
    }
    return (LLRender::eTexIndex)0;
}

LLRender::eTexIndex FSPanelFace::getCurrentPBRChannel() const
{
    const std::string& tab_name = mTabsPBRChannel->getCurrentPanel()->getName();

    if (tab_name == "panel_pbr_transforms_base_color")
    {
        return LLRender::BASECOLOR_MAP;
    }
    if (tab_name == "panel_pbr_transforms_normal")
    {
        return LLRender::GLTF_NORMAL_MAP;
    }
    if (tab_name == "panel_pbr_transforms_metallic")
    {
        return LLRender::METALLIC_ROUGHNESS_MAP;
    }
    if (tab_name == "panel_pbr_transforms_emissive")
    {
        return LLRender::EMISSIVE_MAP;
    }
    return LLRender::NUM_TEXTURE_CHANNELS;
}

LLGLTFMaterial::TextureInfo FSPanelFace::getCurrentPBRType(LLRender::eTexIndex pbr_channel) const
{
    switch (pbr_channel)
    {
        case LLRender::GLTF_NORMAL_MAP:
            return LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL;
        case LLRender::BASECOLOR_MAP:
            return LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR;
        case LLRender::METALLIC_ROUGHNESS_MAP:
            return LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS;
        case LLRender::EMISSIVE_MAP:
            return LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE;
        default:
            return LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT;
    }
}

void FSPanelFace::selectMaterialType(S32 material_type)
{
    if (material_type == MATMEDIA_PBR)
    {
        mTabsPBRMatMedia->selectTabByName("panel_material_type_pbr");
    }
    else if (material_type == MATMEDIA_MEDIA)
    {
        mTabsPBRMatMedia->selectTabByName("panel_material_type_media");
    }
    else
    {
        mTabsPBRMatMedia->selectTabByName("panel_material_type_blinn_phong");
    }
    onMatTabChange(); // set up relative to active tab
}

void FSPanelFace::selectMatChannel(LLRender::eTexIndex mat_channel)
{
    mSetChannelTab = true;
    if (mat_channel == LLRender::NORMAL_MAP)
    {
        mTabsMatChannel->selectTabByName("panel_blinn_phong_normal");
    }
    else if (mat_channel == LLRender::SPECULAR_MAP)
    {
        mTabsMatChannel->selectTabByName("panel_blinn_phong_specular");
    }
    else
    {
        mTabsMatChannel->selectTabByName("panel_blinn_phong_diffuse");
    }
}

void FSPanelFace::selectPBRChannel(LLRender::eTexIndex pbr_channel)
{
    mSetChannelTab = true;
    if (pbr_channel == LLRender::GLTF_NORMAL_MAP)
    {
        mTabsPBRChannel->selectTabByName("panel_pbr_transforms_normal");
    }
    else if (pbr_channel == LLRender::BASECOLOR_MAP)
    {
        mTabsPBRChannel->selectTabByName("panel_pbr_transforms_base_color");
    }
    else if (pbr_channel == LLRender::METALLIC_ROUGHNESS_MAP)
    {
        mTabsPBRChannel->selectTabByName("panel_pbr_transforms_metallic");
    }
    else if (pbr_channel == LLRender::EMISSIVE_MAP)
    {
        mTabsPBRChannel->selectTabByName("panel_pbr_transforms_emissive");
    }
    else
    {
        mTabsPBRChannel->selectTabByName("panel_pbr_transforms_all");
    }
}
