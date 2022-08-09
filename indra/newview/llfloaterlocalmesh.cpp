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
#include "llviewermenufile.h"
#include "llscrolllistctrl.h"
#include "llcheckboxctrl.h"
#include "llviewertexteditor.h"
#include "llcombobox.h"
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "lltoolcomp.h"

#include "llviewerobjectlist.h"

// own headers
#include "llfloaterlocalmesh.h"
#include "lllocalmesh.h"

static const S32 LOCAL_TRACKING_ID_COLUMN = 4;


/*================================*/
/*  LLFloaterLocalMeshFilePicker  */
/*================================*/
class LLFloaterLocalMeshFilePicker : public LLFilePickerThread
{
public:
	LLFloaterLocalMeshFilePicker(LLFloaterLocalMesh* parent_floater);
	virtual void notify(const std::vector<std::string>& filenames);

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
	mLastSelectedObject.setNull();
}

LLFloaterLocalMesh::~LLFloaterLocalMesh(void)
{

}

void LLFloaterLocalMesh::onOpen(const LLSD & key)
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
}

BOOL LLFloaterLocalMesh::postBuild()
{
	childSetAction("btn_add", LLFloaterLocalMesh::onBtnAdd, this);
	childSetAction("btn_reload", LLFloaterLocalMesh::onBtnReload, this);
	childSetAction("btn_remove", LLFloaterLocalMesh::onBtnRemove, this);
	childSetAction("btn_apply", LLFloaterLocalMesh::onBtnApply, this);
	childSetAction("btn_clear", LLFloaterLocalMesh::onBtnClear, this);
	childSetAction("btn_showlog", LLFloaterLocalMesh::onBtnShowLog, this);
	
	LLScrollListCtrl* scroll_ctrl = getChild<LLScrollListCtrl>("l_name_list");
	if (scroll_ctrl)
	{
		scroll_ctrl->setCommitCallback(boost::bind(&LLFloaterLocalMesh::onFileListCommitCallback, this));
	}

	reloadLowerUI();
	return TRUE;
}

void LLFloaterLocalMesh::draw()
{
	// check if selection has changed.
	LLUUID selected_id;
	selected_id.setNull();

	auto current_object = LLSelectMgr::getInstance()->getSelection()->getFirstObject();
	if (current_object)
	{
		selected_id = current_object->getID();
	}

	if (selected_id != mLastSelectedObject)
	{
		mLastSelectedObject = selected_id;
		onSelectionChangedCallback();
	}

	// continue drawing
	LLFloater::draw();
}

void LLFloaterLocalMesh::onBtnAdd(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	(new LLFloaterLocalMeshFilePicker(self))->getFile();
}

void LLFloaterLocalMesh::onBtnAddCallback(std::string filename)
{
	bool try_lods = false;
	auto checkbox_use_lods = getChild<LLCheckBoxCtrl>("chkbox_use_lods");
	if (checkbox_use_lods)
	{
		try_lods = checkbox_use_lods->get();
	}

	LLLocalMeshSystem::getInstance()->addFile(filename, try_lods);
}


void LLFloaterLocalMesh::onBtnReload(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	if (!self)
	{
		return;
	}

	LLScrollListCtrl* scroll_ctrl = self->getChild<LLScrollListCtrl>("l_name_list");
	if (!scroll_ctrl)
	{
		return;
	}

	auto selected_item = scroll_ctrl->getFirstSelected();
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

void LLFloaterLocalMesh::onBtnRemove(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	if (!self)
	{
		return;
	}

	LLScrollListCtrl* scroll_ctrl = self->getChild<LLScrollListCtrl>("l_name_list");
	if (!scroll_ctrl)
	{
		return;
	}

	// more checks necessary, apparently.
	auto selected_item = scroll_ctrl->getFirstSelected();
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
	self->reloadLowerUI();
}

void LLFloaterLocalMesh::onBtnApply(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	if (!self)
	{
		return;
	}

	// check scroll_ctrl pointers
	auto scroll_ctrl = self->getChild<LLScrollListCtrl>("l_name_list");
	if (!scroll_ctrl)
	{
		return;
	}

	auto scroll_ctrl_selected_item = scroll_ctrl->getFirstSelected();
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
	auto objectlist_combo_box = self->getChild<LLComboBox>("object_apply_list");
	if (!objectlist_combo_box)
	{
		return;
	}

	// checkbox pointer
	bool use_scale = false;
	auto checkbox_use_scale = self->getChild<LLCheckBoxCtrl>("chkbox_use_scale");
	if (checkbox_use_scale)
	{
		use_scale = checkbox_use_scale->get();
	}

	// make sure the selection is still valid, and if so - get id.
	LLUUID selected_object_id = self->getCurrentSelectionIfValid();
	if (selected_object_id.isNull())
	{
		return;
	}

	// get selected local file id, object idx and use_scale boolean
	LLUUID file_id = scroll_ctrl_selected_column->getValue().asUUID();
	int object_idx = objectlist_combo_box->getFirstSelectedIndex();

	// finally tell local mesh system to apply
	LLLocalMeshSystem::getInstance()->applyVObject(selected_object_id, file_id, object_idx, use_scale);
}

void LLFloaterLocalMesh::onBtnClear(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	if (!self)
	{
		return;
	}

	LLUUID selected_object_id = self->getCurrentSelectionIfValid();
	if (selected_object_id.isNull())
	{
		return;
	}

	LLLocalMeshSystem::getInstance()->clearVObject(selected_object_id);
}

void LLFloaterLocalMesh::onBtnShowLog(void* userdata)
{
	LLFloaterLocalMesh* self = (LLFloaterLocalMesh*)userdata;
	if (!self)
	{
		return;
	}

	LLScrollListCtrl* scroll_ctrl = self->getChild<LLScrollListCtrl>("l_name_list");
	LLTextEditor* text_ctrl = self->getChild<LLViewerTextEditor>("text_log");
	LLButton* log_btn = self->getChild<LLButton>("btn_showlog");

	if ((!scroll_ctrl) || (!text_ctrl) || (!log_btn))
	{
		return;
	}

	bool scroll_visible = scroll_ctrl->getVisible();
	
	std::string btn_text = (scroll_visible) ? "Show List" : "Show Log";
	log_btn->setLabel(btn_text);

	scroll_ctrl->setVisible(!scroll_visible);
	text_ctrl->setVisible(scroll_visible);

	if (text_ctrl->getVisible())
	{
		text_ctrl->clear();

		// make sure something is actually selected, or we crash
		auto scroll_ctrl_selected_item = scroll_ctrl->getFirstSelected();
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

		for (auto log_string : log)
		{
			text_ctrl->appendText(log_string, true);
		}

	}
}

void LLFloaterLocalMesh::reloadFileList(bool keep_selection)
{
	LLScrollListCtrl* scroll_ctrl = getChild<LLScrollListCtrl>("l_name_list");
	if (!scroll_ctrl)
	{
		return;
	}

	auto& fileinfo_vec = LLLocalMeshSystem::getInstance()->getFileInfoVector();
	int selected_num = scroll_ctrl->getFirstSelectedIndex();

	scroll_ctrl->clearRows();
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

		scroll_ctrl->addElement(element);
	}

	if ((keep_selection) && (selected_num >= 0) && (selected_num < scroll_ctrl->getItemCount()))
	{
		scroll_ctrl->selectNthItem(selected_num);
		reloadLowerUI();
	}
	else if (scroll_ctrl->getItemCount() == 1)
	{
		// if we've just got the one item, select it.
		// this prevents the need to explicitly select a list item when first adding a file.
		scroll_ctrl->selectNthItem(0);
	}
}

void LLFloaterLocalMesh::onFileListCommitCallback()
{
	// i rather not register reloadLowerUI as callback, unless that's preferable?
	reloadLowerUI();
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

	auto scroll_ctrl = getChild<LLScrollListCtrl>("l_name_list");
	if (scroll_ctrl)
	{
		auto selected_item = scroll_ctrl->getFirstSelected();
		if (selected_item)
		{
			auto selected_column = selected_item->getColumn(LOCAL_TRACKING_ID_COLUMN);
			if (selected_column)
			{
				selected_file_id = selected_column->getValue().asUUID();
			}
		}
	}

	std::vector<std::string> selected_object_list;
	if (!selected_file_id.isNull())
	{
		auto& fileinfo_vector = LLLocalMeshSystem::getInstance()->getFileInfoVector();
		for (auto fileinfo : fileinfo_vector)
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
		if ((selected_file_active) && (!selected_object_list.empty()))
		{
			for (auto object_name : selected_object_list)
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
		LLSelectMgr::getInstance()->setForceSelection(TRUE);
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

LLUUID LLFloaterLocalMesh::getCurrentSelectionIfValid()
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


