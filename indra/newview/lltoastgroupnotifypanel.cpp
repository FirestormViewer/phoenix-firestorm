/**
 * @file lltoastgroupnotifypanel.cpp
 * @brief Panel for group notify toasts.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "lltoastgroupnotifypanel.h"

#include "llfocusmgr.h"

#include "llbutton.h"
#include "lliconctrl.h"
#include "llinventoryfunctions.h"
#include "llinventoryicon.h"
#include "llnotifications.h"
#include "llviewertexteditor.h"

#include "llavatarnamecache.h"
#include "lluiconstants.h"
#include "llui.h"
#include "llviewercontrol.h"
#include "lltrans.h"
#include "llstyle.h"

#include "llglheaders.h"
#include "llagent.h"
#include "llavatariconctrl.h"
#include "llfloaterinventory.h"
#include "llinventorytype.h"

#include "llgroupactions.h"
#include "llslurl.h"

const S32 LLToastGroupNotifyPanel::DEFAULT_MESSAGE_MAX_LINE_COUNT	= 7;

LLToastGroupNotifyPanel::LLToastGroupNotifyPanel(LLNotificationPtr& notification)
:	LLToastPanel(notification),
	mInventoryOffer(NULL)
{
	buildFromFile( "panel_group_notify.xml");
	const LLSD& payload = notification->getPayload();
	LLGroupData groupData;
	if (!gAgent.getGroupData(payload["group_id"].asUUID(),groupData))
	{
		llwarns << "Group notice for unknown group: " << payload["group_id"].asUUID() << llendl;
	}
	
	mGroupID = payload["group_id"].asUUID();

	//group icon
	LLIconCtrl* pGroupIcon = getChild<LLIconCtrl>("group_icon", TRUE);
	pGroupIcon->setValue(groupData.mInsigniaID);

	//header title
	std::string from_name = payload["sender_name"].asString();
	if (LLAvatarNameCache::useDisplayNames())
	{
		from_name = LLCacheName::buildUsername(from_name);
	}
	std::string from;
	LLStringUtil::format_map_t args;
	args["[SENDER]"] = from_name;
	args["[GROUPNAME]"] = LLSLURL("group", groupData.mID, "inspect").getSLURLString();
	from = LLTrans::getString("GroupNotifySender", args);
	
	LLTextBox* pTitleText = getChild<LLTextBox>("title");
	pTitleText->setValue(from);
	pTitleText->setToolTip(from);

	//message subject
	const std::string& subject = payload["subject"].asString();
	//message body
	const std::string& message = payload["message"].asString();

	std::string timeStr = "["+LLTrans::getString("TimeWeek")+"], ["
							+LLTrans::getString("TimeMth")+"] ["
							+LLTrans::getString("TimeDay")+"] ["
							+LLTrans::getString("TimeYear")+"] ["
							+LLTrans::getString("TimeHour12")+"]:["
							+LLTrans::getString("TimeMin")+"]:["
							+LLTrans::getString("TimeSec")+"] ["
							+LLTrans::getString("TimeAMPM")+"] ["
							+LLTrans::getString("TimeTimezone")+"]";
	const LLDate timeStamp = notification->getDate();
	LLDate notice_date = timeStamp.notNull() ? timeStamp : LLDate::now();
	LLSD substitution;
	substitution["datetime"] = (S32) notice_date.secondsSinceEpoch();
	LLStringUtil::format(timeStr, substitution);

	LLViewerTextEditor* pMessageText = getChild<LLViewerTextEditor>("message");
	pMessageText->clear();

	LLStyle::Params style;
	LLFontGL* subject_font = LLFontGL::getFontByName(getString("subject_font"));
	if (subject_font) 
		style.font = subject_font;
	pMessageText->appendText(subject, FALSE, style);

	LLFontGL* date_font = LLFontGL::getFontByName(getString("date_font"));
	if (date_font)
		style.font = date_font;
	pMessageText->appendText(timeStr + "\n", TRUE, style);
	
	style.font = pMessageText->getDefaultFont();
	pMessageText->appendText(message, TRUE, style);

	//attachment
	BOOL hasInventory = payload["inventory_offer"].isDefined();

	// attachment container (if any)
	LLPanel* pAttachContainer = getChild<LLPanel>("attachment_container");
	// attachment container label (if any)
	LLTextBox* pAttachContainerLabel = getChild<LLTextBox>("attachment_label");
	//attachment text
	LLTextBox * pAttachLink = getChild<LLTextBox>("attachment");
	//attachment icon
	LLIconCtrl* pAttachIcon = getChild<LLIconCtrl>("attachment_icon", TRUE);

	//If attachment is empty let it be invisible and not take place at the panel
	pAttachContainer->setVisible(hasInventory);
	pAttachContainerLabel->setVisible(hasInventory);
	pAttachLink->setVisible(hasInventory);
	pAttachIcon->setVisible(hasInventory);
	if (hasInventory) {
		pAttachLink->setValue(payload["inventory_name"]);

		mInventoryOffer = new LLOfferInfo(payload["inventory_offer"]);
		getChild<LLTextBox>("attachment")->setClickedCallback(boost::bind(
				&LLToastGroupNotifyPanel::onClickAttachment, this));

		LLUIImagePtr attachIconImg = LLInventoryIcon::getIcon(mInventoryOffer->mType,
												LLInventoryType::IT_TEXTURE);
		pAttachIcon->setValue(attachIconImg->getName());
	}
	else
	{
		LLRect message_rect = pMessageText->getRect();
		message_rect.mBottom -= 20;
		pMessageText->reshape(message_rect.getWidth(), message_rect.getHeight());
		pMessageText->setRect(message_rect);
	}

	//ok button
	LLButton* pOkBtn = getChild<LLButton>("btn_ok");
	pOkBtn->setClickedCallback((boost::bind(&LLToastGroupNotifyPanel::onClickOk, this)));
	setDefaultBtn(pOkBtn);

	//group notices button
	LLButton* pOkNotices = getChild<LLButton>("btn_notices");
	if (pOkNotices)
		pOkNotices->setClickedCallback((boost::bind(&LLToastGroupNotifyPanel::onClickGroupNotices, this)));

	S32 maxLinesCount;
	std::istringstream ss( getString("message_max_lines_count") );
	if (!(ss >> maxLinesCount))
	{
		maxLinesCount = DEFAULT_MESSAGE_MAX_LINE_COUNT;
	}
	snapToMessageHeight(pMessageText, maxLinesCount);
}

// virtual
LLToastGroupNotifyPanel::~LLToastGroupNotifyPanel()
{
}

void LLToastGroupNotifyPanel::close()
{
	// The group notice dialog may be an inventory offer.
	// If it has an inventory save button and that button is still enabled
	// Then we need to send the inventory declined message
	if(mInventoryOffer != NULL)
	{
		mInventoryOffer->forceResponse(IOR_DECLINE);
		mInventoryOffer = NULL;
	}

	die();
}

void LLToastGroupNotifyPanel::onClickOk()
{
	LLSD response = mNotification->getResponseTemplate();
	mNotification->respond(response);
	close();
}

void LLToastGroupNotifyPanel::onClickGroupNotices()
{
	LLGroupActions::show(mGroupID, "group_notices_tab_panel");
}

void LLToastGroupNotifyPanel::onClickAttachment()
{
	if (mInventoryOffer != NULL) {
		mInventoryOffer->forceResponse(IOR_ACCEPT);

		LLTextBox * pAttachLink = getChild<LLTextBox> ("attachment");
		static const LLUIColor textColor = LLUIColorTable::instance().getColor(
				"GroupNotifyDimmedTextColor");
		pAttachLink->setColor(textColor);

		LLIconCtrl* pAttachIcon =
				getChild<LLIconCtrl> ("attachment_icon", TRUE);
		pAttachIcon->setEnabled(FALSE);

		//if attachment isn't openable - notify about saving
		if (!isAttachmentOpenable(mInventoryOffer->mType)) {
			LLNotifications::instance().add("AttachmentSaved", LLSD(), LLSD());
		}

		mInventoryOffer = NULL;
	}
}

//static
bool LLToastGroupNotifyPanel::isAttachmentOpenable(LLAssetType::EType type)
{
	switch(type)
	{
	case LLAssetType::AT_LANDMARK:
	case LLAssetType::AT_NOTECARD:
	case LLAssetType::AT_IMAGE_JPEG:
	case LLAssetType::AT_IMAGE_TGA:
	case LLAssetType::AT_TEXTURE:
	case LLAssetType::AT_TEXTURE_TGA:
		return true;
	default:
		return false;
	}
}

// Copied from Ansariel: Override base method so we have the option to ignore
// the global transparency settings and show the group notice always on
// opaque background. -Zi
F32 LLToastGroupNotifyPanel::getCurrentTransparency()
{
	if (gSavedSettings.getBOOL("FSGroupNotifyNoTransparency"))
	{
		return 1.0;
	}
	else
	{
		return LLUICtrl::getCurrentTransparency();
	}
}
