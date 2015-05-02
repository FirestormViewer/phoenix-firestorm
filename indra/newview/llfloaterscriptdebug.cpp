/** 
 * @file llfloaterscriptdebug.cpp
 * @brief Chat window for showing script errors and warnings
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llfloaterscriptdebug.h"

#include "llfloaterreg.h"
#include "lluictrlfactory.h"
#include "llfontgl.h"
#include "llrect.h"
#include "llerror.h"
#include "llstring.h"
#include "message.h"
#include "lltrans.h"

// project include
#include "llviewertexteditor.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewertexturelist.h"

//
// Statics
//

//
// Member Functions
//
LLFloaterScriptDebug::LLFloaterScriptDebug(const LLSD& key)
  : LLMultiFloater(key)
{
	// avoid resizing of the window to match 
	// the initial size of the tabbed-childs, whenever a tab is opened or closed
	mAutoResize = FALSE;
	// enabled autocous blocks controling focus via  LLFloaterReg::showInstance
	setAutoFocus(FALSE);
}

LLFloaterScriptDebug::~LLFloaterScriptDebug()
{
}

void LLFloaterScriptDebug::show(const LLUUID& object_id)
{
	// <FS:Ansariel> Script debug icon
	//addOutputWindow(object_id);
	addOutputWindow(object_id, true);
	// </FS:Ansariel> Script debug icon
}

BOOL LLFloaterScriptDebug::postBuild()
{
	LLMultiFloater::postBuild();

	// <FS:Ansariel> Disconnect close signal. It would call closeAllFloaters()
	//               and the floaters in the tabs will be gone since LL
	//               messed with the floater instance handling!
	mCloseSignal.disconnect_all_slots();

	if (mTabContainer)
	{
		return TRUE;
	}

	return FALSE;
}

// <FS:Ansariel> Script debug icon
//LLFloater* LLFloaterScriptDebug::addOutputWindow(const LLUUID &object_id)
LLFloater* LLFloaterScriptDebug::addOutputWindow(const LLUUID& object_id, bool show /* = false */)
// </FS:Ansariel> Script debug icon
{
	LLMultiFloater* host = LLFloaterReg::showTypedInstance<LLMultiFloater>("script_debug", LLSD());
	if (!host)
		return NULL;

	LLFloater::setFloaterHost(host);
	// prevent stealing focus, see EXT-8040
	LLFloater* floaterp = LLFloaterReg::showInstance("script_debug_output", object_id, FALSE);
	LLFloater::setFloaterHost(NULL);

	// <FS:Ansariel> Script debug icon
	if (gSavedSettings.getS32("ShowScriptErrorsLocation") == 0 && !show)
	{
		host->closeFloater();
	}
	// </FS:Ansariel> Script debug icon

	return floaterp;
}

// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow] Show llOwnerSays in the script debug window instead of local chat
// void LLFloaterScriptDebug::addScriptLine(const std::string &utf8mesg, const std::string &user_name, const LLColor4& color, const LLUUID& source_id)
void LLFloaterScriptDebug::addScriptLine(const LLChat& chat)
{
	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// LLViewerObject* objectp = gObjectList.findObject(source_id);
	LLViewerObject* objectp = gObjectList.findObject(chat.mFromID);
	std::string floater_label;

	// Handle /me messages.
	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// std::string prefix = utf8mesg.substr(0, 4);
	// std::string message = (prefix == "/me " || prefix == "/me'") ? user_name + utf8mesg.substr(3) : utf8mesg;

	if (objectp)
	{
		// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
		if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
		{
			objectp->setIcon(LLViewerTextureManager::getFetchedTextureFromFile("script_error.j2c", FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_UI));
			// <FS:Ansariel> Mark script error icons
			objectp->getIcon()->setScriptError();
			// </FS:Ansariel> Mark script error icons
		}
		// </FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
		floater_label = llformat("%s (%.0f, %.0f, %.0f)",
						// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
						// user_name.c_str(),
						chat.mFromName.c_str(),
						objectp->getPositionRegion().mV[VX],
						objectp->getPositionRegion().mV[VY],
						objectp->getPositionRegion().mV[VZ]);
	}
	else
	{
		// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
		// floater_label = user_name;
		floater_label = chat.mFromName;
	}

	addOutputWindow(LLUUID::null);
	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// addOutputWindow(source_id);
	addOutputWindow(chat.mFromID);

	// add to "All" floater
	LLFloaterScriptDebugOutput* floaterp = 	LLFloaterReg::getTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", LLUUID::null);
	if (floaterp)
	{
		// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
		// floaterp->addLine(message, user_name, color);
		floaterp->addLine(chat, chat.mFromName);
	}
	
	// add to specific script instance floater
	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// floaterp = LLFloaterReg::getTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", source_id);
	floaterp = LLFloaterReg::getTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", chat.mFromID);
	if (floaterp)
	{
		// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
		// floaterp->addLine(message, floater_label, color);
		floaterp->addLine(chat, floater_label);
	}
}

//
// LLFloaterScriptDebugOutput
//

LLFloaterScriptDebugOutput::LLFloaterScriptDebugOutput(const LLSD& object_id)
  : LLFloater(LLSD(object_id)),
	mObjectID(object_id.asUUID())
{
	// enabled autocous blocks controling focus via  LLFloaterReg::showInstance
	setAutoFocus(FALSE);
}

BOOL LLFloaterScriptDebugOutput::postBuild()
{
	LLFloater::postBuild();
	mHistoryEditor = getChild<LLViewerTextEditor>("Chat History Editor");
	return TRUE;
}

LLFloaterScriptDebugOutput::~LLFloaterScriptDebugOutput()
{
}

// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
// void LLFloaterScriptDebugOutput::addLine(const std::string &utf8mesg, const std::string &user_name, const LLColor4& color)
void LLFloaterScriptDebugOutput::addLine(const LLChat& chat, const std::string &user_name)
{
	if (mObjectID.isNull())
	{
		setCanTearOff(FALSE);
		setCanClose(FALSE);
	}
	else
	{
		setTitle(user_name);
		setShortTitle(user_name);
	}

	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// mHistoryEditor->appendText(utf8mesg, true, LLStyle::Params().color(color));

	bool prependNewLineState = !mHistoryEditor->getText().empty();

	LLColor4 txt_color = LLUIColorTable::instance().getColor("White");
	LLViewerChat::getChatColor(chat, txt_color);

	//IRC styled /me messages.
	std::string prefix = chat.mText.substr(0, 4);
	bool irc_me = (prefix == "/me " || prefix == "/me'");

	if (chat.mChatType == CHAT_TYPE_OWNER)
	{

		LLFontGL* fontp = LLViewerChat::getChatFont();
		std::string font_name = LLFontGL::nameFromFont(fontp);
		std::string font_size = LLFontGL::sizeFromFont(fontp);

		LLStyle::Params body_message_params;
		body_message_params.color(txt_color);
		body_message_params.readonly_color(txt_color);
		body_message_params.font.name(font_name);
		body_message_params.font.size(font_size);

		// Delimiter after a name in header copy/past and in plain text mode
		std::string delimiter = ": ";
		// Don't add any delimiter after name in irc styled messages
		if (irc_me || chat.mChatStyle == CHAT_STYLE_IRC)
		{
			delimiter = LLStringUtil::null;
		}

		time_t utc_time;
		utc_time = time_corrected();
		std::string timeStr ="["+ LLTrans::getString("TimeHour")+"]:["+LLTrans::getString("TimeMin")+"]";
		LLSD substitution;
		substitution["datetime"] = (S32) utc_time;
		LLStringUtil::format (timeStr, substitution);

		LLStyle::Params timestamp_style(body_message_params);
		LLColor4 timestamp_color = LLUIColorTable::instance().getColor("ChatTimestampColor");
		timestamp_style.color(timestamp_color);
		timestamp_style.readonly_color(timestamp_color);
		mHistoryEditor->appendText("[" + timeStr + "] ", prependNewLineState, timestamp_style);

		// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
		// NOTE-RLVa: we don't need to do any @shownames or @showloc filtering here because we'll already have an existing URL
		std::string url = chat.mURL;
		if ( (url.empty()) || (std::string::npos == url.find("objectim")) )
		{
	// [/RLVa:KB]
			// for object IMs, create a secondlife:///app/objectim SLapp
			/*std::string*/ url = LLViewerChat::getSenderSLURL(chat, LLSD());
	// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
		}
	// [/RLVa:KB]

		// set the link for the object name to be the objectim SLapp
		// (don't let object names with hyperlinks override our objectim Url)
		LLStyle::Params link_params(body_message_params);
		link_params.color.control = "ChatNameObjectColor";
		LLColor4 link_color = LLUIColorTable::instance().getColor("ChatNameObjectColor");
		link_params.color = link_color;
		link_params.readonly_color = link_color;
		link_params.is_link = true;
		link_params.link_href = url;
		mHistoryEditor->appendText(chat.mFromName + delimiter, false, link_params);	// <FS:Zi> FIRE-8600: TAB out of chat history

		std::string message = irc_me ? chat.mText.substr(3) : chat.mText;

		mHistoryEditor->appendText(message, false, body_message_params);
	}
	else
	{
		std::string message = irc_me ? chat.mFromName + chat.mText.substr(3) : chat.mText;
		mHistoryEditor->appendText(message, prependNewLineState, LLStyle::Params().color(txt_color));
	}
	// </FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	mHistoryEditor->blockUndo();
}
