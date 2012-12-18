/* @file lggcontactsetsfloater.cpp
   Copyright (C) 2011 LordGregGreg Back (Greg Hendrickson)

   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; version 2.1 of
   the License.
 
   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the viewer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#include "llviewerprecompiledheaders.h"
#include "lggcontactsets.h"
#include "lggcontactsetsfloater.h"

#include "llagentdata.h"
#include "llcommandhandler.h"
#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llagent.h"
#include "llpanel.h"
#include "llbutton.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llview.h"
#include "llhttpclient.h"
#include "llbufferstream.h"
#include "llcheckboxctrl.h"
#include "llviewercontrol.h"

#include "llui.h"
#include "llcontrol.h"
#include "llscrolllistctrl.h"
#include "llscrollingpanellist.h"
#include "llfilepicker.h"
#include "llfile.h"
#include "llsdserialize.h"
#include "llchat.h"
#include "llviewerinventory.h"
#include "llinventorymodel.h"
#include "llhost.h"
#include "llassetstorage.h"
#include "roles_constants.h"
#include "llviewertexteditor.h"

#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "lluuid.h"
#include "llavatarname.h"
#include "llcallingcard.h"
#include "lluserrelations.h"
#include "llimview.h"
#include "llviewermessage.h"
#include "llfloaterworldmap.h"
#include "llstring.h"
#include "llclipboard.h"
#include "llfloateravatarpicker.h"
#include "llfirstuse.h"
#include "llvoavatar.h"
#include "llfloaterreg.h"
#include "llavataractions.h"
#include "llkeyboard.h"
#include "llavatariconctrl.h"
#include "llavatarpropertiesprocessor.h"


// Ansariel: Evil, but still better than having the plain strings float around the code
const std::string ALL_SETS("All Sets");
const std::string NO_SETS("No Sets");

lggContactSetsFloater* lggContactSetsFloater::sInstance;

void lggContactSetsFloater::onClose(bool app_quitting)
{
	LLAvatarTracker::instance().removeObserver(sInstance);

	// <FS:ND> FIRE-3736; remove observers on all pending profile updates. Otherwise crash&burn when the update arrives but the floater is destroyed.
	for (std::set<LLUUID>::iterator itr = profileImagePending.begin(); profileImagePending.end() != itr; ++itr)
		LLAvatarPropertiesProcessor::getInstance()->removeObserver(*itr, this);
	// </FS:ND>

	sInstance = NULL;
	destroy();
}

BOOL lggContactSetsFloater::postBuild(void)
{
	groupsList = getChild<LLComboBox>("lgg_fg_groupCombo");
	groupsList->setCommitCallback(boost::bind(&lggContactSetsFloater::onSelectGroup, this));

	groupColorBox = getChild<LLColorSwatchCtrl>("colorswatch");

	childSetAction("lgg_fg_groupCreate", onClickNew, this);
	childSetAction("lgg_fg_groupDelete", onClickDelete, this);
	childSetAction("lgg_fg_openSettings", onClickSettings, this);

	groupColorBox->setCommitCallback(boost::bind(&lggContactSetsFloater::onBackgroundChange, this));

	noticeBox = getChild<LLCheckBoxCtrl>("lgg_fg_showNotices");
	noticeBox->setCommitCallback(boost::bind(&lggContactSetsFloater::onNoticesChange, this));

	getChild<LLCheckBoxCtrl>("lgg_fg_showOnline")->setCommitCallback(boost::bind(&lggContactSetsFloater::onCheckBoxChange, this));
	getChild<LLCheckBoxCtrl>("lgg_fg_showOffline")->setCommitCallback(boost::bind(&lggContactSetsFloater::onCheckBoxChange, this));
	getChild<LLCheckBoxCtrl>("lgg_fg_showOtherGroups")->setCommitCallback(boost::bind(&lggContactSetsFloater::onCheckBoxChange, this));
	getChild<LLCheckBoxCtrl>("lgg_fg_showAllFriends")->setCommitCallback(boost::bind(&lggContactSetsFloater::onCheckBoxChange, this));

	getChild<LLCheckBoxCtrl>("haxCheckbox")->setCommitCallback(boost::bind(&lggContactSetsFloater::hitSpaceBar, this));

	updateGroupsList();
	generateCurrentList();
	updateGroupGUIs();

	LLFirstUse::usePhoenixContactSet();

	return true;
}


lggContactSetsFloater::~lggContactSetsFloater()
{
}

lggContactSetsFloater::lggContactSetsFloater(const LLSD& seed)
	:LLFloater(seed),
	mouse_x(0),
	mouse_y(900),
	hovered(0.f),
	justClicked(FALSE),
	scrollLoc(0),
	showRightClick(FALSE),
	maxSize(0),
	scrollStarted(0),
	currentFilter(""),
	currentRightClickText(""),
	mouseInWindow(FALSE)
{
	if (sInstance)
	{
		delete sInstance;
	}
	sInstance = this;
	selected.clear();
	currentList.clear();
	allFolders.clear();
	openedFolders.clear();
	profileImagePending.clear();
	LLAvatarTracker::instance().addObserver(this);
	
	if (getRect().mLeft == 0 && getRect().mBottom == 0)
	{
		center();
	}
}

//virtual
void lggContactSetsFloater::processProperties(void* data, EAvatarProcessorType type)
{
	if (APT_PROPERTIES == type)
	{
		LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
		if (avatar_data)
		{
			LLAvatarIconIDCache::getInstance()->add(avatar_data->avatar_id, avatar_data->image_id);
			LLAvatarPropertiesProcessor::getInstance()->removeObserver(avatar_data->avatar_id, this);
			profileImagePending.erase(avatar_data->avatar_id);
		}
	}
}

void lggContactSetsFloater::changed(U32 mask)
{
	if (mask & LLFriendObserver::REMOVE)
	{
		const std::set<LLUUID>& changed_items = LLAvatarTracker::instance().getChangedIDs();
		std::set<LLUUID>::const_iterator id_it = changed_items.begin();
		std::set<LLUUID>::const_iterator id_end = changed_items.end();
		for (; id_it != id_end; ++id_it)
		{
			//if we are fortunate enough to be the viewer of choice when
			//someone removes a friend, go and clean up the contact sets
			LGGContactSets::getInstance()->removeFriendFromAllGroups(*id_it);
		}		
	}

	if (mask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE))
	{
		sInstance->generateCurrentList();
	}

	if (mask & LLFriendObserver::ONLINE)
	{
		static LLCachedControl<bool> showOnline(gSavedSettings, "FSContactSetsShowOnline");
		static LLCachedControl<bool> showOffline(gSavedSettings, "FSContactSetsShowOffline");
		
		if (!showOffline && showOnline)
		{
			sInstance->generateCurrentList();
		}
	}
}

void lggContactSetsFloater::onBackgroundChange()
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	LGGContactSets::getInstance()->setGroupColor(currentGroup, sInstance->groupColorBox->get());
}

void lggContactSetsFloater::onNoticesChange()
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	LGGContactSets::getInstance()->setNotifyForGroup(currentGroup, sInstance->noticeBox->getValue().asBoolean());
}

void lggContactSetsFloater::onCheckBoxChange()
{
	sInstance->generateCurrentList();	
}

void lggContactSetsFloater::onPickAvatar(const std::vector<LLUUID>& ids,
								  const std::vector<LLAvatarName> names)
{
	if (names.empty() || ids.empty())
	{
		return;
	}

	for (int i = 0; i < (int)ids.size(); i++)
	{
		LGGContactSets::getInstance()->addNonFriendToList(ids[i]);
	}

	sInstance->updateGroupsList();
	LLFirstUse::usePhoenixFriendsNonFriend();
}

void lggContactSetsFloater::updateGroupsList()
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	LLComboBox* cb = groupsList;
	cb->clear();
	cb->removeall();

	std::vector<std::string> groups = LGGContactSets::getInstance()->getAllGroups();
	for (int i = 0; i < (int)groups.size(); i++)
	{
		cb->add(groups[i], groups[i], ADD_BOTTOM, TRUE);
	}

	if (LGGContactSets::getInstance()->isAGroup(currentGroup))
	{
		cb->setSimple(LLStringExplicit(currentGroup));
	}
	else if (groups.size() > 0)
	{
		gSavedSettings.setString("FSContactSetsSelectedGroup", groups[0]);
		cb->setSimple(groups[0]);
		updateGroupGUIs();
		generateCurrentList();
	}
	else
	{
		gSavedSettings.setString("FSContactSetsSelectedGroup", "");
	}
}

void lggContactSetsFloater::hitSpaceBar()
{
	if ((sInstance->currentFilter == "" && !sInstance->showRightClick) ||
		(sInstance->currentRightClickText == "" && sInstance->showRightClick))
	{
		sInstance->justClicked=TRUE;
	}
	else
	{
		if (!sInstance->showRightClick)
		{
			sInstance->currentFilter += ' ';
			sInstance->generateCurrentList();
		}
		else
		{
			sInstance->currentRightClickText += ' ';
		}
	}
}

void lggContactSetsFloater::updateGroupGUIs()
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	groupColorBox->set(LGGContactSets::getInstance()->getGroupColor(currentGroup), TRUE);	
	groupsList->setSimple(currentGroup());
	noticeBox->set(LGGContactSets::getInstance()->getNotifyForGroup(currentGroup));
}

void lggContactSetsFloater::onSelectGroup()
{
	gSavedSettings.setString("FSContactSetsSelectedGroup", sInstance->groupsList->getSimple());
	sInstance->updateGroupGUIs();
	sInstance->selected.clear();
	sInstance->generateCurrentList();
}

void lggContactSetsFloater::drawScrollBars()
{
}

void lggContactSetsFloater::drawRightClick()
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	
	if (!sInstance->hasFocus())
	{
		showRightClick = FALSE;
		return;
	}
	int heightPer = 17;
	int width = 208;
	BOOL drawRemove = FALSE;

	int extras = 5; //make sure we have room for the extra options
	BOOL canMap = FALSE;
	int isNonFriend = 0;
	
	if (selected.size() == 1)
	{
		extras += 6; //space,name,tp, profile, im, rename 

		//map
		if (LGGContactSets::getInstance()->isNonFriend(selected[0]))
		{
			isNonFriend = 1;
			extras += 1; //add remove option

		}
		else if (LLAvatarTracker::instance().getBuddyInfo(selected[0])->getRightsGrantedFrom() & LLRelationship::GRANT_MAP_LOCATION)
		{
			if (LLAvatarTracker::instance().getBuddyInfo(selected[0])->isOnline())
			{
				extras += 1;
				canMap = TRUE;
			}
		}

		if (LGGContactSets::getInstance()->hasPseudonym(selected[0]))
		{
			extras += 1; //for clearing it
		}
	}

	if (selected.size() > 1)
	{
		extras += 4; //name, conference, mass tp, space
		for (int i = 0; i < (int)selected.size(); i++)
		{
			if (LGGContactSets::getInstance()->isNonFriend(selected[i]))
			{
				isNonFriend++;
			}
		}
	}

	if (currentGroup() != ALL_SETS && currentGroup() != NO_SETS && currentGroup() != "")
	{
		drawRemove = TRUE;
		extras += 2;
	}
	
	std::vector<std::string> groups = LGGContactSets::getInstance()->getAllGroups(FALSE);
	if (selected.size() == 0)
	{
		groups.clear();
		extras += 4;
	}
	int height = heightPer * (extras + groups.size());

	LLRect rec = sInstance->getChild<LLPanel>("draw_region")->getRect();
	gGL.color4fv(LLColor4(0, 0, 0, 0.5).mV);
	gl_rect_2d(rec);
	
	if ((contextRect.mLeft + width) > rec.mRight)
	{
		contextRect.mLeft = rec.mRight - width;
	}
	if ((contextRect.mTop - height) < rec.mBottom)
	{
		contextRect.mTop = rec.mBottom + height;
	}

	// Add button
	contextRect.setLeftTopAndSize(contextRect.mLeft, contextRect.mTop, width, height);
	LLRect bgRect;
	bgRect.setLeftTopAndSize(contextRect.mLeft - 2, contextRect.mTop + 2, width + 4, contextRect.getHeight() + 4);
	gGL.color4fv(LLColor4::black.mV);
	gl_rect_2d(bgRect);
	
	int top = contextRect.mTop;
	for (int i = 0; i < (int)groups.size(); i++)
	{
		LLRect addBackGround;
		addBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);		
		
		gGL.color4fv(LGGContactSets::getInstance()->getGroupColor(groups[i]).mV);
		gl_rect_2d(addBackGround);
		if (addBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(addBackGround, FALSE);

			if (justClicked)
			{
				for (int v = 0; v < (int)selected.size(); v++)
				{
					LLUUID afriend = selected[v];
					LGGContactSets::getInstance()->addFriendToGroup(afriend, groups[i]);
				}
				selected.clear();
			}
		}

		LLStringUtil::format_map_t args;
		args["[GROUPNAME]"] = groups[i];

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("AddToGroup", args)),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE, 
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);

		top -= heightPer;
	}

	// Remove button
	LLRect remBackGround;
	if (drawRemove)
	{
		//draw remove button
		top -= heightPer;
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);

		gGL.color4fv(LGGContactSets::getInstance()->getGroupColor(currentGroup).mV);
		gl_rect_2d(remBackGround);
		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);

			if (justClicked)
			{
				for (int v = 0; v < (int)selected.size(); v++)
				{
					LLUUID afriend = selected[v];
					LGGContactSets::getInstance()->removeFriendFromGroup(afriend, currentGroup);

					sInstance->generateCurrentList();
				}

				selected.clear();
			}
		}

		LLStringUtil::format_map_t args;
		args["[GROUPNAME]"] = currentGroup();

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("RemoveFromGroup", args)),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);

		top -= heightPer;
	}

	top -= heightPer;

	// Special options if selected a single avatar
	if (selected.size() == 1)
	{
		// Draw avatar name
		std::string avName("");
		LLAvatarName avatar_name;
		if (LLAvatarNameCache::get(selected[0], &avatar_name))
		{
			avName=avatar_name.getLegacyName();
		}

		LLColor4 friendColor = LGGContactSets::getInstance()->getFriendColor(selected[0], "");

		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			//gGL.color4fv(LLColor4::yellow.mV);
			//gl_rect_2d(remBackGround, FALSE);
			if (justClicked)
			{
				//no
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(avName),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::BOLD, //style
			LLFontGL::DROP_SHADOW //shadow
			);
		
		top -= heightPer;
		

		// Rename
		/* Rename code disabled till I add the single quotes.. 
		probably wont till there is a better display name system
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		//draw text block background after the :rename text
		int remWidth = LLFontGL::getFontSansSerif()->getWidth("Set Alias:");
		LLRect inTextBox;
		inTextBox.setLeftTopAndSize(remBackGround.mLeft + 2 + remWidth, remBackGround.mTop,
			remBackGround.getWidth() - 2 - remWidth, remBackGround.getHeight() - 2);
		gGL.color4fv(LLColor4::white.mV);
		gl_rect_2d(inTextBox);

		//draw text in black of rightclicktext
		//if nothing set, give hints
		std::string textToDrawInRightClickBox = sInstance->currentRightClickText;
		LLColor4 textColor = LLColor4::black;
		if (textToDrawInRightClickBox == "")
		{
			textToDrawInRightClickBox="Start Typing, then click here";
			textColor=LLColor4::grey;
		}

		LLFontGL::getFontSansSerif()->renderUTF8(
			textToDrawInRightClickBox,
			0,
			inTextBox.mLeft,
			inTextBox.mBottom + 4,
			textColor,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::DROP_SHADOW
		);
		*/


		// Remove display name
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);
			if (justClicked)
			{
				//rename avatar (or remove display name)
				if (TRUE) //sInstance->currentRightClickText != "")
				{
					//LGGContactSets::getInstance()->setPseudonym(selected[0], sInstance->currentRightClickText);
					LGGContactSets::getInstance()->removeDisplayName(selected[0]);
					sInstance->updateGroupsList();
					LLFirstUse::usePhoenixContactSetRename();
					LLVOAvatar::invalidateNameTag(selected[0]);
				}
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("RemoveDisplayName")),
			0,
			contextRect.mLeft,
			top - (heightPer / 2 ) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);
		
		top -= heightPer;


		// Remove pseudonym
		if (LGGContactSets::getInstance()->hasPseudonym(selected[0]))
		{
			remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
			if (remBackGround.pointInRect(mouse_x, mouse_y))
			{
				//draw hover effect
				gGL.color4fv(LLColor4::yellow.mV);
				gl_rect_2d(remBackGround, FALSE);

				if(justClicked)
				{
					//cler avs rename
					LGGContactSets::getInstance()->clearPseudonym(selected[0]);
					LLVOAvatar::invalidateNameTag(selected[0]);
					sInstance->generateCurrentList();
					sInstance->updateGroupsList();
				}
			}

			LLFontGL::getFontSansSerif()->render(
				utf8str_to_wstring(getString("ResetNameModification")),
				0,
				contextRect.mLeft,
				top - (heightPer / 2) - 2,
				LLColor4::white,
				LLFontGL::LEFT,
				LLFontGL::BASELINE,
				LLFontGL::NORMAL, //style
				LLFontGL::DROP_SHADOW //shadow
				);

			top -= heightPer;
		}


		// Remove non-friend from contact set
		if (isNonFriend)
		{
			remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
			if (remBackGround.pointInRect(mouse_x, mouse_y))
			{
				//draw hover effect
				gGL.color4fv(LLColor4::yellow.mV);
				gl_rect_2d(remBackGround, FALSE);
				if (justClicked)
				{
					//cler avs rename
					LGGContactSets::getInstance()->removeNonFriendFromList(selected[0]);
					sInstance->generateCurrentList();
				}
			}

			LLFontGL::getFontSansSerif()->render(
				utf8str_to_wstring(getString("RemoveFromList")),
				0,
				contextRect.mLeft,
				top - (heightPer / 2) - 2,
				LLColor4::white,
				LLFontGL::LEFT,
				LLFontGL::BASELINE,
				LLFontGL::NORMAL, //style
				LLFontGL::DROP_SHADOW //shadow
				);

			top -= heightPer;
		}


		// View profile
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		gGL.color4fv(friendColor.mV);
		gl_rect_2d(remBackGround);

		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);
			if (justClicked)
			{
				//profileclick
				LLAvatarActions::showProfile(selected[0]);
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("ShowProfile")),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);
		
		top -= heightPer;


		// Show on map
		if (canMap)
		{
			remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
			gGL.color4fv(friendColor.mV);
			gl_rect_2d(remBackGround);

			if (remBackGround.pointInRect(mouse_x, mouse_y))
			{
				//draw hover effect
				gGL.color4fv(LLColor4::yellow.mV);
				gl_rect_2d(remBackGround, FALSE);
				
				if(justClicked)
				{
					//mapclick
					if (gFloaterWorldMap)
					{
						LLAvatarActions::showOnMap(selected[0]);
						//gFloaterWorldMap->trackAvatar(selected[0], avName);
						//LLFloaterWorldMap::show(NULL, TRUE);
					}
				}
			}

			LLFontGL::getFontSansSerif()->render(
				utf8str_to_wstring(getString("ShowOnMap")),
				0,
				contextRect.mLeft,
				top - (heightPer / 2) - 2,
				LLColor4::white,
				LLFontGL::LEFT,
				LLFontGL::BASELINE,
				LLFontGL::NORMAL, //style
				LLFontGL::DROP_SHADOW //shadow
				);
			
			top -= heightPer;
		}


		// Send IM
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		gGL.color4fv(friendColor.mV);
		gl_rect_2d(remBackGround);

		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);
			if (justClicked)
			{
				//im avatar
				LLAvatarActions::startIM(selected[0]);
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("SendIM")),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);

		top -= heightPer;


		// Offer TP
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		gGL.color4fv(friendColor.mV);
		gl_rect_2d(remBackGround);

		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);
			if (justClicked)
			{
				//offer tp click
				handle_lure(selected[0]);
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("OfferTP")),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);		
	}


	// Special options for groups of avatars
	if (selected.size() > 1)
	{
		LLColor4 groupColor = LGGContactSets::getInstance()->getGroupColor(currentGroup);
		LLDynamicArray<LLUUID> ids;

		for (int se = 0; se < (int)selected.size(); se++)
		{
			LLUUID avid= selected[se];
			if (!LGGContactSets::getInstance()->isNonFriend(avid)) //don't mass tp or confrence non friends
			{
				ids.push_back(avid);
			}
		}

		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);

			if (justClicked)
			{
				//no
			}
		}

		LLStringUtil::format_map_t args;
		args["[COUNT]"] = llformat("%d", ids.size());

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("AllSelected", args)),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);		
		
		top -= heightPer;


		// Start conference call
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		gGL.color4fv(groupColor.mV);
		gl_rect_2d(remBackGround);

		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);
			
			if (justClicked)
			{
				//confrence	
				LLAvatarActions::startConference(ids);
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("StartConferenceCall", args)),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);

		top -= heightPer;


		// Mass-TP (friends only!)
		remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
		gGL.color4fv(groupColor.mV);
		gl_rect_2d(remBackGround);

		if (remBackGround.pointInRect(mouse_x, mouse_y))
		{
			//draw hover effect
			gGL.color4fv(LLColor4::yellow.mV);
			gl_rect_2d(remBackGround, FALSE);

			if (justClicked)
			{
				//mass tp
				handle_lure(ids);
			}
		}

		LLFontGL::getFontSansSerif()->render(
			utf8str_to_wstring(getString("SendMassTP", args)),
			0,
			contextRect.mLeft,
			top - (heightPer / 2) - 2,
			LLColor4::white,
			LLFontGL::LEFT,
			LLFontGL::BASELINE,
			LLFontGL::NORMAL, //style
			LLFontGL::DROP_SHADOW //shadow
			);
	}

	top -= heightPer;
	top -= heightPer;


	// Deselect option
	remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
	if (remBackGround.pointInRect(mouse_x, mouse_y))
	{
		//draw hover effect
		gGL.color4fv(LLColor4::yellow.mV);
		gl_rect_2d(remBackGround, FALSE);

		if (justClicked)
		{
			selected.clear();
		}
	}

	LLStringUtil::format_map_t deselect_args;
	deselect_args["[COUNT]"] = llformat("%d", selected.size());

	LLFontGL::getFontSansSerif()->render(
		utf8str_to_wstring(getString("DeselectAll", deselect_args)),
		0,
		contextRect.mLeft,
		top - (heightPer / 2) - 2,
		LLColor4::white,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::NORMAL, //style
		LLFontGL::DROP_SHADOW //shadow
		);

	top -= heightPer;


	// Select all
	remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
	if (remBackGround.pointInRect(mouse_x, mouse_y))
	{
		//draw hover effect
		gGL.color4fv(LLColor4::yellow.mV);
		gl_rect_2d(remBackGround, FALSE);
		if (justClicked)
		{
			/*std::vector<LLUUID> newSelected;
			newSelected.clear();
			for (int pp = 0; pp < currentList.size(); pp++) // don't use snapshot, get anything new
			{
				newSelected.push_back(currentList[pp]);
			}*/
			sInstance->selected = sInstance->currentList;
		}
	}

	LLStringUtil::format_map_t select_args;
	select_args["[COUNT]"] = llformat("%d", currentList.size() - selected.size());

	LLFontGL::getFontSansSerif()->render(
		utf8str_to_wstring(getString("SelectAll", select_args)),
		0,
		contextRect.mLeft,
		top - (heightPer / 2) - 2,
		LLColor4::white,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::NORMAL, //style
		LLFontGL::DROP_SHADOW //shadow
		);

	top -= heightPer;
	top -= heightPer;


	// Add new avatar
	remBackGround.setLeftTopAndSize(contextRect.mLeft, top, width, heightPer);
	if (remBackGround.pointInRect(mouse_x, mouse_y))
	{
		//draw hover effect
		gGL.color4fv(LLColor4::yellow.mV);
		gl_rect_2d(remBackGround, FALSE);

		if (justClicked)
		{
			//add new av
			LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&lggContactSetsFloater::onPickAvatar, _1, _2), TRUE, TRUE);
			sInstance->addDependentFloater(picker);
		}
	}

	LLFontGL::getFontSansSerif()->render(
		utf8str_to_wstring(getString("AddNewAvatar")),
		0,
		contextRect.mLeft,
		top - (heightPer / 2) - 2,
		LLColor4::white,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::NORMAL, //style
		LLFontGL::DROP_SHADOW
		//shadow
		);

	if (justClicked)
	{
		showRightClick = FALSE;
		if (selected.size() == 1)
		{
		selected.clear();
		}
	}

	justClicked = FALSE;	
}

void lggContactSetsFloater::drawFilter()
{
	if (sInstance->currentFilter == "")
	{
		return;
	}

	int mySize = 40;
	LLRect rec = sInstance->getChild<LLPanel>("top_region")->getRect();

	LLRect aboveThisMess;
	aboveThisMess.setLeftTopAndSize(rec.mLeft, rec.mTop + mySize, rec.getWidth(), mySize);

	LLColor4 backGround(0, 0, 0, 1.0f);
	LLColor4 foreGround(1, 1, 1, 1.0f);

	if (aboveThisMess.pointInRect(sInstance->mouse_x, sInstance->mouse_y))
	{
		backGround = LLColor4(0, 0, 0, 0.4f);
		foreGround = LLColor4(1, 1, 1, 0.4f);
		gGL.color4fv(LLColor4(0, 0, 0, 0.2f).mV); //for main bg
	}
	else
	{
		gGL.color4fv(LLColor4(0, 0, 0, 0.8f).mV);
	}

	gl_rect_2d(aboveThisMess);
	std::string preText = getString("FilterLabel") + " ";

	int width1 = LLFontGL::getFontSansSerif()->getWidth(preText) + 8;
	int width2 = LLFontGL::getFontSansSerif()->getWidth(sInstance->currentFilter) + 8;
	int tSize = 24;

	LLRect fullTextBox;
	fullTextBox.setLeftTopAndSize(aboveThisMess.mLeft + 20, aboveThisMess.getCenterY() + (tSize / 2), width1 + width2, tSize);
	gGL.color4fv(backGround.mV);
	gl_rect_2d(fullTextBox);
	gGL.color4fv(foreGround.mV);
	gl_rect_2d(fullTextBox, FALSE);
	LLRect filterTextBox;
	filterTextBox.setLeftTopAndSize(fullTextBox.mLeft + width1, fullTextBox.mTop, width2, tSize);
	gl_rect_2d(filterTextBox);	
	
	LLFontGL::getFontSansSerif()->render(
		utf8str_to_wstring(preText),
		0,
		fullTextBox.mLeft + 4,
		fullTextBox.mBottom + 4,
		foreGround,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::NORMAL, //style
		LLFontGL::DROP_SHADOW //shadow
		);

	LLFontGL::getFontSansSerif()->render(
		utf8str_to_wstring(sInstance->currentFilter),
		0,
		filterTextBox.mLeft + 4,
		filterTextBox.mBottom + 4,
		backGround,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::NORMAL, //style
		LLFontGL::DROP_SHADOW //shadow
		);
}

void lggContactSetsFloater::draw()
{
	LLFloater::draw();
	if (sInstance->isMinimized())
	{
		return;
	}

	LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	LLFontGL* bigFont = LLFontGL::getFontSansSerifBig();
	LLFontGL* hugeFont = LLFontGL::getFontSansSerifHuge();

	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");
	static LLCachedControl<bool> textNotBg(gSavedSettings, "FSContactSetsColorizeText");
	static LLCachedControl<bool> barNotBg(gSavedSettings, "FSContactSetsColorizeBar");
	static LLCachedControl<bool> requireCTRL(gSavedSettings, "FSContactSetsRequireCTRL");
	static LLCachedControl<bool> doZoom(gSavedSettings, "FSContactSetsDoZoom");
	static LLCachedControl<bool> doColorChange(gSavedSettings, "FSContactSetsUseColorHighlight");
	static LLCachedControl<bool> drawProfileIcon(gSavedSettings, "FSContactSetsDrawProfileIcon");
	static LLCachedControl<bool> showOtherGroups(gSavedSettings, "FSContactSetsShowOtherGroups");

	int roomForBar = 0; // used to move text and icon over away from the little bar on the left
	if (barNotBg || textNotBg)
	{
		roomForBar = 10 + 2;
	}

	std::vector<LLUUID> workingList;
	workingList = currentList;
	int numberOfPanels = workingList.size(); //45;

	// see if we are guna draw some folders
	allFolders = LGGContactSets::getInstance()->getInnerGroups(currentGroup);
	numberOfPanels += allFolders.size();

	LLRect topScroll = getChild<LLPanel>("top_region")->getRect();
	LLRect bottomScroll = getChild<LLPanel>("bottom_region")->getRect();
	LLPanel* mainPanel = getChild<LLPanel>("draw_region");
	LLRect rec = mainPanel->getRect();

	if (rec.pointInRect(mouse_x, mouse_y) && sInstance->hasFocus())
	{
		sInstance->getChild<LLCheckBoxCtrl>("haxCheckbox")->setFocus(TRUE);
	}

	gGL.pushMatrix();
	int bMag = 35;

	if (!doZoom)
	{
		bMag = 0;
	}

	// kinda magic numbers to compensate for max bloom effect and stuff
	float sizeV = (F32)((rec.getHeight() - 143) - (((F32)numberOfPanels) * 1.07f) - 0) / (F32)(numberOfPanels);
	if (!doZoom)
	{
		sizeV = (F32)((rec.getHeight() - 0 - (numberOfPanels * 2))) / (F32)(numberOfPanels);
	}
	
	maxSize=sizeV + bMag;
	int minSize = 10;

	if (!doZoom)
	{
		minSize = 24;
	}

	if (sizeV < minSize)
	{
		//need scroll bars
		sizeV = minSize;
//#pragma region ScrollBars

		if (this->hasFocus() && mouseInWindow)
		{
			LLUIImage* arrowUpImage = LLUI::getUIImage("map_avatar_above_32.tga");
			LLUIImage* arrowDownImage = LLUI::getUIImage("map_avatar_below_32.tga");
			LLColor4 active = LGGContactSets::getInstance()->getGroupColor(currentGroup);
			LLColor4 unactive = LGGContactSets::toneDownColor(active, 0.5);
			static LLCachedControl<S32> scrollSpeedSetting(gSavedSettings, "FSContactSetsScrollSpeed");
			float speedFraction = ((F32)(scrollSpeedSetting)) / 100.0f;

			LLColor4 useColor = unactive;
			if (topScroll.pointInRect(mouse_x, mouse_y))
			{
				useColor = active;
				scrollLoc -= llclamp((S32)((((F32)numberOfPanels) / 4.0f) * speedFraction), 1, 100);
			}

			if (scrollLoc > 0)
			{
				gGL.color4fv(useColor.mV);
				gl_rect_2d(topScroll, true);
			}		
			
			int x = topScroll.mLeft;
			if (scrollLoc > 0)
			{
				for (; x < topScroll.mRight - topScroll.getHeight(); x += (30 + topScroll.getHeight()))
				{
					gl_draw_scaled_image_with_border(
						x,
						topScroll.mBottom,
						topScroll.getHeight(),
						topScroll.getHeight(),
						arrowUpImage->getImage(),
						useColor,
						FALSE); 
				}
			}

			int maxS = (numberOfPanels * 11) + 200 - rec.getHeight();
			if (!doZoom)
			{
				maxS = (numberOfPanels * (minSize + 2)) + 10 - rec.getHeight();
			}

			useColor = unactive;
			if (bottomScroll.pointInRect(mouse_x, mouse_y))
			{
				useColor = active;
				scrollLoc += llclamp((S32)((((F32)numberOfPanels) / 4.0f) * speedFraction), 1, 100);
			}

			if (scrollLoc < maxS)
			{
				gGL.color4fv(useColor.mV);
				gl_rect_2d(bottomScroll, true);

				for (x = bottomScroll.mLeft; x < bottomScroll.mRight - bottomScroll.getHeight(); x += (30 + bottomScroll.getHeight()))
				{
					gl_draw_scaled_image_with_border(
						x,
						bottomScroll.mBottom,
						bottomScroll.getHeight(),
						topScroll.getHeight(),
						arrowDownImage->getImage(),
						useColor,
						FALSE
					); 
				}
			}
			scrollLoc = llclamp(scrollLoc, 0, maxS);
		}
//#pragma endregion ScrollBars

	}
	else
	{
		scrollLoc = 0;
	}


	float top = rec.mTop + scrollLoc; // sizeV + 12;
	//if (mouse_y < 15)
	//{
	//	mouse_y = 15;
	//}
	
	for (int f = 0; f < (int)allFolders.size(); f++)
	{
		float thisSize = sizeV;
		float pi = 3.1415f;
		float piOver2Centered = pi / 2 + ( (top - ((F32)(sizeV + 40) / 2.0f) - mouse_y) * 0.01);
		float bubble = sin((float)llclamp(piOver2Centered, 0.0f, pi)); // * bMag;
		thisSize += (bubble * bMag);

		if ((top - thisSize) < rec.mBottom)
		{
			continue; 
		}

		if ((top - thisSize) > rec.mTop) {}
		else
		{
			//draw folder stuff
			if (top > rec.mTop)
			{
				//draw as much as the top one as we can
				top = rec.mTop;
			}

			LLRect box;
			box.setLeftTopAndSize(rec.mLeft + (bMag / 2) + 5 - ((bubble * bMag) / 2), llceil(top + 0.00001), (rec.getWidth() - bMag - 10) + ((bubble * bMag) / 1), (int)llfloor(thisSize + 0.00001f));

			std::string folder = allFolders[f];
			LLColor4 color = LGGContactSets::getInstance()->getGroupColor(folder);
			
			color = LGGContactSets::toneDownColor(color,
				doColorChange ? ((F32)bubble + 0.001) / (1.0f) * 1.0f : 1.0f, TRUE);

			gGL.color4fv(color.mV);			
			if (!barNotBg && !textNotBg)
			{
				gl_rect_2d(box);
			}
			else
			{
				LLRect smallBox = box;
				smallBox.setLeftTopAndSize(box.mLeft, box.mTop, 10 + (bubble / 2), box.getHeight());
				gl_rect_2d(smallBox);
				smallBox.setLeftTopAndSize(box.mLeft + 10 + (bubble / 2), box.mTop, box.getWidth() - (10 + (bubble / 2)), box.getHeight());
				gGL.color4fv(LGGContactSets::toneDownColor(LGGContactSets::getInstance()->getDefaultColor(), doColorChange ? (((F32)bubble) / (1) * 1.0f) : 1.0f, TRUE).mV);
				gl_rect_2d(smallBox);
			}

			if (box.pointInRect(mouse_x, mouse_y))
			{
				gGL.color4fv(LLColor4::white.mV);
				gl_rect_2d(box,FALSE);
				if (justClicked && !showRightClick)
				{
					justClicked = FALSE;
					gSavedSettings.setString("FSContactSetsSelectedGroup", folder);
					sInstance->updateGroupGUIs();
					sInstance->selected.clear();
					sInstance->generateCurrentList();
				}
			}

			LLFontGL* useFont = font;
			if (thisSize > 25)
			{
				useFont = bigFont;
			}
			if (thisSize > 46)
			{
				useFont = hugeFont;
			}
			if (doZoom)
			{
				if (thisSize > 14)
				{
					useFont = bigFont;
				}
				if (thisSize > 35)
				{
					useFont = hugeFont;
				}
			}

			int size = llclamp(thisSize + (bubble * bMag / 2),
				llmax(10.0f, llmin((((F32)box.getHeight()) / 1.0f), 20.0f)),
				llmin(20 + (bubble * bMag / 2), thisSize + (bubble * bMag / 2)));

			int xLoc = box.mLeft + roomForBar; //size;
			LLUIImage* selectedImage = LLUI::getUIImage("TabIcon_Close_Off");
			if (folder == ALL_SETS)
			{
				selectedImage = LLUI::getUIImage("TabIcon_Open_Off");
			}

			LLRect imageBox;
			imageBox.setLeftTopAndSize(xLoc, (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
			gl_draw_scaled_image_with_border(
				imageBox.mLeft,
				imageBox.mBottom,
				imageBox.getWidth(),
				imageBox.getHeight(),
				selectedImage->getImage(),
				LLColor4::white,
				FALSE
			);

			LLColor4 groupTextColor = LLColor4::white;
			if (textNotBg)
			{
				groupTextColor = LGGContactSets::toneDownColor(color, 1.0f);
			}

			useFont->render(
				utf8str_to_wstring(folder),
				0,
				box.mLeft + roomForBar + size + 2,
				top - (thisSize / 2) + (doZoom ? -2 : 2),
				groupTextColor,
				LLFontGL::LEFT,
				LLFontGL::BASELINE,
				LLFontGL::NORMAL, //style
				LLFontGL::DROP_SHADOW //shadow
			);
		}

		top -= (thisSize + 1);
	}
	
	for (int p = 0; p < (numberOfPanels - (int)allFolders.size()); p++)
	{
		float thisSize = sizeV;
		float pi = 3.1415f;
		float piOver2Centered = pi / 2 + ((top - ((F32)(sizeV + 40) / 2.0f) - mouse_y) * 0.01);
		float bubble = sin((float)llclamp(piOver2Centered, 0.0f, pi)); // * bMag;
		thisSize += (bubble * bMag);

		if ((top - thisSize) < rec.mBottom)
		{
			continue;
		}

		if ((top - thisSize) > rec.mTop) {}
		else
		{
//#pragma region DrawListItem
			if (top > rec.mTop)
			{
				//draw as much as the top one as we can
				top = rec.mTop;
			}

			LLRect box;
			box.setLeftTopAndSize(rec.mLeft + (bMag / 2) + 5 - ((bubble * bMag) / 2), llceil(top + 0.00001), (rec.getWidth() - bMag - 10) + ((bubble * bMag) / 1), (int)llfloor(thisSize + 0.00001f));

			BOOL hoveringThis = FALSE;
			if (top > mouse_y && (top - thisSize) < mouse_y)
			{
				hoveringThis = TRUE;
			}

			BOOL iAMSelected = FALSE;
			for (int i = 0; i < (int)selected.size(); i++)
			{	
				if (selected[i] == workingList[p])
				{
					iAMSelected = TRUE;
				}
			}

			LLUUID agent_id = workingList[p];

			std::vector<std::string> groupsIsIn;				
			groupsIsIn = LGGContactSets::getInstance()->getFriendGroups(agent_id);

			LLColor4 color = LGGContactSets::getInstance()->getGroupColor(currentGroup);
			if (!LGGContactSets::getInstance()->isFriendInGroup(agent_id,currentGroup))
			{
				color = LGGContactSets::getInstance()->getDefaultColor();
			}
			if (showOtherGroups || (currentGroup() == ALL_SETS))
			{
				color = LGGContactSets::getInstance()->getFriendColor(agent_id,currentGroup);
			}
			
			color = LGGContactSets::toneDownColor(color,
				(!iAMSelected && doColorChange) ?
				((F32)bubble + 0.001) / (1.0f) * 1.0f : 1.0f, TRUE);
			
			gGL.color4fv(color.mV);			
			if (!barNotBg && !textNotBg)
			{
				gl_rect_2d(box);
			}
			else
			{
				LLRect smallBox = box;
				smallBox.setLeftTopAndSize(box.mLeft, box.mTop, 10 + (bubble / 2), box.getHeight());
				gl_rect_2d(smallBox);
				smallBox.setLeftTopAndSize(box.mLeft + 10 + (bubble / 2), box.mTop, box.getWidth() - (10 + (bubble / 2)), box.getHeight());
				gGL.color4fv(LGGContactSets::toneDownColor(LGGContactSets::getInstance()->getDefaultColor(), doColorChange ? (((F32)bubble) / (1) * 1.0f) : 1.0f, TRUE).mV);
				gl_rect_2d(smallBox);
			}

			int roomForIcon = 0;
			//draw av icon?
			if (drawProfileIcon) 
			{
				S32 profileImageSize = llclamp(box.getHeight(), 1, 120);

				roomForIcon = profileImageSize + 4;
				LLRect profileImageBox;
				profileImageBox.setLeftTopAndSize(box.mLeft + roomForBar + (bubble / 2) + 2,
					box.mTop, profileImageSize, box.getHeight());
				if (profileImageBox.pointInRect(mouse_x, mouse_y))
				{
					profileImageSize = llclamp(profileImageSize, (S32)(bubble * box.getHeight()), box.getHeight());
				}

				LLUUID* icon_id_ptr = LLAvatarIconIDCache::getInstance()->get(agent_id);
				if (icon_id_ptr)
				{
					const LLUUID& icon_id = *icon_id_ptr;
					// Update the avatar
					if (icon_id.notNull())
					{
						LLUIImage *avatarProfileImage =	LLUI::getUIImageByID(icon_id, LLViewerFetchedTexture::BOOST_ICON);
						gl_draw_scaled_image_with_border(
							profileImageBox.mLeft,
							profileImageBox.getCenterY() - (profileImageSize / 2),
							profileImageSize,
							profileImageSize,
							avatarProfileImage->getImage(), 
							LLColor4::white,
							FALSE
						);
					}
				}
				else if (profileImagePending.find(agent_id) == profileImagePending.end())
				{
					LLAvatarPropertiesProcessor* app =
						LLAvatarPropertiesProcessor::getInstance();					
					app->removeObserver(agent_id, this);
					app->addObserver(agent_id, this);
					app->sendAvatarPropertiesRequest(agent_id);
					profileImagePending.insert(agent_id);
				}
			}

			//draw over lays (other group names)
			if (box.getHeight() > (doZoom ? 25 : 0))
			{
				int breathingRoom = 0;
				if (box.getHeight() > 35)
				{
					breathingRoom = 4;
				}

				//move it away from the image.. but not all the way
				int w = box.mLeft + breathingRoom + roomForBar +
					(S32)(roomForIcon / (1.0f)) //make room for the icon
					-(doZoom ? ((S32)(roomForIcon * bubble)) : 0); //if we got allot of room, move it back

				int sizePerOGroup = 40;
				for (int gr = 0; gr < (int)groupsIsIn.size(); gr++)
				{
					std::string oGroupName = groupsIsIn[gr];
					sizePerOGroup = LLFontGL::getFontSansSerifSmall()->getWidth(oGroupName) + 8;

					LLColor4 oGroupColor = LGGContactSets::toneDownColor(LGGContactSets::getInstance()->getGroupColor(oGroupName), 1.0f, TRUE);
					LLRect oGroupArea;
					oGroupArea.setLeftTopAndSize(w, box.mBottom + 12 + breathingRoom, sizePerOGroup, 12 + (breathingRoom / 2));
					gGL.color4fv(oGroupColor.mV);			
					gl_rect_2d(oGroupArea);
					gGL.color4fv(LLColor4(1, 1, 1, 0.5).mV);
					gl_rect_2d(oGroupArea, FALSE);

					LLFontGL::getFontSansSerifSmall()->render(
						utf8str_to_wstring(oGroupName),
						0,
						w + 4,
						box.mBottom + breathingRoom,
						LLColor4::white,
						LLFontGL::LEFT,
						LLFontGL::BASELINE,
						LLFontGL::NORMAL, //style
						LLFontGL::DROP_SHADOW //shadow
					);
					
					if (oGroupArea.pointInRect(mouse_x, mouse_y))
					{
						gGL.color4fv(LLColor4(1, 1, 1, 1.0).mV);			
						gl_rect_2d(oGroupArea, FALSE);
						if (justClicked)
						{
							justClicked = FALSE;
							gSavedSettings.setString("FSContactSetsSelectedGroup", oGroupName);
							sInstance->updateGroupGUIs();
							sInstance->selected.clear();
							sInstance->generateCurrentList();
						}
					}
					w += sizePerOGroup + 5;
				}
			}

			//draw icons
			//not to small if we can, but not to big, but still have a good zoom effect
			LLColor4 imageColor = LLColor4::white;
			int size = llclamp(thisSize + (bubble * bMag / 2),
				llmax(10.0f, llmin((((F32)box.getHeight()) / 1.0f), 20.0f)),
				llmin(20 + (bubble * bMag / 2), thisSize + (bubble * bMag / 2)));

			std::string toolTipText = getString("TooltipFriendUnselected");
			std::string toDisplayToolTipText = "";
			
			int xLoc = box.mRight - size;
			LLUIImage* selectedImage = LLUI::getUIImage("Checkbox_Off");
			
			if (iAMSelected)
			{
				toolTipText = getString("TooltipFriendSelected");
				selectedImage = LLUI::getUIImage("Checkbox_On");
			}
			LLRect imageBox;

			gGL.color4fv(LLColor4::white.mV);
			
			imageBox.setLeftTopAndSize(xLoc, (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
			gl_draw_scaled_image_with_border(
				imageBox.mLeft - 1,
				imageBox.mBottom - 1,
				imageBox.getWidth(),
				imageBox.getHeight(),
				selectedImage->getImage(),
				LLColor4::black,
				FALSE
			);
			gl_draw_scaled_image_with_border(
				imageBox.mLeft,
				imageBox.mBottom,
				imageBox.getWidth(),
				imageBox.getHeight(),
				selectedImage->getImage(),
				imageColor,
				FALSE
			);

			if (imageBox.pointInRect(mouse_x, mouse_y))
			{
				gl_rect_2d(imageBox, FALSE);
				toDisplayToolTipText = toolTipText;

				if (justClicked && !showRightClick)
				{
					justClicked = FALSE;
					toggleSelect(workingList[p]);
				}
			}

			LLUIImage *onlineImage = LLUI::getUIImage("Permission_Visible_Online");
			toolTipText = getString("TooltipFriendOnline");
			LLColor4 overlay = imageColor;
			if(LGGContactSets::getInstance()->isNonFriend(agent_id))
			{
				//onlineImage = LLUI::getUIImage("icon_avatar_offline.tga");
				toolTipText = getString("TooltipNotOnFriendlist");
				overlay = LLColor4::black;
			}
			else if (!LLAvatarTracker::instance().getBuddyInfo(agent_id)->isOnline())
			{
				//onlineImage = LLUI::getUIImage("icon_avatar_offline.tga");
				toolTipText = getString("TooltipFriendOffline");
				overlay = LGGContactSets::toneDownColor(imageColor, 0.3f);
			}
			
			imageBox.setLeftTopAndSize(xLoc -= (1 + size), (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
			gl_draw_scaled_image_with_border(
				imageBox.mLeft - 1,
				imageBox.mBottom - 1,
				imageBox.getWidth(),
				imageBox.getHeight(),
				onlineImage->getImage(), 
				LLColor4::black,
				FALSE
			);
			gl_draw_scaled_image_with_border(
				imageBox.mLeft,
				imageBox.mBottom,
				imageBox.getWidth(),
				imageBox.getHeight(),
				onlineImage->getImage(), 
				overlay,
				FALSE
			);

			if (imageBox.pointInRect(mouse_x, mouse_y))
			{
				toDisplayToolTipText = toolTipText;
			}

			// IM button
			LLUIImage* imImage = LLUI::getUIImage("TabIcon_Translate_Off");

			imageBox.setLeftTopAndSize(xLoc -= (1 + size), (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
			gl_draw_scaled_image_with_border(
				imageBox.mLeft - 1,
				imageBox.mBottom - 1,
				imageBox.getWidth(),
				imageBox.getHeight(),
				imImage->getImage(),
				LLColor4::black,
				FALSE
			);
			gl_draw_scaled_image_with_border(
				imageBox.mLeft,
				imageBox.mBottom,
				imageBox.getWidth(),
				imageBox.getHeight(),
				imImage->getImage(),
				imageColor,
				FALSE
			);

			if (imageBox.pointInRect(mouse_x, mouse_y))
			{
				gl_rect_2d(imageBox, FALSE);				
				toDisplayToolTipText = getString("TooltipIM");
				
				if (justClicked && !showRightClick)
				{
					justClicked = FALSE;
					LLAvatarActions::startIM(agent_id);
				}
			}

			// Profile button
			LLUIImage* profileImage = LLUI::getUIImage("profile_icon_24x24");
			imageBox.setLeftTopAndSize(xLoc -= (1 + size), (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
			gl_draw_scaled_image_with_border(
				imageBox.mLeft - 1,
				imageBox.mBottom - 1,
				imageBox.getWidth(),
				imageBox.getHeight(),
				profileImage->getImage(),
				LLColor4::black,
				FALSE
			);
			gl_draw_scaled_image_with_border(
				imageBox.mLeft,
				imageBox.mBottom,
				imageBox.getWidth(),
				imageBox.getHeight(),
				profileImage->getImage(),
				imageColor,
				FALSE
			);

			if (imageBox.pointInRect(mouse_x, mouse_y))
			{
				gl_rect_2d(imageBox, FALSE);
				toDisplayToolTipText = getString("TooltipProfile");

				if (justClicked && !showRightClick)
				{
					justClicked = FALSE;
					LLAvatarActions::showProfile(agent_id);
				}
			}

			//(if set) av has been renamed button
			if (LGGContactSets::getInstance()->hasPseudonym(agent_id))
			{
				LLUIImage* profileImage = LLUI::getUIImage("icn_voice-localchat.tga");
				imageBox.setLeftTopAndSize(xLoc -= (1 + size), (box.getHeight() / 2) + 0 + box.mBottom + (size / 2), size, size);			
				gl_draw_scaled_image_with_border(
					imageBox.mLeft,
					imageBox.mBottom,
					imageBox.getWidth() - 1,
					imageBox.getHeight() - 1,
					profileImage->getImage(),
					LLColor4::black,
					FALSE
				);
				gl_draw_scaled_image_with_border(
					imageBox.mLeft,
					imageBox.mBottom,
					imageBox.getWidth(),
					imageBox.getHeight(),
					profileImage->getImage(),
					imageColor,
					FALSE
				);

				if (imageBox.pointInRect(mouse_x, mouse_y))
				{
					//gl_rect_2d(imageBox, FALSE);
					toDisplayToolTipText = getString("TooltipNameChanged");
					
					if (justClicked && !showRightClick)
					{
						//nothing yet
					}
				}
			}

			//draw hover and selected		
			if (iAMSelected)
			{
				gGL.color4fv(LLColor4(1, 1, 1, 1.0).mV);
				gl_rect_2d(box, FALSE);
				//gGL.color4fv(LLColor4(1, 1, 1, 0.7).mV);
				//gl_circle_2d(box.mRight - (box.getHeight() / 2), box.mTop - (box.getHeight() / 2), box.getHeight() / 2, 20, TRUE);
			}

			if (hoveringThis)
			{
				gGL.color4fv(LLColor4::yellow.mV);
				gl_rect_2d(box, FALSE);

				if (justClicked && !showRightClick)
				{
					BOOL found = FALSE;
					if (requireCTRL && !gKeyboard->getKeyDown(KEY_CONTROL))
					{
						found = toggleSelect(workingList[p]);
						selected.clear();
					}

					if (!found)
					{
						toggleSelect(workingList[p]);
					}
				}
				
				if (showRightClick)
				{
					if (selected.size() < 1)
					{
						toggleSelect(workingList[p]);
					}
				}
				
			}

			//draw tooltip
			if (toDisplayToolTipText != "")
			{
				int tsize = LLFontGL::getFontSansSerifSmall()->getWidth(toDisplayToolTipText) + 8;
				LLRect toolRect;toolRect.setLeftTopAndSize(mouse_x - tsize, mouse_y + 16, tsize, 16);
				gGL.color4fv(LLColor4::black.mV);
				gl_rect_2d(toolRect);
				//gGL.color4fv(LLColor4::yellow.mV);
				gl_rect_2d(toolRect, FALSE);

				LLFontGL::getFontSansSerifSmall()->render(
					utf8str_to_wstring(toDisplayToolTipText),
					0,
					toolRect.mLeft + 4,
					toolRect.mBottom + 2,
					LLColor4::white,
					LLFontGL::LEFT,
					LLFontGL::BASELINE,
					LLFontGL::BOLD, //style
					LLFontGL::DROP_SHADOW //shadow
				);
			}

			// Draw name
			if (thisSize > 8)
			{
				std::string text = "";
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(agent_id, &avatar_name))
				{
					std::string fullname;
					static LLCachedControl<S32> sPhoenixNameSystem(gSavedSettings, "FSContactSetsNameFormat");

					switch (sPhoenixNameSystem())
					{
						case 0 : fullname = avatar_name.getLegacyName(); break;
						case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
						case 2 : fullname = avatar_name.mDisplayName; break;
						default : fullname = avatar_name.getCompleteName(); break;
					}

					text += fullname;
				}

				LLFontGL* useFont = font;
				if (thisSize > 25)
				{
					useFont = bigFont;
				}
				if (thisSize > 46)
				{
					useFont = hugeFont;
				}
				if (doZoom)
				{
					if (thisSize > 14)
					{
						useFont = bigFont;
					}
					if (thisSize > 35)
					{
						useFont = hugeFont;
					}
				}
				
				LLColor4 nameTextColor = LLColor4::white;
				if (textNotBg && groupsIsIn.size() > 0)
				{
					nameTextColor=LGGContactSets::toneDownColor(color, 1.0f);
				}
				
				useFont->render(utf8str_to_wstring(text),
					0,
					box.mLeft + roomForBar + roomForIcon, //x
					top - (thisSize / 2) + (doZoom ? -2 : 2), //y
					nameTextColor, //color
					LLFontGL::LEFT, //halign
					LLFontGL::BASELINE, //valign
					LLFontGL::NORMAL, //style
					LLFontGL::DROP_SHADOW //shadow
				);

				//useFont->renderUTF8(llformat(" %d ", p), 0, box.mLeft + 0, top - (thisSize / 2), LLColor4::white, LLFontGL::LEFT,
				//	LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);
			}

//#pragma endregion DrawListItem
		}

		top -= (thisSize + 1);
	}

	// Mouse
	if (mouse_x != 0 && mouse_y != 0 && mouse_y < rec.mTop && (gFrameTimeSeconds - 2 < hovered))
	{
		gGL.color4fv(LLColor4::black.mV);
		//gl_circle_2d(mouse_x, mouse_y, 20.0f, ((S32)(gFrameTimeSeconds)) % 4 + 3, false);
	}

	/*font->renderUTF8(
		llformat("Mouse is at %d , %d scoll lock is %d height is %d panels is %d",
			mouse_x,
			mouse_y,
			scrollLoc,
			rec.getHeight(),
			numberOfPanels),
		0,
		rec.mLeft,
		rec.mBottom + 0,
		LLColor4::white,
		LLFontGL::LEFT,
		LLFontGL::BASELINE,
		LLFontGL::DROP_SHADOW
	);*/

	drawFilter();
	if (showRightClick)
	{
		drawRightClick();
	}

	gGL.popMatrix();
	justClicked = FALSE;
}

BOOL lggContactSetsFloater::toggleSelect(LLUUID whoToToggle)
{
	justClicked = FALSE;
	bool found = false;

	for (int i = 0; i < (int)selected.size(); i++)
	{
		if(selected[i] == whoToToggle)
		{
			found = true;
		}
	}

	if (!found)
	{
		selected.push_back(whoToToggle);
	}
	else
	{
		std::vector<LLUUID> newList;
		newList.clear();
		for (int i = 0; i < (int)selected.size(); i++)
		{
			if (selected[i] != whoToToggle)
			{
				newList.push_back(selected[i]);
			}
		}
		selected = newList;
	}
	return found;
}

BOOL lggContactSetsFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
	sInstance->justClicked = true;
	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL lggContactSetsFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (!showRightClick)
	{
		contextRect.setLeftTopAndSize(x, y, 2, 2);
		showRightClick = TRUE;
		sInstance->currentRightClickText = "";
	}
	else
	{
		justClicked = TRUE;
	}
	return LLFloater::handleRightMouseDown(x, y, mask);
}

BOOL lggContactSetsFloater::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	LLRect rec = sInstance->getChild<LLPanel>("draw_region")->getRect();

	int maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 11 + 200 - rec.getHeight();
	
	static LLCachedControl<bool> doZoom(gSavedSettings, "FSContactSetsDoZoom");
	if (!doZoom)
	{
		maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 26 + 10 - rec.getHeight();
	}

	int moveAmt = 12;
	if (!doZoom)
	{
		moveAmt = 26;
	}

	sInstance->scrollLoc=llclamp(sInstance->scrollLoc + (clicks * moveAmt), 0, maxS);
	return LLFloater::handleScrollWheel(x, y, clicks);
}

BOOL lggContactSetsFloater::handleUnicodeCharHere(llwchar uni_char)
{
	if ((uni_char < 0x20) || (uni_char == 0x7F)) // Control character or DEL
	{
		return FALSE;
	}

	if(' ' == uni_char 
		&& !gKeyboard->getKeyRepeated(' ')
		&& 
		((!sInstance->showRightClick && sInstance->currentFilter == "") || (sInstance->showRightClick && sInstance->currentRightClickText == ""))
		)
	{
		sInstance->justClicked = TRUE;
	}
	else
	{
		if (gKeyboard->getKeyDown(KEY_CONTROL) && 22 == (U32)uni_char)
		{
			LLWString strW;
			LLClipboard::instance().pasteFromClipboard( strW );
			std::string toPaste = wstring_to_utf8str( strW );
			if (sInstance->showRightClick)
			{
				sInstance->currentRightClickText += toPaste;
			}
			else
			{
				sInstance->currentFilter += toPaste;
			}
		}
		else if ((U32)uni_char != 27 && (U32)uni_char != 8)
		{
			//sInstance->getChild<LLCheckBoxCtrl>("haxCheckbox")->setFocus(TRUE);
			if (!sInstance->showRightClick)
			{
				sInstance->currentFilter += uni_char;
				sInstance->generateCurrentList();
			}
			else
			{
				sInstance->currentRightClickText += uni_char;
			}
			
			return TRUE;
		}
	}

	return LLFloater::handleUnicodeCharHere(uni_char);
}

BOOL lggContactSetsFloater::handleKeyHere(KEY key, MASK mask)
{
	LLRect rec = sInstance->getChild<LLPanel>("draw_region")->getRect();
	
	int maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 11 + 200 - rec.getHeight();

	static LLCachedControl<bool> doZoom(gSavedSettings, "FSContactSetsDoZoom");
	
	if (!doZoom)
	{
		maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 26 + 10 - rec.getHeight();
	}

	std::string localFilter = sInstance->currentFilter;
	if (sInstance->showRightClick)
	{
		localFilter=sInstance->currentRightClickText;
	}

	int curLoc = sInstance->scrollLoc;
	
	if (key == KEY_PAGE_UP)
	{
		curLoc -= rec.getHeight();
	}
	else if (key == KEY_UP)
	{
		if (!doZoom)
		{
			curLoc -= 26;
		}
		else
		{
			curLoc -= 12;
		}
	}
	else if(key == KEY_PAGE_DOWN)
	{
		curLoc += rec.getHeight();
	}
	else if (key == KEY_DOWN)
	{
		if (!doZoom)
		{
			curLoc += 26;
		}
		else
		{
			curLoc += 12;
		}
	}

	if (key == KEY_ESCAPE)
	{
		if (localFilter != "")
		{
			sInstance->currentFilter = "";
			sInstance->generateCurrentList();
			return TRUE;
		}
		if (sInstance->showRightClick)
		{
			sInstance->showRightClick = FALSE;
			return TRUE;
		}
	}

	if(key == KEY_RETURN)
	{
		sInstance->justClicked = TRUE;
	}

	if (key == KEY_BACKSPACE)
	{
		int length = localFilter.length();
		if (length > 0)
		{
			length--;
			if (!sInstance->showRightClick)
			{
				sInstance->currentFilter = localFilter.substr(0, length);
				sInstance->generateCurrentList();
			}
			else
			{
				sInstance->currentRightClickText = localFilter.substr(0, length);
			}
		}
	}

	sInstance->scrollLoc=llclamp(curLoc, 0, maxS);

	return LLFloater::handleKeyHere(key, mask);
}

BOOL lggContactSetsFloater::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLRect rec = sInstance->getChild<LLPanel>("draw_region")->getRect();
	LLRect topScroll = sInstance->getChild<LLPanel>("top_region")->getRect();
	LLRect bottomScroll = sInstance->getChild<LLPanel>("bottom_region")->getRect();

	int maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 11 + 200 - rec.getHeight();
	static LLCachedControl<bool> doZoom(gSavedSettings, "FSContactSetsDoZoom");
	
	if (!doZoom)
	{
		maxS = (sInstance->currentList.size() + sInstance->allFolders.size()) * 26 + 10 - rec.getHeight();
	}
	
	if (bottomScroll.pointInRect(x, y))
	{
		sInstance->scrollLoc = maxS;
	}
	else if (topScroll.pointInRect(x, y))
	{
		sInstance->scrollLoc = 0;
	}

	return LLFloater::handleDoubleClick(x, y, mask);
}

BOOL lggContactSetsFloater::handleHover(S32 x, S32 y, MASK mask)
{
	sInstance->mouseInWindow = TRUE;
	mouse_x = x;
	mouse_y = y;
	hovered = gFrameTimeSeconds;
	return LLFloater::handleHover(x, y, mask);
}

void lggContactSetsFloater::onMouseLeave(S32 x, S32 y, MASK mask)
{
	sInstance->mouseInWindow = FALSE;
}

void lggContactSetsFloater::onMouseEnter(S32 x, S32 y, MASK mask)
{
	sInstance->mouseInWindow = TRUE;
}

BOOL lggContactSetsFloater::compareAv(LLUUID av1, LLUUID av2)
{
	static LLCachedControl<S32> sPhoenixNameSystem(gSavedSettings, "FSContactSetSortNameFormat");

	std::string avN1("");
	std::string avN2("");
	LLAvatarName avatar_name;
	if (LLAvatarNameCache::get(av1, &avatar_name))
	{
		std::string fullname;
		switch (sPhoenixNameSystem())
		{
			case 0 : fullname = avatar_name.getLegacyName(); break;
			case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
			case 2 : fullname = avatar_name.mDisplayName; break;
			default : fullname = avatar_name.getCompleteName(); break;
		}

		avN1 = fullname;
	}
	if (LLAvatarNameCache::get(av2, &avatar_name))
	{
		std::string fullname;
		switch (sPhoenixNameSystem())
		{
			case 0 : fullname = avatar_name.getLegacyName(); break;
			case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
			case 2 : fullname = avatar_name.mDisplayName; break;
			default : fullname = avatar_name.getCompleteName(); break;
		}

		avN2 = fullname;
	}
	LLStringUtil::toLower(avN2);
	LLStringUtil::toLower(avN1);
	
	return (avN1.compare(avN2) < 0);
}

BOOL lggContactSetsFloater::generateCurrentList()
{
	static LLCachedControl<bool> showOnline(gSavedSettings, "FSContactSetsShowOnline");
	static LLCachedControl<bool> showOffline(gSavedSettings, "FSContactSetsShowOffline");
	static LLCachedControl<bool> yshowAllFriends(gSavedSettings, "FSContactSetsShowAllFriends");
	//static LLCachedControl<bool> showOtherGroups(gSavedSettings, "FSContactSetsShowOtherGroups");
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");

	currentList.clear();
	std::map<LLUUID, LLRelationship*>::iterator p;
	std::map<LLUUID, LLRelationship*> friends;
	LLAvatarTracker::instance().copyBuddyList(friends);

	for(p = friends.begin(); p != friends.end(); p++)
	{
		LLRelationship* relation = p->second;
		if (!showOnline && relation->isOnline())
		{
			continue;
		}

		if (!showOffline && !relation->isOnline())
		{
			continue;
		}

		if (!yshowAllFriends && !LGGContactSets::getInstance()->isFriendInGroup(p->first,currentGroup))
		{
			continue;
		}

		currentList.push_back(p->first);
	}

	//add ppl not in friends list
	std::vector<LLUUID> nonFriends = LGGContactSets::getInstance()->getListOfNonFriends();
	//currentList.insert(currentList.end(), nonFriends.begin(), nonFriends.end());
	for (int i = 0; i < (int)nonFriends.size(); i++)
	{
		if (!showOffline)
		{
			continue;
		}

		if (!yshowAllFriends && !LGGContactSets::getInstance()->isFriendInGroup(nonFriends[i], currentGroup))
		{
			continue;
		}

		currentList.push_back(nonFriends[i]);
	}

	//filter \o/
	if (sInstance->currentFilter != "")
	{
		std::vector<LLUUID> newList;
		std::string workingFilter = sInstance->currentFilter;
		LLStringUtil::toLower(workingFilter);
		for (int itFilter = 0; itFilter < (int)currentList.size(); itFilter++)
		{
			std::string avN("");
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(currentList[itFilter], &avatar_name))
			{
				std::string fullname;
				static LLCachedControl<S32> sPhoenixNameSystem(gSavedSettings, "FSContactSetsNameFormat");
				switch (sPhoenixNameSystem())
				{
					case 0 : fullname = avatar_name.getLegacyName(); break;
					case 1 : fullname = (avatar_name.mIsDisplayNameDefault ? avatar_name.mDisplayName : avatar_name.getCompleteName()); break;
					case 2 : fullname = avatar_name.mDisplayName; break;
					default : fullname = avatar_name.getCompleteName(); break;
				}
				avN = fullname;
			}

			LLStringUtil::toLower(avN);			
			if (avN.find(workingFilter) != std::string::npos)
			{
				newList.push_back(currentList[itFilter]);
			}
		}
		currentList=newList;
	}

	std::sort(currentList.begin(), currentList.end(), &lggContactSetsFloater::compareAv);
	profileImagePending.clear();
	return TRUE;
}

void lggContactSetsFloater::onClickDelete(void* data)
{
	static LLCachedControl<std::string> currentGroup(gSavedSettings, "FSContactSetsSelectedGroup");

	LGGContactSets::getInstance()->deleteGroup(currentGroup);
	gSavedSettings.setString("FSContactSetsSelectedGroup", "");
	sInstance->updateGroupsList();
}

void lggContactSetsFloater::onClickNew(void* data)
{
	LLLineEditor* line =sInstance->getChild<LLLineEditor>("lgg_fg_groupNewName");
	std::string text = line->getText();
	if (text != "")
	{
		LGGContactSets::getInstance()->addGroup(text);
		line->setText(LLStringExplicit(""));
	}

	sInstance->updateGroupsList();
}

void lggContactSetsFloater::onClickSettings(void* data)
{
	lggContactSetsFloaterSettings::showFloater();
}


/////////////////////////////////////////////////////////////////////////////
// Contact set settings floater
/////////////////////////////////////////////////////////////////////////////
lggContactSetsFloaterSettings* lggContactSetsFloaterSettings::sSettingsInstance;
void lggContactSetsFloaterSettings::onClose(bool app_quitting)
{
	sSettingsInstance = NULL;
	destroy();
}

lggContactSetsFloaterSettings* lggContactSetsFloaterSettings::showFloater()
{
	lggContactSetsFloaterSettings *floater = dynamic_cast<lggContactSetsFloaterSettings*>(LLFloaterReg::getInstance("contactsetsettings"));
	if (floater)
	{
		floater->setVisible(true);
		floater->setFrontmost(true);
		floater->center();
		return floater;
	}
	else
	{
		LL_WARNS("LGGContactSetS") << "Can't find floater!" << LL_ENDL;
		return NULL;
	}
}

lggContactSetsFloaterSettings::~lggContactSetsFloaterSettings()
{
	sSettingsInstance = NULL;
}

lggContactSetsFloaterSettings::lggContactSetsFloaterSettings(const LLSD& seed) : LLFloater(seed)
{
	if (sSettingsInstance)
	{
		delete sSettingsInstance;
	}
	sSettingsInstance= this;

	if (getRect().mLeft == 0 && getRect().mBottom == 0)
	{
		center();
	}
}

BOOL lggContactSetsFloaterSettings::postBuild(void)
{
	childSetAction("lgg_fg_okButton", onClickOk, this);
	getChild<LLColorSwatchCtrl>("colordefault")->setCommitCallback(boost::bind(&lggContactSetsFloaterSettings::onDefaultBackgroundChange, this));
	getChild<LLColorSwatchCtrl>("colordefault")->set(LGGContactSets::getInstance()->getDefaultColor());

	LLComboBox* dispName = getChild<LLComboBox>("lgg_fg_dispName");
	dispName->setCommitCallback(boost::bind(&lggContactSetsFloaterSettings::onSelectNameFormat, this));
	dispName->setCurrentByIndex(gSavedSettings.getS32("FSContactSetsNameFormat"));

	LLComboBox* sortName = getChild<LLComboBox>("lgg_fg_sortName");
	sortName->setCommitCallback(boost::bind(&lggContactSetsFloaterSettings::onSelectNameFormat, this));
	sortName->setCurrentByIndex(gSavedSettings.getS32("FSContactSetSortNameFormat"));

	return TRUE;
}

void lggContactSetsFloaterSettings::onDefaultBackgroundChange()
{
	LGGContactSets::getInstance()->setDefaultColor(
		sSettingsInstance->getChild<LLColorSwatchCtrl>("colordefault")->get()
		);
}

void lggContactSetsFloaterSettings::onClickOk(void* data)
{
	sSettingsInstance->closeFloater();
}

void lggContactSetsFloaterSettings::onSelectNameFormat()
{
	gSavedSettings.setS32("FSContactSetsNameFormat",
		getChild<LLComboBox>("lgg_fg_dispName")->getCurrentIndex());	
	gSavedSettings.setS32("FSContactSetSortNameFormat",
		getChild<LLComboBox>("lgg_fg_sortName")->getCurrentIndex());
}
