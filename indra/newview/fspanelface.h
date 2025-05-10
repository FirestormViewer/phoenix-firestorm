/**
 * @file fspanelface.h
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

#ifndef FS_FSPANELFACE_H
#define FS_FSPANELFACE_H

#include "v4color.h"
#include "llpanel.h"
#include "llgltfmaterial.h"
#include "llmaterial.h"
#include "llmaterialmgr.h"
#include "lltextureentry.h"
#include "llselectmgr.h"

#include <memory>

class LLButton;
class LLCheckBoxCtrl;
class LLColorSwatchCtrl;
class LLComboBox;
class LLInventoryItem;
class LLLineEditor;
class LLRadioGroup;
class LLSpinCtrl;
class LLTextBox;
class LLTextureCtrl;
class LLUICtrl;
class LLViewerObject;
class LLFloater;
class LLMaterialID;
class LLMediaCtrl;
class LLMenuButton;

class PBRPickerAgentListener;
class PBRPickerObjectListener;

class LLTabContainer;

// Represents an edit for use in replicating the op across one or more materials in the selection set.
//
// The apply function optionally performs the edit which it implements
// as a functor taking Data that calls member func MaterialFunc taking SetValueType
// on an instance of the LLMaterial class.
//
// boost who?
//
template<
    typename DataType,
    typename SetValueType,
    void (LLMaterial::*MaterialEditFunc)(SetValueType data) >
class FSMaterialEditFunctor
{
public:
    FSMaterialEditFunctor(const DataType& data) : _data(data) {}
    virtual ~FSMaterialEditFunctor() {}
    virtual void apply(LLMaterialPtr& material) { (material->*(MaterialEditFunc))(_data); }
    DataType _data;
};

template<
    typename DataType,
    DataType (LLMaterial::*MaterialGetFunc)() >
class FSMaterialGetFunctor
{
public:
    FSMaterialGetFunctor() {}
    virtual DataType get(LLMaterialPtr& material) { return (material->*(MaterialGetFunc)); }
};

template<
    typename DataType,
    DataType (LLTextureEntry::*TEGetFunc)() >
class FSTEGetFunctor
{
public:
    FSTEGetFunctor() {}
    virtual DataType get(LLTextureEntry* entry) { return (entry*(TEGetFunc)); }
};

//
// main class
//

class FSPanelFace : public LLPanel
{
public:
    FSPanelFace();
    virtual ~FSPanelFace();

    virtual bool postBuild();

    void refresh();
    void refreshMedia();
    void unloadMedia();
    void changePrecision(S32 decimal_precision);    // <FS:CR> Adjustable decimal precision

    static void onMaterialOverrideReceived(const LLUUID& object_id, S32 side);

    /*virtual*/ void onVisibilityChange(bool new_visibility);
    /*virtual*/ void draw();

    LLMaterialPtr createDefaultMaterial(LLMaterialPtr current_material);

    LLRender::eTexIndex getTextureChannelToEdit();
    LLRender::eTexIndex getTextureDropChannel();

    LLGLTFMaterial::TextureInfo getPBRDropChannel();

protected:
    void navigateToTitleMedia(const std::string url);
    bool selectedMediaEditable();
    void clearMediaSettings();
    void updateMediaSettings();
    void updateMediaTitle();

    bool isMediaTexSelected();

    void getState();

    void sendTexture();         // applies and sends texture
    void sendTextureInfo();     // applies and sends texture scale, offset, etc.
    void sendColor();           // applies and sends color
    void sendAlpha();           // applies and sends transparency
    void sendBump(U32 bumpiness);               // applies and sends bump map
    void sendTexGen();              // applies and sends bump map
    void sendShiny(U32 shininess);          // applies and sends shininess
    void sendFullbright();      // applies and sends full bright
    void sendGlow();
    void alignTextureLayer();

    void updateCopyTexButton();

    //
    // UI Callbacks
    //

    // common controls and parameters for Blinn-Phong and PBR
    void onCopyFaces();     // <FS> Extended copy & paste buttons
    void onPasteFaces();
    void onCommitHideWater();
    void onCommitGlow();
    void onCommitRepeatsPerMeter();

    // Blinn-Phong alpha parameters
    void onCommitAlpha();
    void onCommitAlphaMode();
    void onCommitMaterialMaskCutoff();
    void onCommitFullbright();

    // Blinn-Phong texture transforms and controls
    void onCommitTexGen();
    void onCommitPlanarAlign();
    void onCommitBump();
    void onCommitShiny();
    void onCommitTextureScaleX();
    void onCommitTextureScaleY();
    void onCommitTextureOffsetX();
    void onCommitTextureOffsetY();
    void onCommitTextureRot();
    void onCommitMaterialBumpyScaleX();
    void onCommitMaterialBumpyScaleY();
    void onCommitMaterialBumpyOffsetX();
    void onCommitMaterialBumpyOffsetY();
    void onCommitMaterialBumpyRot();
    void onCommitMaterialShinyScaleX();
    void onCommitMaterialShinyScaleY();
    void onCommitMaterialShinyOffsetX();
    void onCommitMaterialShinyOffsetY();
    void onCommitMaterialShinyRot();
    void onCommitMaterialGloss();
    void onCommitMaterialEnv();

    // Blinn-Phong Diffuse tint color swatch
    void onCommitColor();
    void onCancelColor();
    void onSelectColor();

    // Blinn-Phong Diffuse texture swatch
    void onSelectTexture();
    void onCommitTexture();
    void onCancelTexture();
    bool onDragTexture(const LLUICtrl* texture_ctrl, LLInventoryItem* item);  // this function is to return true if the drag should succeed.
    void onCloseTexturePicker();

    // Blinn-Phong Normal texture swatch
    void onCommitNormalTexture();
    void onCancelNormalTexture();

    // Blinn-Phong Specular texture swatch
    void onCommitSpecularTexture();
    void onCancelSpecularTexture();

    // Blinn-Phong Specular tint color swatch
    void onCommitShinyColor();
    void onCancelShinyColor();
    void onSelectShinyColor();

    // Texture alignment and maps synchronization
    void onClickAutoFix();
    void onAlignTexture();
    void onClickMapsSync();

    /*
     * Checks whether the selected texture from the LLFloaterTexturePicker can be applied to the currently selected object.
     * If agent selects texture which is not allowed to be applied for the currently selected object,
     * all controls of the floater texture picker which allow to apply the texture will be disabled.
     */
    void onTextureSelectionChanged(LLTextureCtrl* texture_ctrl);

    // Media
    void onClickBtnEditMedia();
    void onClickBtnDeleteMedia();
    void onClickBtnAddMedia();

    void alignMaterialsProperties();

    //
    // PBR
    //

    // PBR Material
    void onCommitPbr();
    void onCancelPbr();
    void onSelectPbr();
    bool onDragPbr(LLInventoryItem* item);  // this function is to return true if the drag should succeed.

    void onPbrSelectionChanged(LLInventoryItem* itemp);
    void onClickBtnSavePBR();

    void updatePBROverrideDisplay();

    // PBR texture maps
    void onCommitPbr(const LLUICtrl* pbr_ctrl);
    void onCancelPbr(const LLUICtrl* pbr_ctrl);
    void onSelectPbr(const LLUICtrl* pbr_ctrl);

    void getGLTFMaterial(LLGLTFMaterial* mat);

    //
    // other
    //

    bool deleteMediaConfirm(const LLSD& notification, const LLSD& response);
    bool multipleFacesSelectedConfirm(const LLSD& notification, const LLSD& response);

    // Make UI reflect state of currently selected material (refresh)
    // and UI mode (e.g. editing normal map v diffuse map)
    //
    // @param force_set_values forces spinners to set value even if they are focused
    void updateUI(bool force_set_values = false);

    // Convenience func to determine if all faces in selection have
    // identical planar texgen settings during edits
    //
    bool isIdenticalPlanarTexgen();

    // Callback funcs for individual controls
    //
    static void syncRepeatX(FSPanelFace* self, F32 scaleU);
    static void syncRepeatY(FSPanelFace* self, F32 scaleV);
    static void syncOffsetX(FSPanelFace* self, F32 offsetU);
    static void syncOffsetY(FSPanelFace* self, F32 offsetV);
    static void syncMaterialRot(FSPanelFace* self, F32 rot, int te = -1);

    // unify all GLTF spinners with no switching around required -Zi
    void onCommitGLTFUVSpinner(LLUICtrl* ctrl, const LLSD& user_data);

    void onClickBtnSelectSameTexture(const LLUICtrl* ctrl, const LLSD& user_data);  // Find all faces with same texture
    void onShowFindAllButton(LLUICtrl* ctrl, const LLSD& user_data);    // Find all faces with same texture
    void onHideFindAllButton(LLUICtrl* ctrl, const LLSD& user_data);    // Find all faces with same texture

public: // needs to be accessible to selection manager
    void onCopyColor(); // records all selected faces
    void onPasteColor(); // to specific face
    void onPasteColor(LLViewerObject* objectp, S32 te); // to specific face
    void onCopyTexture();
    void onPasteTexture();
    void onPasteTexture(LLViewerObject* objectp, S32 te);

protected:
    //
    // Constant definitions for tabs and combo boxes
    // Must match the commbobox definitions in panel_tools_texture.xml  // Not anymore -Zi
    //
    static constexpr S32 MATMEDIA_MATERIAL = 0;             // Material
    static constexpr S32 MATMEDIA_PBR = 1;                  // PBR
    static constexpr S32 MATMEDIA_MEDIA = 2;                // Media

    static constexpr S32 BUMPY_TEXTURE = 18;                // use supplied normal map   (index of "Use Texture" in combo box)
    static constexpr S32 SHINY_TEXTURE = 4;                 // use supplied specular map (index of "Use Texture" in combo box)

public:
    // public because ... functors? -Zi
    void onCommitFlip(const LLSD& user_data);

    // Blinn-Phong texture transforms and controls
    // public because ... functors? -Zi
    LLSpinCtrl* mCtrlTexScaleU;
    LLSpinCtrl* mCtrlTexScaleV;
    LLSpinCtrl* mCtrlBumpyScaleU;
    LLSpinCtrl* mCtrlBumpyScaleV;
    LLSpinCtrl* mCtrlShinyScaleU;
    LLSpinCtrl* mCtrlShinyScaleV;
    LLSpinCtrl* mCtrlTexOffsetU;
    LLSpinCtrl* mCtrlTexOffsetV;
    LLSpinCtrl* mCtrlBumpyOffsetU;
    LLSpinCtrl* mCtrlBumpyOffsetV;
    LLSpinCtrl* mCtrlShinyOffsetU;
    LLSpinCtrl* mCtrlShinyOffsetV;
    LLSpinCtrl* mCtrlTexRot;
    LLSpinCtrl* mCtrlBumpyRot;
    LLSpinCtrl* mCtrlShinyRot;

    // Blinn-Phong texture transforms and controls
    // public to give functors access -Zi
    LLComboBox* mComboTexGen;
    LLCheckBoxCtrl* mCheckPlanarAlign;

    // Tab controls
    // public to give functors access -Zi
    LLTabContainer* mTabsMatChannel;
    void onMatTabChange();
    void onMatChannelTabChange();
    void onPBRChannelTabChange();

private:
    bool isAlpha() { return mIsAlpha; }

    // Update visibility of controls to match current UI mode
    // (e.g. materials vs media editing)
    //
    // Do NOT call updateUI from within this function.
    //
    void updateVisibility(LLViewerObject* objectp = nullptr);

    // Convenience funcs to keep the visual flack to a minimum
    //
    LLUUID  getCurrentNormalMap() const;
    LLUUID  getCurrentSpecularMap() const;
    U32     getCurrentShininess();
    U32     getCurrentBumpiness();
    U8      getCurrentDiffuseAlphaMode();
    U8      getCurrentAlphaMaskCutoff();
    U8      getCurrentEnvIntensity();
    U8      getCurrentGlossiness();
    F32     getCurrentBumpyRot();
    F32     getCurrentBumpyScaleU();
    F32     getCurrentBumpyScaleV();
    F32     getCurrentBumpyOffsetU();
    F32     getCurrentBumpyOffsetV();
    F32     getCurrentShinyRot();
    F32     getCurrentShinyScaleU();
    F32     getCurrentShinyScaleV();
    F32     getCurrentShinyOffsetU();
    F32     getCurrentShinyOffsetV();

    // <FS:Zi> map tab states to various values
    // TODO: should be done with tab-change signals and flags, really
    S32     getCurrentMaterialType() const;
    LLRender::eTexIndex getCurrentMatChannel() const;
    LLRender::eTexIndex getCurrentPBRChannel() const;
    LLGLTFMaterial::TextureInfo getCurrentPBRType(LLRender::eTexIndex pbr_channel) const;

    void    selectMaterialType(S32 material_type);
    void    selectMatChannel(LLRender::eTexIndex mat_channel);
    void    selectPBRChannel(LLRender::eTexIndex pbr_channel);

    F32     getCurrentTextureRot();
    F32     getCurrentTextureScaleU();
    F32     getCurrentTextureScaleV();
    F32     getCurrentTextureOffsetU();
    F32     getCurrentTextureOffsetV();

    //
    // Build tool controls
    //

    // private Tab controls
    LLTabContainer*     mTabsPBRMatMedia;
    LLTabContainer*     mTabsPBRChannel;
    bool                mSetChannelTab = false;

    // common controls and parameters for Blinn-Phong and PBR
    LLButton*           mBtnCopyFaces;
    LLButton*           mBtnPasteFaces;
    LLSpinCtrl*         mCtrlGlow;
    LLSpinCtrl*         mCtrlRpt;

    // Blinn-Phong alpha parameters
    LLSpinCtrl*         mCtrlColorTransp;       // transparency = 1 - alpha
    LLView*             mColorTransPercent;
    LLTextBox*          mLabelAlphaMode;
    LLComboBox*         mComboAlphaMode;
    LLSpinCtrl*         mCtrlMaskCutoff;
    LLCheckBoxCtrl*     mCheckFullbright;
    LLCheckBoxCtrl*     mCheckHideWater;

    // private Blinn-Phong texture transforms and controls
    LLView*             mLabelBumpiness;
    LLComboBox*         mComboBumpiness;
    LLView*             mLabelShininess;
    LLComboBox*         mComboShininess;
    // others are above in the public section
    LLSpinCtrl*         mCtrlGlossiness;
    LLSpinCtrl*         mCtrlEnvironment;

    // Blinn-Phong Diffuse tint color swatch
    LLColorSwatchCtrl*  mColorSwatch;

    // Blinn-Phong Diffuse texture swatch
    LLTextureCtrl*      mTextureCtrl;

    // Blinn-Phong Normal texture swatch
    LLTextureCtrl*      mBumpyTextureCtrl;

    // Blinn-Phong Specular texture swatch
    LLTextureCtrl*      mShinyTextureCtrl;

    // Blinn-Phong Specular tint color swatch
    LLColorSwatchCtrl*  mShinyColorSwatch;

    // Texture alignment and maps synchronization
    LLButton*           mBtnAlignMedia;
    LLButton*           mBtnAlignTextures;
    LLCheckBoxCtrl*     mCheckSyncMaterials;

    // Media
    LLButton*           mBtnDeleteMedia;
    LLButton*           mBtnAddMedia;
    LLMediaCtrl*        mTitleMedia;
    LLTextBox*          mTitleMediaText;

    // PBR
    LLTextureCtrl*      mMaterialCtrlPBR;
    LLColorSwatchCtrl*  mBaseTintPBR;
    LLTextureCtrl*      mBaseTexturePBR;
    LLTextureCtrl*      mNormalTexturePBR;
    LLTextureCtrl*      mORMTexturePBR;
    LLTextureCtrl*      mEmissiveTexturePBR;
    LLColorSwatchCtrl*  mEmissiveTintPBR;
    LLCheckBoxCtrl*     mCheckDoubleSidedPBR;
    LLSpinCtrl*         mAlphaPBR;
    LLTextBox*          mLabelAlphaModePBR;
    LLComboBox*         mAlphaModePBR;
    LLSpinCtrl*         mMaskCutoffPBR;
    LLSpinCtrl*         mMetallicFactorPBR;
    LLSpinCtrl*         mRoughnessFactorPBR;
    LLButton*           mBtnSavePBR;

    struct
    {
        std::array<LLUUID, LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT> mMap;
        LLColor4 mBaseColorTint;
        F32 mMetallic;
        F32 mRoughness;
        LLColor4 mEmissiveTint;
        LLGLTFMaterial::AlphaMode mAlphaMode;
        F32 mAlphaCutoff;
        bool mDoubleSided;
    } mPBRBaseMaterialParams;

    // Dirty flags - taken from llmaterialeditor.cpp ... LL please put this in a .h! -Zi
    U32 mUnsavedChanges; // flags to indicate individual changed parameters
    U32 mRevertedChanges; // flags to indicate individual reverted parameters

    // Hey look everyone, a type-safe alternative to copy and paste! :)
    //

    // Update material parameters by applying 'edit_func' to selected TEs
    //
    template<
        typename DataType,
        typename SetValueType,
        void (LLMaterial::*MaterialEditFunc)(SetValueType data) >
    static void edit(FSPanelFace* p, DataType data, int te = -1, const LLUUID &only_for_object_id = LLUUID())
    {
        FSMaterialEditFunctor< DataType, SetValueType, MaterialEditFunc > edit(data);
        struct LLSelectedTEEditMaterial : public LLSelectedTEMaterialFunctor
        {
            LLSelectedTEEditMaterial(FSPanelFace* panel, FSMaterialEditFunctor< DataType, SetValueType, MaterialEditFunc >* editp, const LLUUID &only_for_object_id) : _panel(panel), _edit(editp), _only_for_object_id(only_for_object_id) {}
            virtual ~LLSelectedTEEditMaterial() {};
            virtual LLMaterialPtr apply(LLViewerObject* object, S32 face, LLTextureEntry* tep, LLMaterialPtr& current_material)
            {
                if (_edit && (_only_for_object_id.isNull() || _only_for_object_id == object->getID()))
                {
                    LLMaterialPtr new_material = _panel->createDefaultMaterial(current_material);
                    llassert_always(new_material);

                    // Determine correct alpha mode for current diffuse texture
                    // (i.e. does it have an alpha channel that makes alpha mode useful)
                    //
                    // _panel->isAlpha() "lies" when one face has alpha and the rest do not (NORSPEC-329)
                    // need to get per-face answer to this question for sane alpha mode retention on updates.
                    //
                    bool is_alpha_face = object->isImageAlphaBlended(face);

                    // need to keep this original answer for valid comparisons in logic below
                    //
                    U8 original_default_alpha_mode = is_alpha_face ? LLMaterial::DIFFUSE_ALPHA_MODE_BLEND : LLMaterial::DIFFUSE_ALPHA_MODE_NONE;

                    U8 default_alpha_mode = original_default_alpha_mode;

                    if (!current_material.isNull())
                    {
                        default_alpha_mode = current_material->getDiffuseAlphaMode();
                    }

                    // Insure we don't inherit the default of blend by accident...
                    // this will be stomped by a legit request to change the alpha mode by the apply() below
                    //
                    new_material->setDiffuseAlphaMode(default_alpha_mode);

                    // Do "It"!
                    //
                    _edit->apply(new_material);

                    U32     new_alpha_mode          = new_material->getDiffuseAlphaMode();
                    LLUUID  new_normal_map_id       = new_material->getNormalID();
                    LLUUID  new_spec_map_id         = new_material->getSpecularID();

                    if ((new_alpha_mode == LLMaterial::DIFFUSE_ALPHA_MODE_BLEND) && !is_alpha_face)
                    {
                        new_alpha_mode = LLMaterial::DIFFUSE_ALPHA_MODE_NONE;
                        new_material->setDiffuseAlphaMode(LLMaterial::DIFFUSE_ALPHA_MODE_NONE);
                    }

                    bool is_default_blend_mode      = (new_alpha_mode == original_default_alpha_mode);
                    bool is_need_material           = !is_default_blend_mode || !new_normal_map_id.isNull() || !new_spec_map_id.isNull();

                    if (!is_need_material)
                    {
                        LL_DEBUGS("Materials") << "Removing material from object " << object->getID() << " face " << face << LL_ENDL;
                        LLMaterialMgr::getInstance()->remove(object->getID(),face);
                        new_material = NULL;
                    }
                    else
                    {
                        LL_DEBUGS("Materials") << "Putting material on object " << object->getID() << " face " << face << ", material: " << new_material->asLLSD() << LL_ENDL;
                        LLMaterialMgr::getInstance()->put(object->getID(),face,*new_material);
                    }

                    object->setTEMaterialParams(face, new_material);
                    return new_material;
                }
                return NULL;
            }
            FSMaterialEditFunctor< DataType, SetValueType, MaterialEditFunc >*  _edit;
            FSPanelFace *_panel;
            const LLUUID & _only_for_object_id;
        } editor(p, &edit, only_for_object_id);
        LLSelectMgr::getInstance()->selectionSetMaterialParams(&editor, te);
    }

    template<
        typename DataType,
        typename ReturnType,
        ReturnType (LLMaterial::* const MaterialGetFunc)() const >
    static void getTEMaterialValue(DataType& data_to_return, bool& identical,DataType default_value, bool has_tolerance = false, DataType tolerance = DataType())
    {
        DataType data_value = default_value;
        struct GetTEMaterialVal : public LLSelectedTEGetFunctor<DataType>
        {
            GetTEMaterialVal(DataType default_value) : _default(default_value) {}
            virtual ~GetTEMaterialVal() {}

            DataType get(LLViewerObject* object, S32 face)
            {
                DataType ret = _default;
                LLMaterialPtr material_ptr;
                LLTextureEntry* tep = object ? object->getTE(face) : NULL;
                if (tep)
                {
                    material_ptr = tep->getMaterialParams();
                    if (!material_ptr.isNull())
                    {
                        ret = (material_ptr->*(MaterialGetFunc))();
                    }
                }
                return ret;
            }
            DataType _default;
        } GetFunc(default_value);
        identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &GetFunc, data_value, has_tolerance, tolerance);
        data_to_return = data_value;
    }

    template<
        typename DataType,
        typename ReturnType, // some kids just have to different...
        ReturnType (LLTextureEntry::* const TEGetFunc)() const >
    static void getTEValue(DataType& data_to_return, bool& identical, DataType default_value, bool has_tolerance = false, DataType tolerance = DataType())
    {
        DataType data_value = default_value;
        struct GetTEVal : public LLSelectedTEGetFunctor<DataType>
        {
            GetTEVal(DataType default_value) : _default(default_value) {}
            virtual ~GetTEVal() {}

            DataType get(LLViewerObject* object, S32 face) {
                LLTextureEntry* tep = object ? object->getTE(face) : NULL;
                return tep ? ((tep->*(TEGetFunc))()) : _default;
            }
            DataType _default;
        } GetTEValFunc(default_value);
        identical = LLSelectMgr::getInstance()->getSelection()->getSelectedTEValue( &GetTEValFunc, data_value, has_tolerance, tolerance );
        data_to_return = data_value;
    }

    // Update vis and enabling of specific subsets of controls based on material params
    // (e.g. hide the spec controls if no spec texture is applied)
    //
    void updateShinyControls(bool is_setting_texture = false, bool mess_with_combobox = false);
    void updateBumpyControls(bool is_setting_texture = false, bool mess_with_combobox = false);
    void updateAlphaControls();
    void updateUIGLTF(LLViewerObject* objectp, bool& has_pbr_material, bool& has_faces_without_pbr, bool force_set_values);

    void updateSelectedGLTFMaterials(std::function<void(LLGLTFMaterial*)> func);
    void updateSelectedGLTFMaterialsWithScale(std::function<void(LLGLTFMaterial*, const F32, const F32)> func);
    void updateGLTFTextureTransform(const LLGLTFMaterial::TextureInfo texture_info, std::function<void(LLGLTFMaterial::TextureTransform*)> edit);
    void updateGLTFTextureTransformWithScale(const LLGLTFMaterial::TextureInfo texture_info, std::function<void(LLGLTFMaterial::TextureTransform*, const F32, const F32)> edit);

    void setMaterialOverridesFromSelection();

    bool mIsAlpha;
    bool mExcludeWater;

    LLSD mClipboardParams;

    LLSD mMediaSettings;
    bool mNeedMediaTitle;

    class Selection
    {
    public:
        void connect();

        // Returns true if the selected objects or sides have changed since
        // this was last called, and no object update is pending
        bool update();

        // Prevents update() returning true until the provided object is
        // updated. Necessary to prevent controls updating when the mouse is
        // held down.
        void setDirty() { mChanged = true; };

        // Callbacks
        void onSelectionChanged() { mNeedsSelectionCheck = true; }
        void onSelectedObjectUpdated(const LLUUID &object_id, S32 side);

    protected:
        bool compareSelection();

        bool mChanged = false;

        boost::signals2::scoped_connection mSelectConnection;
        bool mNeedsSelectionCheck = true;
        S32 mSelectedObjectCount = 0;
        S32 mSelectedTECount = 0;
        LLUUID mSelectedObjectID;
        S32 mLastSelectedSide = -1;
    };

    static Selection sMaterialOverrideSelection;

    std::unique_ptr<PBRPickerAgentListener> mAgentInventoryListener;
    std::unique_ptr<PBRPickerObjectListener> mVOInventoryListener;

public:
    #if defined(FS_DEF_GET_MAT_STATE)
        #undef FS_DEF_GET_MAT_STATE
    #endif

    #if defined(FS_DEF_GET_TE_STATE)
        #undef FS_DEF_GET_TE_STATE
    #endif

    #if defined(FS_DEF_EDIT_MAT_STATE)
        FS_DEF_EDIT_MAT_STATE
    #endif

    // Accessors for selected TE material state
    //
    #define FS_DEF_GET_MAT_STATE(DataType,ReturnType,MaterialMemberFunc,DefaultValue,HasTolerance,Tolerance) \
        static void MaterialMemberFunc(DataType& data, bool& identical, bool has_tolerance = HasTolerance, DataType tolerance = Tolerance) \
        { \
            getTEMaterialValue< DataType, ReturnType, &LLMaterial::MaterialMemberFunc >(data, identical, DefaultValue, has_tolerance, tolerance); \
        }

    // Mutators for selected TE material
    //
    #define FS_DEF_EDIT_MAT_STATE(DataType,ReturnType,MaterialMemberFunc) \
        static void MaterialMemberFunc(FSPanelFace* p, DataType data, int te = -1, const LLUUID &only_for_object_id = LLUUID()) \
        { \
            edit< DataType, ReturnType, &LLMaterial::MaterialMemberFunc >(p, data, te, only_for_object_id); \
        }

    // Accessors for selected TE state proper (legacy settings etc)
    //
    #define FS_DEF_GET_TE_STATE(DataType,ReturnType,TexEntryMemberFunc,DefaultValue,HasTolerance,Tolerance) \
        static void TexEntryMemberFunc(DataType& data, bool& identical, bool has_tolerance = HasTolerance, DataType tolerance = Tolerance) \
        { \
            getTEValue< DataType, ReturnType, &LLTextureEntry::TexEntryMemberFunc >(data, identical, DefaultValue, has_tolerance, tolerance); \
        }

    class LLSelectedTEMaterial
    {
    public:
        static void getCurrent(LLMaterialPtr& material_ptr, bool& identical_material);
        static void getMaxSpecularRepeats(F32& repeats, bool& identical);
        static void getMaxNormalRepeats(F32& repeats, bool& identical);
        static void getCurrentDiffuseAlphaMode(U8& diffuse_alpha_mode, bool& identical, bool diffuse_texture_has_alpha);
        static void selectionNormalScaleAutofit(FSPanelFace* panel_face, F32 repeats_per_meter);
        static void selectionSpecularScaleAutofit(FSPanelFace* panel_face, F32 repeats_per_meter);

        FS_DEF_GET_MAT_STATE(LLUUID,const LLUUID&,getNormalID,LLUUID::null, false, LLUUID::null);
        FS_DEF_GET_MAT_STATE(LLUUID,const LLUUID&,getSpecularID,LLUUID::null, false, LLUUID::null);
        FS_DEF_GET_MAT_STATE(F32,F32,getSpecularRepeatX,1.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getSpecularRepeatY,1.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getSpecularOffsetX,0.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getSpecularOffsetY,0.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getSpecularRotation,0.0f, true, 0.001f);

        FS_DEF_GET_MAT_STATE(F32,F32,getNormalRepeatX,1.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getNormalRepeatY,1.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getNormalOffsetX,0.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getNormalOffsetY,0.0f, true, 0.001f);
        FS_DEF_GET_MAT_STATE(F32,F32,getNormalRotation,0.0f, true, 0.001f);

        FS_DEF_EDIT_MAT_STATE(U8,U8,setDiffuseAlphaMode);
        FS_DEF_EDIT_MAT_STATE(U8,U8,setAlphaMaskCutoff);

        FS_DEF_EDIT_MAT_STATE(F32,F32,setNormalOffsetX);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setNormalOffsetY);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setNormalRepeatX);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setNormalRepeatY);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setNormalRotation);

        FS_DEF_EDIT_MAT_STATE(F32,F32,setSpecularOffsetX);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setSpecularOffsetY);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setSpecularRepeatX);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setSpecularRepeatY);
        FS_DEF_EDIT_MAT_STATE(F32,F32,setSpecularRotation);

        FS_DEF_EDIT_MAT_STATE(U8,U8,setEnvironmentIntensity);
        FS_DEF_EDIT_MAT_STATE(U8,U8,setSpecularLightExponent);

        FS_DEF_EDIT_MAT_STATE(LLUUID,const LLUUID&,setNormalID);
        FS_DEF_EDIT_MAT_STATE(LLUUID,const LLUUID&,setSpecularID);
        FS_DEF_EDIT_MAT_STATE(LLColor4U,    const LLColor4U&,setSpecularLightColor);
    };

    class LLSelectedTE
    {
    public:
        static void getFace(class LLFace*& face_to_return, bool& identical_face);
        static void getImageFormat(LLGLenum& image_format_to_return, bool& identical_face, bool& missing_asset);
        static void getTexId(LLUUID& id, bool& identical);
        static void getObjectScaleS(F32& scale_s, bool& identical);
        static void getObjectScaleT(F32& scale_t, bool& identical);
        static void getMaxDiffuseRepeats(F32& repeats, bool& identical);

        FS_DEF_GET_TE_STATE(U8,U8,getBumpmap,0, false, 0)
        FS_DEF_GET_TE_STATE(U8,U8,getShiny,0, false, 0)
        FS_DEF_GET_TE_STATE(U8,U8,getFullbright,0, false, 0)
        FS_DEF_GET_TE_STATE(F32,F32,getRotation,0.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(F32,F32,getOffsetS,0.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(F32,F32,getOffsetT,0.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(F32,F32,getScaleS,1.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(F32,F32,getScaleT,1.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(F32,F32,getGlow,0.0f, true, 0.001f)
        FS_DEF_GET_TE_STATE(LLTextureEntry::e_texgen,LLTextureEntry::e_texgen,getTexGen,LLTextureEntry::TEX_GEN_DEFAULT, false, LLTextureEntry::TEX_GEN_DEFAULT)
        FS_DEF_GET_TE_STATE(LLColor4,const LLColor4&,getColor,LLColor4::white, false, LLColor4::black);
    };
};

#endif
