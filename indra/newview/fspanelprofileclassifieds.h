/** 
 * @file fspanelprofileclassifieds.h
 * @brief FSPanelClassifieds and related class definitions
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_LLPANELPICKS_H
#define LL_LLPANELPICKS_H

#include "llpanel.h"
#include "v3dmath.h"
#include "lluuid.h"
#include "llavatarpropertiesprocessor.h"
#include "fspanelprofile.h"
#include "llregistry.h"

class LLAccordionCtrlTab;
class LLMessageSystem;
class LLVector3d;
class FSPanelProfileTab;
class LLAgent;
class LLMenuGL;
class FSClassifiedItem;
class LLFlatListView;
class LLToggleableMenu;
class FSPanelClassifiedInfo;
class FSPanelClassifiedEdit;

// *TODO
// Panel Picks has been consolidated with Classifieds (EXT-2095), give FSPanelClassifieds
// and corresponding files (cpp, h, xml) a new name. (new name is TBD at the moment)

class FSPanelClassifieds 
	: public FSPanelProfileTab
{
public:
	FSPanelClassifieds();
	~FSPanelClassifieds();

	static void* create(void* data);

	/*virtual*/ BOOL postBuild(void);

	/*virtual*/ void onOpen(const LLSD& key);

	/*virtual*/ void onClosePanel();

	void processProperties(void* data, EAvatarProcessorType type);

	void updateData();

	// returns the selected pick item
	FSClassifiedItem* getSelectedClassifiedItem();
	FSClassifiedItem* findClassifiedById(const LLUUID& classified_id);

	void createNewClassified();

protected:
	/*virtual*/void updateButtons();

private:
	void onClickDelete();
	void onClickTeleport();
	void onClickMap();

	void onPlusMenuItemClicked(const LLSD& param);
	bool isActionEnabled(const LLSD& userdata) const;

	bool isClassifiedPublished(FSClassifiedItem* c_item);

	void onListCommit(const LLFlatListView* f_list);

	//------------------------------------------------
	// Callbacks which require panel toggling
	//------------------------------------------------
	void onClickPlusBtn();
	void onClickInfo();
	void onPanelPickClose(LLPanel* panel);
	void onPanelClassifiedSave(FSPanelClassifiedEdit* panel);
	void onPanelClassifiedClose(FSPanelClassifiedInfo* panel);
	void onPanelClassifiedEdit();
	void editClassified(const LLUUID&  classified_id);
	void onClickMenuEdit();

	bool onEnableMenuItem(const LLSD& user_data);

	void openClassifiedInfo();
	void openClassifiedInfo(const LLSD& params);
	void openClassifiedEdit(const LLSD& params);

	bool callbackDeleteClassified(const LLSD& notification, const LLSD& response);
	bool callbackTeleport(const LLSD& notification, const LLSD& response);


	virtual void onDoubleClickClassifiedItem(LLUICtrl* item);
	virtual void onRightMouseUpItem(LLUICtrl* item, S32 x, S32 y, MASK mask);

	// FSPanelClassifieds* getProfilePanel();

	void createClassifiedInfoPanel();
	void createClassifiedEditPanel(FSPanelClassifiedEdit** panel);
    
    void openPanel(LLPanel* panel, const LLSD& params);
    void closePanel(LLPanel* panel);

	LLMenuGL* mPopupMenu;
	// FSPanelClassifieds* mProfilePanel;
	LLFlatListView* mClassifiedsList;
	FSPanelClassifiedInfo* mPanelClassifiedInfo;
	LLToggleableMenu* mPlusMenu;
	LLUICtrl* mNoItemsLabel;

	// <classified_id, edit_panel>
	typedef std::map<LLUUID, FSPanelClassifiedEdit*> panel_classified_edit_map_t;

	// This map is needed for newly created classifieds. The purpose of panel is to
	// sit in this map and listen to FSPanelClassifiedEdit::processProperties callback.
	panel_classified_edit_map_t mEditClassifiedPanels;

	//true if classifieds list is empty after processing classifieds
	bool mNoClassifieds;
};

class FSClassifiedItem : public LLPanel, public LLAvatarPropertiesObserver
{
public:

	FSClassifiedItem(const LLUUID& avatar_id, const LLUUID& classified_id);
	
	virtual ~FSClassifiedItem();

	/*virtual*/ void processProperties(void* data, EAvatarProcessorType type);

	/*virtual*/ BOOL postBuild();

	/*virtual*/ void setValue(const LLSD& value);

	void fillIn(FSPanelClassifiedEdit* panel);

	LLUUID getAvatarId() {return mAvatarId;}
	
	void setAvatarId(const LLUUID& avatar_id) {mAvatarId = avatar_id;}

	LLUUID getClassifiedId() {return mClassifiedId;}

	void setClassifiedId(const LLUUID& classified_id) {mClassifiedId = classified_id;}

	void setPosGlobal(const LLVector3d& pos) { mPosGlobal = pos; }

	const LLVector3d getPosGlobal() { return mPosGlobal; }

	void setLocationText(const std::string location) { mLocationText = location; }

	std::string getLocationText() { return mLocationText; }

	void setClassifiedName (const std::string& name);

	std::string getClassifiedName() { return getChild<LLUICtrl>("name")->getValue().asString(); }

	void setDescription(const std::string& desc);

	std::string getDescription() { return getChild<LLUICtrl>("description")->getValue().asString(); }

	void setSnapshotId(const LLUUID& snapshot_id);

	LLUUID getSnapshotId();

	void setCategory(U32 cat) { mCategory = cat; }

	U32 getCategory() { return mCategory; }

	void setContentType(U32 ct) { mContentType = ct; }

	U32 getContentType() { return mContentType; }

	void setAutoRenew(U32 renew) { mAutoRenew = renew; }

	bool getAutoRenew() { return mAutoRenew; }

	void setPriceForListing(S32 price) { mPriceForListing = price; }

	S32 getPriceForListing() { return mPriceForListing; }

private:
	LLUUID mAvatarId;
	LLUUID mClassifiedId;
	LLVector3d mPosGlobal;
	std::string mLocationText;
	U32 mCategory;
	U32 mContentType;
	bool mAutoRenew;
	S32 mPriceForListing;
};

#endif // LL_LLPANELPICKS_H
