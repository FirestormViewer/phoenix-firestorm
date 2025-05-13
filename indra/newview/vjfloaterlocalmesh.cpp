/**
 * @file vjfloaterlocalmesh.cpp
 * @author Vaalith Jinn
 * @brief Local Mesh Floater source
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Local Mesh contribution source code
 * Copyright (C) 2022, Vaalith Jinn.
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
 * $/LicenseInfo$
 */

// linden headers
#include "llviewerprecompiledheaders.h"

#include "llfilepicker.h"
#include "llinventoryicon.h"
#include "llviewercontrol.h"
#include "llviewermenufile.h"
#include "fsscrolllistctrl.h"
#include "llcheckboxctrl.h"
#include "llviewertexteditor.h"
#include "llcombobox.h"
#include "lltoolmgr.h"
#include "lltoolcomp.h"
#include "llmodelpreview.h"

#include "llviewerobjectlist.h"

// own headers
#include "vjfloaterlocalmesh.h"
#include "vjlocalmesh.h"

static constexpr S32 LOCAL_TRACKING_ID_COLUMN = 4;


/*================================*/
/*  LLFloaterLocalMeshFilePicker  */
/*================================*/
class LLFloaterLocalMeshFilePicker : public LLFilePickerThread
{
public:
    explicit LLFloaterLocalMeshFilePicker(LLFloaterLocalMesh* parent_floater);
    void notify(const std::vector<std::string>& filenames) final;

private:
    LLFloaterLocalMesh* mParentFloater;
};

LLFloaterLocalMeshFilePicker::LLFloaterLocalMeshFilePicker(LLFloaterLocalMesh* parent_floater)
    : LLFilePickerThread(LLFilePicker::FFLOAD_COLLADA)
{
    mParentFloater = parent_floater;
}

void LLFloaterLocalMeshFilePicker::notify(const std::vector<std::string>& filenames)
{
    if ((!mParentFloater) || filenames.empty())
    {
        return;
    }

    mParentFloater->onBtnAddCallback(filenames[0]);
}


/*======================*/
/*  LLFloaterLocalMesh  */
/*======================*/
LLFloaterLocalMesh::LLFloaterLocalMesh(const LLSD & key) :
    LLFloater(key)
{
}

LLFloaterLocalMesh::~LLFloaterLocalMesh()
{
    mObjectCreatedCallback.disconnect();
}

//static

void LLFloaterLocalMesh::onOpen(const LLSD& key)
{
    reloadFileList(false);

    // register with local mesh system
    LLLocalMeshSystem::getInstance()->registerFloaterPointer(this);

    // toggle select tool
    toggleSelectTool(true);
}

void LLFloaterLocalMesh::onClose(bool app_quitting)
{
    // deregister from local mesh system
    LLLocalMeshSystem::getInstance()->registerFloaterPointer(nullptr);

    // toggle select tool
    toggleSelectTool(false);
}

void LLFloaterLocalMesh::onSelectionChangedCallback()
{
    reloadLowerUI();
    showLog();
}

bool LLFloaterLocalMesh::postBuild()
{
    getChild<LLButton>("btn_add")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnAdd(); });
    getChild<LLButton>("btn_reload")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnReload(); });
    getChild<LLButton>("btn_remove")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnRemove(); });
    getChild<LLButton>("btn_apply")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnApply(); });
    getChild<LLButton>("btn_clear")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnClear(); });
    getChild<LLButton>("btn_rez")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onBtnRez(); });

    mTabContainer = findChild<LLTabContainer>("local_mesh_tabs");
    if(mTabContainer)
    {
        mLogPanel = mTabContainer->getChild<LLViewerTextEditor>("local_mesh_log");
        mScrollCtrl = mTabContainer->getChild<FSScrollListCtrl>("l_name_list");
        mScrollCtrl->setCommitCallback(boost::bind(&LLFloaterLocalMesh::onFileListCommitCallback, this));
    }

    getChild<LLComboBox>("lod_suffix_combo")->setCommitCallback(boost::bind(&LLFloaterLocalMesh::onSuffixStandardSelected, this, _1));

    reloadLowerUI();
    return true;
}

void LLFloaterLocalMesh::update_selected_target(const LLUUID& selected_id)
{
    if ( selected_id != mLastSelectedObject )
    {
        mLastSelectedObject = selected_id;
        onSelectionChangedCallback();
    }
}

void LLFloaterLocalMesh::draw()
{
    // check if selection has changed.
    if (auto current_object = LLSelectMgr::getInstance()->getSelection()->getFirstObject())
    {
        update_selected_target( current_object->getID() );
    }
    else
    {
        update_selected_target( LLUUID::null );
    }
    // continue drawing
    LLFloater::draw();
}

void LLFloaterLocalMesh::onBtnAdd()
{
    (new LLFloaterLocalMeshFilePicker(this))->getFile();
}

void LLFloaterLocalMesh::onBtnAddCallback(const std::string& filename)
{
    constexpr bool try_lods{ true };

    LLLocalMeshSystem::getInstance()->addFile(filename, try_lods);
    showLog();
}


void LLFloaterLocalMesh::onBtnReload()
{
    auto selected_item = mScrollCtrl->getFirstSelected();
    if (!selected_item)
    {
        return;
    }

    auto selected_column = selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
    if (!selected_column)
    {
        return;
    }

    LLUUID selected_id = selected_column->getValue().asUUID();
    LLLocalMeshSystem::getInstance()->reloadFile(selected_id);
}

void LLFloaterLocalMesh::onBtnRemove()
{
    // more checks necessary, apparently.
    auto selected_item = mScrollCtrl->getFirstSelected();
    if (!selected_item)
    {
        return;
    }

    auto selected_column = selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
    if (!selected_column)
    {
        return;
    }

    LLUUID selected_id = selected_column->getValue().asUUID();
    LLLocalMeshSystem::getInstance()->deleteFile(selected_id);
    reloadLowerUI();
}

void LLFloaterLocalMesh::onBtnApply()
{
    auto scroll_ctrl_selected_item = mScrollCtrl->getFirstSelected();
    if (!scroll_ctrl_selected_item)
    {
        return;
    }

    auto scroll_ctrl_selected_column = scroll_ctrl_selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
    if (!scroll_ctrl_selected_column)
    {
        return;
    }

    // check combobox pointer
    auto objectlist_combo_box = getChild<LLComboBox>("object_apply_list");
    if (!objectlist_combo_box)
    {
        return;
    }

    // make sure the selection is still valid, and if so - get id.
    LLUUID selected_object_id = getCurrentSelectionIfValid();
    if (selected_object_id.isNull())
    {
        return;
    }

    // get selected local file id, object idx and use_scale boolean
    LLUUID file_id = scroll_ctrl_selected_column->getValue().asUUID();
    S32 object_idx = objectlist_combo_box->getFirstSelectedIndex();

    // finally tell local mesh system to apply
    LLLocalMeshSystem::getInstance()->applyVObject(selected_object_id, file_id, object_idx, false);
}

void LLFloaterLocalMesh::onSuffixStandardSelected(LLUICtrl* ctrl)
{
    S32 which{0};
// SL standard LODs are the reverse of every other game engine (LOD0 least detail)
// SL has no suffix for the HIGH LOD
    static const std::array<std::string,5> sl_suffixes = {
        "LOD0",
        "LOD1",
        "LOD2",
        "",
        "PHYS"
    };
// Game engines (UE, Unity, CryEngine, Godot, etc.) all use LOD0 as highest.
// They typically also label the high with a suffix too
    static const std::array<std::string,5> std_suffixes = {
        "LOD3",
        "LOD2",
        "LOD1",
        "LOD0",
        "PHYS"
    };
// Human friendly. When making things manually people naturally use names.
    static const std::array<std::string,5> desc_suffixes = {
        "LOWEST",
        "LOW",
        "MED",
        "HIGH",
        "PHYS"
    };

    if (auto cbx = dynamic_cast<LLComboBox*>(ctrl))
    {
        which = cbx->getFirstSelectedIndex();
    }
    else
    {
        LL_WARNS() << "no UI element found! nothing changed" << LL_ENDL;
        return;
    }
    gSavedSettings.setS32("FSMeshLodSuffixScheme",which);
    switch (which)
    {
        case 1: // SL
            for (S32 i = 0; i < LLModel::NUM_LODS; i++)
            {
                gSavedSettings.setString(LLModelPreview::sSuffixVarNames[i], sl_suffixes[i]);
            }
            break;
        case 2: // standard
            for (S32 i = 0; i < LLModel::NUM_LODS; i++)
            {
                gSavedSettings.setString(LLModelPreview::sSuffixVarNames[i], std_suffixes[i]);
            }
            break;
        case 3: // descriptive english
            for (S32 i = 0; i < LLModel::NUM_LODS; i++)
            {
                gSavedSettings.setString(LLModelPreview::sSuffixVarNames[i], desc_suffixes[i]);
            }
            break;
        default:
            LL_WARNS() << "no standard selected, nothing changed" << LL_ENDL;
            break;
    };
}

void LLFloaterLocalMesh::onBtnClear()
{
    LLUUID selected_object_id = getCurrentSelectionIfValid();
    if (selected_object_id.isNull())
    {
        return;
    }

    LLLocalMeshSystem::getInstance()->clearVObject(selected_object_id);
}

bool LLFloaterLocalMesh::processPrimCreated(LLViewerObject* object)
{
    if(!object)
    {
        return false;
    }

    // Select the new object
    LLSelectMgr::getInstance()->selectObjectAndFamily(object, true);


    // LLUUID local_id{"aee92334-90e9-110b-7c03-0ff3bc19de63"};
    LLUUID local_id{};
    auto* volp = object->getVolume();
    if(!volp)
    {
        return false;
    }
    auto volume_params{volp->getParams()};

    object->setParameterEntryInUse(LLNetworkData::PARAMS_SCULPT, true, true);
    auto *sculpt_params = (LLSculptParams *)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);

    if (sculpt_params)
    {
        sculpt_params->setSculptTexture( local_id, LL_SCULPT_TYPE_MESH);
        volume_params.setSculptID(local_id, LL_SCULPT_TYPE_MESH);
        object->updateVolume(volume_params);
    }
    else
    {
        return false;
    }

    if (auto floater_ptr = LLLocalMeshSystem::getInstance()->getFloaterPointer())
    {
        floater_ptr->update_selected_target(object->getID());
        auto scroll_ctrl_selected_item = floater_ptr->mScrollCtrl->getFirstSelected();
        if (!scroll_ctrl_selected_item)
        {
            // at this point we have a valid object even if we can't fill it.
            return true;
        }

        auto scroll_ctrl_selected_column = scroll_ctrl_selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
        if (!scroll_ctrl_selected_column)
        {
            // at this point we have a valid object even if we can't fill it.
            return true;
        }

        auto objectlist_combo_box = floater_ptr->getChild<LLComboBox>("object_apply_list");
        if (!objectlist_combo_box)
        {
            // at this point we have a valid object even if we can't fill it.
            return true;
        }

        // TODO: replace this with check box. "apply selected"
        bool apply_local { scroll_ctrl_selected_item && scroll_ctrl_selected_column && objectlist_combo_box };

        if (apply_local)
        {
            local_id = scroll_ctrl_selected_column->getValue().asUUID();
            // fill it up with local goodness
            static const bool use_scale {false};

            // // make sure the selection is still valid, and if so - get id.

            // get selected local file id, object idx and use_scale boolean
            int object_idx = objectlist_combo_box->getFirstSelectedIndex();
            LLLocalMeshSystem::getInstance()->applyVObject(object->getID(), local_id, object_idx, use_scale);
            volp = object->getVolume();
            if (!volp)
            {
                return true;
            }
            volume_params = volp->getParams();
            object->updateVolume(volume_params);
            object->markForUpdate();
        }
    }
    return true;
}

void LLFloaterLocalMesh::onBtnRez()
{
    mObjectCreatedCallback = gObjectList.setNewObjectCallback(boost::bind(&LLFloaterLocalMesh::processPrimCreated, this, _1));
    LLToolMgr::getInstance()->getCurrentToolset()->selectTool( (LLTool *) LLToolCompCreate::getInstance());

}

void LLFloaterLocalMesh::showLog()
{
    // load a log file for the newly selected item
    mLogPanel->clear();
    // make sure something is actually selected, or we crash
    auto scroll_ctrl_selected_item = mScrollCtrl->getFirstSelected();
    if (!scroll_ctrl_selected_item)
    {
        return;
    }

    auto scroll_ctrl_selected_column = scroll_ctrl_selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
    if (!scroll_ctrl_selected_column)
    {
        return;
    }

    // something is selected, yay. request log
    LLUUID file_id = scroll_ctrl_selected_column->getValue().asUUID();
    auto log = LLLocalMeshSystem::getInstance()->getFileLog(file_id);

    for (const auto& log_string : log)
    {
        mLogPanel->appendText(log_string, true);
    }
}

void LLFloaterLocalMesh::reloadFileList(bool keep_selection)
{
    const auto& fileinfo_vec = LLLocalMeshSystem::getInstance()->getFileInfoVector();
    int selected_num = mScrollCtrl->getFirstSelectedIndex();

    mScrollCtrl->clearRows();
    for (auto& cinfo : fileinfo_vec)
    {
        std::string status_text;
        switch (cinfo.mStatus)
        {
            case LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_NONE:
            {
                status_text = "None";
                break;
            }

            case LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_LOADING:
            {
                status_text = "Loading";
                break;
            }

            case LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_ACTIVE:
            {
                status_text = "Active";
                break;
            }

            case LLLocalMeshFile::LLLocalMeshFileStatus::STATUS_ERROR:
            {
                status_text = "Error";
                break;
            }

            default:
            {
                status_text = "Error";
                break;
            }
        }

        std::string lod_state;
        for (int lod_reverse_iter = 3; lod_reverse_iter >= 0; --lod_reverse_iter)
        {
            std::string current_state = "[";

            if (cinfo.mLODAvailability[lod_reverse_iter])
            {
                current_state += std::to_string(lod_reverse_iter);
            }

            current_state += "]";

            if (lod_reverse_iter > 0)
            {
                current_state += " ";
            }

            lod_state += current_state;
        }
        std::string icon_name = LLInventoryIcon::getIconName(
                                                    LLAssetType::AT_OBJECT,
                                                    LLInventoryType::IT_OBJECT);
        LLSD element;

        element["columns"][0]["column"] = "unit_status";
        element["columns"][0]["type"] = "text";
        element["columns"][0]["value"] = status_text;

        element["columns"][1]["column"] = "unit_name";
        element["columns"][1]["type"] = "text";
        element["columns"][1]["value"] = cinfo.mName;

        element["columns"][2]["column"] = "unit_lods";
        element["columns"][2]["type"] = "text";
        element["columns"][2]["value"] = lod_state;

        element["columns"][3]["column"] = "unit_objects";
        element["columns"][3]["type"] = "text";
        element["columns"][3]["value"] = std::to_string(cinfo.mObjectList.size());

        element["columns"][4]["column"] = "unit_id_HIDDEN";
        element["columns"][4]["type"] = "text";
        element["columns"][4]["value"] = cinfo.mLocalID;
        element["value"] = "fcff33f4-82b7-367a-632e-1673b7142982";

        mScrollCtrl->addElement(element);
    }

    if ( keep_selection && (selected_num >= 0) && (selected_num < mScrollCtrl->getItemCount()) )
    {
        mScrollCtrl->selectNthItem(selected_num);
        reloadLowerUI();
    }
    else if (mScrollCtrl->getItemCount() > 0)
    {
        // select the last item in the list
        mScrollCtrl->selectNthItem( mScrollCtrl->getItemCount()-1 );
        reloadLowerUI();
    }
}

void LLFloaterLocalMesh::onFileListCommitCallback()
{
    reloadLowerUI();
    showLog();
}

void LLFloaterLocalMesh::reloadLowerUI()
{
    // do we have a valid target selected?
    bool selected_target_valid = !getCurrentSelectionIfValid().isNull();

    // do we have a valid file in list and selected?
    bool selected_file_loaded = false;
    bool selected_file_active = false;
    LLUUID selected_file_id;
    selected_file_id.setNull();

    if (auto selected_item = mScrollCtrl->getFirstSelected())
    {
        auto selected_column = selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
        if (selected_column)
        {
            selected_file_id = selected_column->getValue().asUUID();
        }
    }

    std::vector<std::string> selected_object_list;
    if (!selected_file_id.isNull())
    {
        const auto& fileinfo_vector = LLLocalMeshSystem::getInstance()->getFileInfoVector();
        for (const auto& fileinfo : fileinfo_vector)
        {
            if (selected_file_id == fileinfo.mLocalID)
            {
                switch (fileinfo.mStatus)
                {
                    case LLLocalMeshFile::STATUS_ACTIVE:
                    {
                        selected_file_loaded = true;
                        selected_file_active = true;
                        selected_object_list = fileinfo.mObjectList;
                        break;
                    }

                    case LLLocalMeshFile::STATUS_ERROR:
                    {
                        selected_file_loaded = true;
                        break;
                    }

                    default:
                    {
                        // no other status enables anything.
                        break;
                    }
                }

                break;
            }
        }
    }


    // checks done
    // toggle target-relevant elements
    auto btn_clear = getChild<LLButton>("btn_clear");
    if (btn_clear)
    {
        btn_clear->setEnabled(selected_target_valid);
    }

    // toggle elements that need a file to be loaded, even if it's got an error
    auto btn_remove = getChild<LLButton>("btn_remove");
    if (btn_remove)
    {
        btn_remove->setEnabled(selected_file_loaded);

    }

    auto btn_reload = getChild<LLButton>("btn_reload");
    if (btn_reload)
    {
        btn_reload->setEnabled(selected_file_loaded);
    }

    // apply needs a target to be selected AND a file to be loaded and active
    auto btn_apply = getChild<LLButton>("btn_apply");
    if (btn_apply)
    {
        btn_apply->setEnabled((selected_target_valid) && (selected_file_active));
    }

    // objectlist needs the file to be loaded and active
    auto objectlist_combo_box = getChild<LLComboBox>("object_apply_list");
    if (objectlist_combo_box)
    {
        objectlist_combo_box->setEnabled(selected_file_active);
        objectlist_combo_box->clearRows();

        // and if it is loaded & active, fill object list
        if (selected_file_active && (!selected_object_list.empty()))
        {
            for (const auto& object_name : selected_object_list)
            {
                objectlist_combo_box->addSimpleElement(object_name);
            }

            objectlist_combo_box->selectFirstItem();
        }
    }
}

void LLFloaterLocalMesh::toggleSelectTool(bool toggle)
{
    // hiding this in it's own function so i can experiment with the ux of
    // toggling it in either onOpen/onClose, or onFocusReceived/onFocusLost

    if (toggle)
    {
        LLSelectMgr::getInstance()->setForceSelection(true);
        LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());

        // this one's necessary for the tool to actually be active, go figure.
        mObjectSelection = LLSelectMgr::getInstance()->getSelection();
    }

    else
    {
        if (LLToolMgr::getInstance()->getBaseTool() == LLToolCompInspect::getInstance())
        {
            LLToolMgr::getInstance()->clearTransientTool();
        }

        LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);
    }
}

LLUUID LLFloaterLocalMesh::getCurrentSelectionIfValid() const
{
    LLUUID result;
    result.setNull();

    auto current_object = gObjectList.findObject(mLastSelectedObject);
    if (!current_object)
    {
        return result;
    }

    // turning a non-mesh object into mesh is immensely more hacky, let's not do that.
    if (!current_object->isMesh())
    {
        return result;
    }

    // any other rules can be set here

    result = mLastSelectedObject;
    return result;
}


