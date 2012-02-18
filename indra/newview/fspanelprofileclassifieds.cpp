/** 
 * @file fspanelprofileclassifieds.cpp
 * @brief FSPanelClassifieds and related class implementations
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

#include "llviewerprecompiledheaders.h"

#include "fspanelprofileclassifieds.h"

#include "llagent.h"
#include "llagentpicksinfo.h"
#include "llavatarconstants.h"
// #include "llcommandhandler.h"
// #include "lldispatcher.h"
#include "llflatlistview.h"
#include "llfloaterreg.h"
// #include "llfloatersidepanelcontainer.h"
#include "llfloaterworldmap.h"
#include "llnotificationsutil.h"
#include "lltexturectrl.h"
#include "lltoggleablemenu.h"
#include "lltrans.h"
#include "llviewergenericmessage.h"	// send_generic_message
#include "llmenugl.h"
#include "llviewermenu.h"
#include "llregistry.h"

#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
#include "llavatarpropertiesprocessor.h"
// #include "llfloatersidepanelcontainer.h"
#include "fspanelprofile.h"
#include "fspanelclassified.h"

static const std::string XML_BTN_NEW = "new_btn";
static const std::string XML_BTN_DELETE = "trash_btn";
static const std::string XML_BTN_INFO = "info_btn";
static const std::string XML_BTN_TELEPORT = "teleport_btn";
static const std::string XML_BTN_SHOW_ON_MAP = "show_on_map_btn";

static const std::string PICK_ID("pick_id");
static const std::string PICK_CREATOR_ID("pick_creator_id");
static const std::string PICK_NAME("pick_name");

static const std::string CLASSIFIED_ID("classified_id");
static const std::string CLASSIFIED_NAME("classified_name");


static LLRegisterPanelClassWrapper<FSPanelClassifieds> t_panel_classifieds("panel_profile_classified");

//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// FSPanelClassifieds
//-----------------------------------------------------------------------------
FSPanelClassifieds::FSPanelClassifieds()
:	FSPanelProfileTab(),
	mPopupMenu(NULL),
	// mProfilePanel(NULL),
	mClassifiedsList(NULL),
	mPlusMenu(NULL),
	mClassifiedsAccTab(NULL),
	mPanelClassifiedInfo(NULL),
	mNoClassifieds(false)
{
}

FSPanelClassifieds::~FSPanelClassifieds()
{
	if(getAvatarId().notNull())
	{
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(),this);
	}
}

void* FSPanelClassifieds::create(void* data /* = NULL */)
{
	return new FSPanelClassifieds();
}

void FSPanelClassifieds::updateData()
{
	// Send Picks request only when we need to, not on every onOpen(during tab switch).
	if(isDirty())
	{
		mNoClassifieds = false;

		mNoItemsLabel->setValue(LLTrans::getString("PicksClassifiedsLoadingText"));
		mNoItemsLabel->setVisible(TRUE);

		mClassifiedsList->clear();
		LLAvatarPropertiesProcessor::getInstance()->sendAvatarClassifiedsRequest(getAvatarId());
	}
}

void FSPanelClassifieds::processProperties(void* data, EAvatarProcessorType type)
{
	if(APT_CLASSIFIEDS == type)
	{
		LLAvatarClassifieds* c_info = static_cast<LLAvatarClassifieds*>(data);
		if(c_info && getAvatarId() == c_info->target_id)
		{
			// do not clear classified list in case we will receive two or more data packets.
			// list has been cleared in updateData(). (fix for EXT-6436)

			LLAvatarClassifieds::classifieds_list_t::const_iterator it = c_info->classifieds_list.begin();
			for(; c_info->classifieds_list.end() != it; ++it)
			{
				LLAvatarClassifieds::classified_data c_data = *it;

				FSClassifiedItem* c_item = new FSClassifiedItem(getAvatarId(), c_data.classified_id);
				c_item->childSetAction("info_chevron", boost::bind(&FSPanelClassifieds::onClickInfo, this));
				c_item->setClassifiedName(c_data.name);

				LLSD pick_value = LLSD();
				pick_value.insert(CLASSIFIED_ID, c_data.classified_id);
				pick_value.insert(CLASSIFIED_NAME, c_data.name);

				if (!findClassifiedById(c_data.classified_id))
				{
					mClassifiedsList->addItem(c_item, pick_value);
				}

				c_item->setDoubleClickCallback(boost::bind(&FSPanelClassifieds::onDoubleClickClassifiedItem, this, _1));
				c_item->setRightMouseUpCallback(boost::bind(&FSPanelClassifieds::onRightMouseUpItem, this, _1, _2, _3, _4));
				c_item->setMouseUpCallback(boost::bind(&FSPanelClassifieds::updateButtons, this));
			}

			showAccordion("tab_classifieds", mClassifiedsList->size());

			resetDirty();
			updateButtons();
		}
		
		mNoClassifieds = !mClassifiedsList->size();

        bool no_data = mNoClassifieds;
        mNoItemsLabel->setVisible(no_data);
        if (no_data)
        {
            if(getAvatarId() == gAgentID)
            {
                mNoItemsLabel->setValue(LLTrans::getString("NoPicksClassifiedsText"));
            }
            else
            {
                mNoItemsLabel->setValue(LLTrans::getString("NoAvatarPicksClassifiedsText"));
            }
        }

        enableControls();
	}
}

FSClassifiedItem* FSPanelClassifieds::getSelectedClassifiedItem()
{
	LLPanel* selected_item = mClassifiedsList->getSelectedItem();
	if (!selected_item) 
	{
		return NULL;
	}
	return dynamic_cast<FSClassifiedItem*>(selected_item);
}

BOOL FSPanelClassifieds::postBuild()
{
	mClassifiedsList = getChild<LLFlatListView>("classifieds_list");
	mClassifiedsList->setCommitOnSelectionChange(true);
	mClassifiedsList->setCommitCallback(boost::bind(&FSPanelClassifieds::onListCommit, this, mClassifiedsList));
	mClassifiedsList->setNoItemsCommentText(getString("no_classifieds"));

	mNoItemsLabel = getChild<LLUICtrl>("picks_panel_text");

	childSetAction(XML_BTN_NEW, boost::bind(&FSPanelClassifieds::onClickPlusBtn, this));
	childSetAction(XML_BTN_DELETE, boost::bind(&FSPanelClassifieds::onClickDelete, this));
	childSetAction(XML_BTN_TELEPORT, boost::bind(&FSPanelClassifieds::onClickTeleport, this));
	childSetAction(XML_BTN_SHOW_ON_MAP, boost::bind(&FSPanelClassifieds::onClickMap, this));
	childSetAction(XML_BTN_INFO, boost::bind(&FSPanelClassifieds::onClickInfo, this));

	mClassifiedsAccTab = getChild<LLAccordionCtrlTab>("tab_classifieds");
	mClassifiedsAccTab->setDropDownStateChangedCallback(boost::bind(&FSPanelClassifieds::onAccordionStateChanged, this, mClassifiedsAccTab));
	mClassifiedsAccTab->setDisplayChildren(false);
	
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registar;
	registar.add("Pick.Info", boost::bind(&FSPanelClassifieds::onClickInfo, this));
	registar.add("Pick.Edit", boost::bind(&FSPanelClassifieds::onClickMenuEdit, this)); 
	registar.add("Pick.Teleport", boost::bind(&FSPanelClassifieds::onClickTeleport, this));
	registar.add("Pick.Map", boost::bind(&FSPanelClassifieds::onClickMap, this));
	registar.add("Pick.Delete", boost::bind(&FSPanelClassifieds::onClickDelete, this));
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registar;
	enable_registar.add("Pick.Enable", boost::bind(&FSPanelClassifieds::onEnableMenuItem, this, _2));

	mPopupMenu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>("menu_picks.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());

	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar plus_registar;
	plus_registar.add("Picks.Plus.Action", boost::bind(&FSPanelClassifieds::onPlusMenuItemClicked, this, _2));
	mEnableCallbackRegistrar.add("Picks.Plus.Enable", boost::bind(&FSPanelClassifieds::isActionEnabled, this, _2));
	mPlusMenu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_picks_plus.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	
	return TRUE;
}

void FSPanelClassifieds::onPlusMenuItemClicked(const LLSD& param)
{
	std::string value = param.asString();

	if("new_classified" == value)
	{
		createNewClassified();
	}
}

bool FSPanelClassifieds::isActionEnabled(const LLSD& userdata) const
{
	std::string command_name = userdata.asString();

	if (command_name == "new_pick" && LLAgentPicksInfo::getInstance()->isPickLimitReached())
	{
		return false;
	}

	return true;
}

bool FSPanelClassifieds::isClassifiedPublished(FSClassifiedItem* c_item)
{
	if(c_item)
	{
		FSPanelClassifiedEdit* panel = mEditClassifiedPanels[c_item->getClassifiedId()];
		if(panel)
		{
			 return !panel->isNewWithErrors();
		}

		// we've got this classified from server - it's published
		return true;
	}
	return false;
}

void FSPanelClassifieds::onAccordionStateChanged(const LLAccordionCtrlTab* acc_tab)
{
	if(!mClassifiedsAccTab->getDisplayChildren())
	{
		mClassifiedsList->resetSelection(true);
	}

	updateButtons();
}

void FSPanelClassifieds::onOpen(const LLSD& key)
{
	const LLUUID id(key.asUUID());
	BOOL self = (gAgent.getID() == id);

	// only agent can edit her picks 
	getChildView("edit_panel")->setEnabled(self);
	getChildView("edit_panel")->setVisible( self);

	// Disable buttons when viewing profile for first time
	if(getAvatarId() != id)
	{
		getChildView(XML_BTN_INFO)->setEnabled(FALSE);
		getChildView(XML_BTN_TELEPORT)->setEnabled(FALSE);
		getChildView(XML_BTN_SHOW_ON_MAP)->setEnabled(FALSE);
	}

	// and see a special title - set as invisible by default in xml file
	if (self)
	{
		getChildView("pick_title")->setVisible( !self);
		getChildView("pick_title_agent")->setVisible( self);

		mPopupMenu->setItemVisible("pick_delete", TRUE);
		mPopupMenu->setItemVisible("pick_edit", TRUE);
		mPopupMenu->setItemVisible("pick_separator", TRUE);
	}

	if(getAvatarId() != id)
	{
		showAccordion("tab_picks", false);
		showAccordion("tab_classifieds", false);

		mClassifiedsList->goToTop();
		// Set dummy value to make panel dirty and make it reload picks
		setValue(LLSD());
	}

	FSPanelProfileTab::onOpen(key);

	updateData();
	updateButtons();  
}

void FSPanelClassifieds::onClosePanel()
{
	if (mPanelClassifiedInfo)
	{
		onPanelClassifiedClose(mPanelClassifiedInfo);
	}
}

void FSPanelClassifieds::onListCommit(const LLFlatListView* f_list)
{
	// Make sure only one of the lists has selection.
	// if(f_list == mPicksList)
	// {
		// mClassifiedsList->resetSelection(true);
	// }
	// else if(f_list == mClassifiedsList)
	// {
		// mPicksList->resetSelection(true);
	// }
	// else
	// {
		// llwarns << "Unknown list" << llendl;
	// }

	updateButtons();
}

//static
void FSPanelClassifieds::onClickDelete()
{
	LLSD value = mClassifiedsList->getSelectedValue();
	if(value.isDefined())
	{
		LLSD args; 
		args["NAME"] = value[CLASSIFIED_NAME]; 
		LLNotificationsUtil::add("DeleteClassified", args, LLSD(), boost::bind(&FSPanelClassifieds::callbackDeleteClassified, this, _1, _2)); 
		return;
	}
}

bool FSPanelClassifieds::callbackDeleteClassified(const LLSD& notification, const LLSD& response) 
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLSD value = mClassifiedsList->getSelectedValue();

	if (0 == option)
	{
		LLAvatarPropertiesProcessor::instance().sendClassifiedDelete(value[CLASSIFIED_ID]);
		mClassifiedsList->removeItemByValue(value);
	}
	updateButtons();
	return false;
}

bool FSPanelClassifieds::callbackTeleport( const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		onClickTeleport();
	}
	return false;
}

//static
void FSPanelClassifieds::onClickTeleport()
{
	FSClassifiedItem* c_item = getSelectedClassifiedItem();

	LLVector3d pos;
	if(c_item)
	{
		pos = c_item->getPosGlobal();
		FSPanelClassifiedInfo::sendClickMessage("teleport", false,
			c_item->getClassifiedId(), LLUUID::null, pos, LLStringUtil::null);
	}

	if (!pos.isExactlyZero())
	{
		gAgent.teleportViaLocation(pos);
		LLFloaterWorldMap::getInstance()->trackLocation(pos);
	}
}

//static
void FSPanelClassifieds::onClickMap()
{
	FSClassifiedItem* c_item = getSelectedClassifiedItem();

	LLVector3d pos;
	if(c_item)
	{
		FSPanelClassifiedInfo::sendClickMessage("map", false,
			c_item->getClassifiedId(), LLUUID::null, pos, LLStringUtil::null);
		pos = c_item->getPosGlobal();
	}

	LLFloaterWorldMap::getInstance()->trackLocation(pos);
	LLFloaterReg::showInstance("world_map", "center");
}


void FSPanelClassifieds::onRightMouseUpItem(LLUICtrl* item, S32 x, S32 y, MASK mask)
{
	updateButtons();

	if (mPopupMenu)
	{
		mPopupMenu->buildDrawLabels();
		mPopupMenu->updateParent(LLMenuGL::sMenuContainer);
		((LLContextMenu*)mPopupMenu)->show(x, y);
		LLMenuGL::showPopup(item, mPopupMenu, x, y);
	}
}

void FSPanelClassifieds::onDoubleClickClassifiedItem(LLUICtrl* item)
{
	LLSD value = mClassifiedsList->getSelectedValue();
	if (value.isUndefined()) return;

	LLSD args; 
	args["CLASSIFIED"] = value[CLASSIFIED_NAME]; 
	LLNotificationsUtil::add("TeleportToClassified", args, LLSD(), boost::bind(&FSPanelClassifieds::callbackTeleport, this, _1, _2)); 
}

void FSPanelClassifieds::updateButtons()
{
	bool has_selected = mClassifiedsList->numSelected() > 0;

	if (getAvatarId() == gAgentID)
	{
		getChildView(XML_BTN_DELETE)->setEnabled(has_selected);
	}

	getChildView(XML_BTN_INFO)->setEnabled(has_selected);
	getChildView(XML_BTN_TELEPORT)->setEnabled(has_selected);
	getChildView(XML_BTN_SHOW_ON_MAP)->setEnabled(has_selected);

	FSClassifiedItem* c_item = dynamic_cast<FSClassifiedItem*>(mClassifiedsList->getSelectedItem());
	if(c_item)
	{
		getChildView(XML_BTN_INFO)->setEnabled(isClassifiedPublished(c_item));
	}
}

// void FSPanelClassifieds::setProfilePanel(FSPanelClassifieds* profile_panel)
// {
	// mProfilePanel = profile_panel;
// }

void FSPanelClassifieds::onClickPlusBtn()
{
	LLRect rect(getChildView(XML_BTN_NEW)->getRect());

	mPlusMenu->updateParent(LLMenuGL::sMenuContainer);
	mPlusMenu->setButtonRect(rect, this);
	LLMenuGL::showPopup(this, mPlusMenu, rect.mLeft, rect.mTop);
}

void FSPanelClassifieds::createNewClassified()
{
	FSPanelClassifiedEdit* panel = NULL;
	createClassifiedEditPanel(&panel);

	// getProfilePanel()->openPanel(panel, LLSD());
	openPanel(panel, LLSD());
}

void FSPanelClassifieds::onClickInfo()
{
	if(mClassifiedsList->numSelected() > 0)
	{
		openClassifiedInfo();
	}
}

void FSPanelClassifieds::openClassifiedInfo()
{
	LLSD selected_value = mClassifiedsList->getSelectedValue();
	if (selected_value.isUndefined()) return;

	FSClassifiedItem* c_item = getSelectedClassifiedItem();
	LLSD params;
	params["classified_id"] = c_item->getClassifiedId();
	params["classified_creator_id"] = c_item->getAvatarId();
	params["classified_snapshot_id"] = c_item->getSnapshotId();
	params["classified_name"] = c_item->getClassifiedName();
	params["classified_desc"] = c_item->getDescription();
	params["from_search"] = false;

	openClassifiedInfo(params);
}

void FSPanelClassifieds::openClassifiedInfo(const LLSD &params)
{
	createClassifiedInfoPanel();
	// getProfilePanel()->openPanel(mPanelClassifiedInfo, params);
	openPanel(mPanelClassifiedInfo, params);
}

void FSPanelClassifieds::openClassifiedEdit(const LLSD& params)
{
	LLUUID classified_id = params["classified_id"].asUUID();;
	llinfos << "opening classified " << classified_id << " for edit" << llendl;
	editClassified(classified_id);
}

void FSPanelClassifieds::showAccordion(const std::string& name, bool show)
{
	LLAccordionCtrlTab* tab = getChild<LLAccordionCtrlTab>(name);
	tab->setVisible(show);
	LLAccordionCtrl* acc = getChild<LLAccordionCtrl>("accordion");
	acc->arrange();
}

void FSPanelClassifieds::onPanelPickClose(LLPanel* panel)
{
	// getProfilePanel()->closePanel(panel);
	closePanel(panel);
}

void FSPanelClassifieds::onPanelClassifiedSave(FSPanelClassifiedEdit* panel)
{
	if(!panel->canClose())
	{
		return;
	}

	if(panel->isNew())
	{
		mEditClassifiedPanels[panel->getClassifiedId()] = panel;

		FSClassifiedItem* c_item = new FSClassifiedItem(getAvatarId(), panel->getClassifiedId());
		c_item->fillIn(panel);

		LLSD c_value;
		c_value.insert(CLASSIFIED_ID, c_item->getClassifiedId());
		c_value.insert(CLASSIFIED_NAME, c_item->getClassifiedName());
		mClassifiedsList->addItem(c_item, c_value, ADD_TOP);

		c_item->setDoubleClickCallback(boost::bind(&FSPanelClassifieds::onDoubleClickClassifiedItem, this, _1));
		c_item->setRightMouseUpCallback(boost::bind(&FSPanelClassifieds::onRightMouseUpItem, this, _1, _2, _3, _4));
		c_item->setMouseUpCallback(boost::bind(&FSPanelClassifieds::updateButtons, this));
		c_item->childSetAction("info_chevron", boost::bind(&FSPanelClassifieds::onClickInfo, this));

		// order does matter, showAccordion will invoke arrange for accordions.
		mClassifiedsAccTab->changeOpenClose(false);
		showAccordion("tab_classifieds", true);
	}
	else if(panel->isNewWithErrors())
	{
		FSClassifiedItem* c_item = dynamic_cast<FSClassifiedItem*>(mClassifiedsList->getSelectedItem());
		llassert(c_item);
		if (c_item)
		{
			c_item->fillIn(panel);
		}
	}
	else 
	{
		onPanelClassifiedClose(panel);
		return;
	}

	onPanelPickClose(panel);
	updateButtons();
}

void FSPanelClassifieds::onPanelClassifiedClose(FSPanelClassifiedInfo* panel)
{
	if(panel->getInfoLoaded() && !panel->isDirty())
	{
		std::vector<LLSD> values;
		mClassifiedsList->getValues(values);
		for(size_t n = 0; n < values.size(); ++n)
		{
			LLUUID c_id = values[n][CLASSIFIED_ID].asUUID();
			if(panel->getClassifiedId() == c_id)
			{
				FSClassifiedItem* c_item = dynamic_cast<FSClassifiedItem*>(
					mClassifiedsList->getItemByValue(values[n]));
				llassert(c_item);
				if (c_item)
				{
					c_item->setClassifiedName(panel->getClassifiedName());
					c_item->setDescription(panel->getDescription());
					c_item->setSnapshotId(panel->getSnapshotId());
				}
			}
		}
	}

	onPanelPickClose(panel);
	updateButtons();
}

void FSPanelClassifieds::createClassifiedInfoPanel()
{
	mPanelClassifiedInfo = FSPanelClassifiedInfo::create();
	mPanelClassifiedInfo->setExitCallback(boost::bind(&FSPanelClassifieds::onPanelClassifiedClose, this, mPanelClassifiedInfo));
	mPanelClassifiedInfo->setEditClassifiedCallback(boost::bind(&FSPanelClassifieds::onPanelClassifiedEdit, this));
	mPanelClassifiedInfo->setVisible(FALSE);
}

void FSPanelClassifieds::createClassifiedEditPanel(FSPanelClassifiedEdit** panel)
{
	if(panel)
	{
		FSPanelClassifiedEdit* new_panel = FSPanelClassifiedEdit::create();
		new_panel->setExitCallback(boost::bind(&FSPanelClassifieds::onPanelClassifiedClose, this, new_panel));
		new_panel->setSaveCallback(boost::bind(&FSPanelClassifieds::onPanelClassifiedSave, this, new_panel));
		new_panel->setCancelCallback(boost::bind(&FSPanelClassifieds::onPanelClassifiedClose, this, new_panel));
		new_panel->setVisible(FALSE);
		*panel = new_panel;
	}
}

void FSPanelClassifieds::onPanelClassifiedEdit()
{
	LLSD selected_value = mClassifiedsList->getSelectedValue();
	if (selected_value.isUndefined()) 
	{
		return;
	}

	FSClassifiedItem* c_item = dynamic_cast<FSClassifiedItem*>(mClassifiedsList->getSelectedItem());
	llassert(c_item);
	if (!c_item)
	{
		return;
	}
	editClassified(c_item->getClassifiedId());
}

FSClassifiedItem *FSPanelClassifieds::findClassifiedById(const LLUUID& classified_id)
{
	// HACK - find item by classified id.  Should be a better way.
	std::vector<LLPanel*> items;
	mClassifiedsList->getItems(items);
	FSClassifiedItem* c_item = NULL;
	for(std::vector<LLPanel*>::iterator it = items.begin(); it != items.end(); ++it)
	{
		FSClassifiedItem *test_item = dynamic_cast<FSClassifiedItem*>(*it);
		if (test_item && test_item->getClassifiedId() == classified_id)
		{
			c_item = test_item;
			break;
		}
	}
	return c_item;
}

void FSPanelClassifieds::editClassified(const LLUUID&  classified_id)
{
	FSClassifiedItem* c_item = findClassifiedById(classified_id);
	if (!c_item)
	{
		llwarns << "item not found for classified_id " << classified_id << llendl;
		return;
	}

	LLSD params;
	params["classified_id"] = c_item->getClassifiedId();
	params["classified_creator_id"] = c_item->getAvatarId();
	params["snapshot_id"] = c_item->getSnapshotId();
	params["name"] = c_item->getClassifiedName();
	params["desc"] = c_item->getDescription();
	params["category"] = (S32)c_item->getCategory();
	params["content_type"] = (S32)c_item->getContentType();
	params["auto_renew"] = c_item->getAutoRenew();
	params["price_for_listing"] = c_item->getPriceForListing();
	params["location_text"] = c_item->getLocationText();

	FSPanelClassifiedEdit* panel = mEditClassifiedPanels[c_item->getClassifiedId()];
	if(!panel)
	{
		createClassifiedEditPanel(&panel);
		mEditClassifiedPanels[c_item->getClassifiedId()] = panel;
	}
	// getProfilePanel()->openPanel(panel, params);
	openPanel(panel, params);
	panel->setPosGlobal(c_item->getPosGlobal());
}

void FSPanelClassifieds::onClickMenuEdit()
{
	if(getSelectedClassifiedItem())
	{
		onPanelClassifiedEdit();
	}
}

bool FSPanelClassifieds::onEnableMenuItem(const LLSD& user_data)
{
	std::string param = user_data.asString();

	FSClassifiedItem* c_item = dynamic_cast<FSClassifiedItem*>(mClassifiedsList->getSelectedItem());
	if(c_item && "info" == param)
	{
		// dont show Info panel if classified was not created
		return isClassifiedPublished(c_item);
	}

	return true;
}

// inline FSPanelClassifieds* FSPanelClassifieds::getProfilePanel()
// {
	// llassert_always(NULL != mProfilePanel);
	// return mProfilePanel;
	// return this;
// }

//hack
void FSPanelClassifieds::openPanel(LLPanel* panel, const LLSD& params)
{
	// Hide currently visible panel (STORM-690).
	// mChildStack.push();

	// Add the panel or bring it to front.
	if (panel->getParent() != this)
	{
		addChild(panel);
	}
	else
	{
		sendChildToFront(panel);
	}

	panel->setVisible(TRUE);
	panel->setFocus(TRUE); // prevent losing focus by the floater
	panel->onOpen(params);

	LLRect new_rect = getRect();
	panel->reshape(new_rect.getWidth(), new_rect.getHeight());
	new_rect.setLeftTopAndSize(0, new_rect.getHeight(), new_rect.getWidth(), new_rect.getHeight());
	panel->setRect(new_rect);
}

//hack
void FSPanelClassifieds::closePanel(LLPanel* panel)
{
	panel->setVisible(FALSE);

	if (panel->getParent() == this) 
	{
		removeChild(panel);

		// Make the underlying panel visible.
		// mChildStack.pop();

		// Prevent losing focus by the floater
		const child_list_t* child_list = getChildList();
		if (child_list->size() > 0)
		{
			child_list->front()->setFocus(TRUE);
		}
		else
		{
			llwarns << "No underlying panel to focus." << llendl;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

FSClassifiedItem::FSClassifiedItem(const LLUUID& avatar_id, const LLUUID& classified_id)
 : LLPanel()
 , mAvatarId(avatar_id)
 , mClassifiedId(classified_id)
{
	buildFromFile("panel_classifieds_list_item.xml");

	LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(), this);
	LLAvatarPropertiesProcessor::getInstance()->sendClassifiedInfoRequest(getClassifiedId());
}

FSClassifiedItem::~FSClassifiedItem()
{
	LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(), this);
}

void FSClassifiedItem::processProperties(void* data, EAvatarProcessorType type)
{
	if(APT_CLASSIFIED_INFO != type)
	{
		return;
	}

	LLAvatarClassifiedInfo* c_info = static_cast<LLAvatarClassifiedInfo*>(data);
	if( !c_info || c_info->classified_id != getClassifiedId() )
	{
		return;
	}

	setClassifiedName(c_info->name);
	setDescription(c_info->description);
	setSnapshotId(c_info->snapshot_id);
	setPosGlobal(c_info->pos_global);

	LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(), this);
}

void set_child_visible2(LLView* parent, const std::string& child_name, bool visible)
{
	parent->getChildView(child_name)->setVisible(visible);
}

BOOL FSClassifiedItem::postBuild()
{
	setMouseEnterCallback(boost::bind(&set_child_visible2, this, "hovered_icon", true));
	setMouseLeaveCallback(boost::bind(&set_child_visible2, this, "hovered_icon", false));
	return TRUE;
}

void FSClassifiedItem::setValue(const LLSD& value)
{
	if (!value.isMap()) return;;
	if (!value.has("selected")) return;
	getChildView("selected_icon")->setVisible( value["selected"]);
}

void FSClassifiedItem::fillIn(FSPanelClassifiedEdit* panel)
{
	if(!panel)
	{
		return;
	}

	setClassifiedName(panel->getClassifiedName());
	setDescription(panel->getDescription());
	setSnapshotId(panel->getSnapshotId());
	setCategory(panel->getCategory());
	setContentType(panel->getContentType());
	setAutoRenew(panel->getAutoRenew());
	setPriceForListing(panel->getPriceForListing());
	setPosGlobal(panel->getPosGlobal());
	setLocationText(panel->getClassifiedLocation());
}

void FSClassifiedItem::setClassifiedName(const std::string& name)
{
	getChild<LLUICtrl>("name")->setValue(name);
}

void FSClassifiedItem::setDescription(const std::string& desc)
{
	getChild<LLUICtrl>("description")->setValue(desc);
}

void FSClassifiedItem::setSnapshotId(const LLUUID& snapshot_id)
{
	getChild<LLUICtrl>("picture")->setValue(snapshot_id);
}

LLUUID FSClassifiedItem::getSnapshotId()
{
	return getChild<LLUICtrl>("picture")->getValue();
}

//EOF
