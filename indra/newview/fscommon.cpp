/**
 * @file fscommon.cpp
 * @brief Central class for common used functions in Firestorm
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fscommon.h"
#include "llagent.h"
#include "llfloatersidepanelcontainer.h"
#include "llnotificationmanager.h"
#include "llinventorymodel.h"
#include "llpanel.h"
#include "lltooldraganddrop.h"
#include "llviewerinventory.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
using namespace boost::posix_time;
using namespace boost::gregorian;

static const std::string LL_LINDEN = "Linden";
static const std::string LL_MOLE = "Mole";
static const std::string LL_PRODUCTENGINE = "ProductEngine";
static const std::string LL_SCOUT = "Scout";
static const std::string LL_TESTER = "Tester";

S32 FSCommon::sObjectAddMsg = 0;

void reportToNearbyChat(const std::string& message)
{
	LLChat chat;
    chat.mText = message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
	LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
}

std::string applyAutoCloseOoc(const std::string& message)
{
	if (!gSavedSettings.getBOOL("AutoCloseOOC"))
	{
		return message;
	}

	std::string utf8_text(message);

	// Try to find any unclosed OOC chat (i.e. an opening
	// double parenthesis without a matching closing double
	// parenthesis.
	if (utf8_text.find("(( ") != -1 && utf8_text.find("))") == -1)
	{
		// add the missing closing double parenthesis.
		utf8_text += " ))";
	}
	else if (utf8_text.find("((") != -1 && utf8_text.find("))") == -1)
	{
		if (utf8_text.at(utf8_text.length() - 1) == ')')
		{
			// cosmetic: add a space first to avoid a closing triple parenthesis
			utf8_text += " ";
		}
		// add the missing closing double parenthesis.
		utf8_text += "))";
	}
	else if (utf8_text.find("[[ ") != -1 && utf8_text.find("]]") == -1)
	{
		// add the missing closing double parenthesis.
		utf8_text += " ]]";
	}
	else if (utf8_text.find("[[") != -1 && utf8_text.find("]]") == -1)
	{
		if (utf8_text.at(utf8_text.length() - 1) == ']')
		{
			// cosmetic: add a space first to avoid a closing triple parenthesis
			utf8_text += " ";
		}
			// add the missing closing double parenthesis.
		utf8_text += "]]";
	}

	return utf8_text;
}

std::string applyMuPose(const std::string& message)
{
	std::string utf8_text(message);

	// Convert MU*s style poses into IRC emotes here.
	if (gSavedSettings.getBOOL("AllowMUpose") && utf8_text.find(":") == 0 && utf8_text.length() > 3)
	{
		if (utf8_text.find(":'") == 0)
		{
			utf8_text.replace(0, 1, "/me");
 		}
		else if (!isdigit(utf8_text.at(1)) && !ispunct(utf8_text.at(1)) && !isspace(utf8_text.at(1)))	// Do not prevent smileys and such.
		{
			utf8_text.replace(0, 1, "/me ");
		}
	}

	return utf8_text;
}

std::string formatString(std::string text, const LLStringUtil::format_map_t& args)
{
	LLStringUtil::format(text, args);
	return text;
}

LLPanelPeople* getPeoplePanel()
{
	LLPanel* panel_people = LLFloaterSidePanelContainer::getPanel("people", "panel_people");
	if (panel_people)
	{
		return dynamic_cast<LLPanelPeople*>(panel_people);
	}
	return NULL;
}

S32 FSCommon::secondsSinceEpochFromString(const std::string& format, const std::string& str)
{
	// LLDateUtil::secondsSinceEpochFromString does not handle time, only the date.
	// copied that function here and added the needed code to handle time fields.  -- TL
	time_input_facet *facet = new time_input_facet(format);
	std::stringstream ss;
	ss << str;
	ss.imbue(std::locale(ss.getloc(), facet));
	ptime time_t_date;
	ss >> time_t_date;
	ptime time_t_epoch(date(1970,1,1));
	time_duration diff = time_t_date - time_t_epoch;
	return diff.total_seconds();
}

void FSCommon::applyDefaultBuildPreferences(LLViewerObject* object)
{
	if (!object)
	{
		return;
	}
  
	LLTextureEntry texture_entry;
	texture_entry.setID(LLUUID(gSavedSettings.getString("FSDefaultObjectTexture")));
	texture_entry.setColor(gSavedSettings.getColor4("FSBuildPrefs_Color"));
	texture_entry.setAlpha((100.f - gSavedSettings.getF32("FSBuildPrefs_Alpha")) / 100.f);
	texture_entry.setGlow(gSavedSettings.getF32("FSBuildPrefs_Glow"));
	if(gSavedSettings.getBOOL("FSBuildPrefs_FullBright"))
	{
		texture_entry.setFullbright(TEM_FULLBRIGHT_MASK);
	}
	
	U8 shiny = 0; // Default none
	std::string shininess = gSavedSettings.getString("FSBuildPrefs_Shiny");
	if(shininess == "Low")
	{
		shiny = 1;
	}
	else if(shininess == "Medium")
	{
		shiny = 2;
	}
	else if(shininess == "High")
	{
		shiny = 3;
	}
	texture_entry.setShiny(shiny);
	
	for(U8 face = 0; face < object->getNumTEs(); face++)
	{
		object->setTE(face, texture_entry);
	}
	object->sendTEUpdate();
	
	if(gSavedSettings.getBOOL("FSBuildPrefs_EmbedItem"))
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)gInventory.getItem((LLUUID)gSavedSettings.getString("FSBuildPrefs_Item"));
		if(item)
		{
			if (item->getType() == LLAssetType::AT_LSL_TEXT)
			{
				
				LLToolDragAndDrop::dropScript(object, item, TRUE,
							      LLToolDragAndDrop::SOURCE_AGENT,
							      gAgentID);
			}
			else
			{
				LLToolDragAndDrop::dropInventory(object, item,
								LLToolDragAndDrop::SOURCE_AGENT,
								gAgentID);
			}
		}
	}
	
	U32 object_local_id = object->getLocalID();
	gMessageSystem->newMessageFast(_PREHASH_ObjectPermissions);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_HeaderData);
	gMessageSystem->addBOOLFast(_PREHASH_Override, (BOOL)FALSE);
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
	gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
	gMessageSystem->addBOOLFast(_PREHASH_Set, gSavedSettings.getBOOL("NextOwnerModify"));
	gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_MODIFY);
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
	gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
	gMessageSystem->addBOOLFast(_PREHASH_Set, gSavedSettings.getBOOL("NextOwnerCopy"));
	gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_COPY);
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
	gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
	gMessageSystem->addBOOLFast(_PREHASH_Set, gSavedSettings.getBOOL("NextOwnerTransfer"));
	gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_TRANSFER);
	gMessageSystem->sendReliable(object->getRegion()->getHost());

	gMessageSystem->newMessage(_PREHASH_ObjectFlagUpdate);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
	gMessageSystem->addBOOLFast(_PREHASH_UsePhysics, gSavedSettings.getBOOL("FSBuildPrefs_Physical"));
	gMessageSystem->addBOOL(_PREHASH_IsTemporary, gSavedSettings.getBOOL("FSBuildPrefs_Temporary"));
	gMessageSystem->addBOOL(_PREHASH_IsPhantom, gSavedSettings.getBOOL("FSBuildPrefs_Phantom"));
	gMessageSystem->addBOOL("CastsShadows", FALSE );
	gMessageSystem->sendReliable(object->getRegion()->getHost());
}

bool FSCommon::isLinden(const LLUUID& av_id)
{
#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		return false;
	}
#endif

	std::string full_name;
	gCacheName->getFullName(av_id, full_name);

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(full_name, sep);
	tokenizer::iterator token_iter = tokens.begin();
	
	if (token_iter == tokens.end()) return FALSE;
	token_iter++;
	if (token_iter == tokens.end()) return FALSE;
	
	std::string last_name = *token_iter;

	return (last_name == LL_LINDEN ||
			last_name == LL_MOLE ||
			last_name == LL_PRODUCTENGINE ||
			last_name == LL_SCOUT ||
			last_name == LL_TESTER);
}