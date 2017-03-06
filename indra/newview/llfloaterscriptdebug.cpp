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
#include "llvoavatarself.h"
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

// <FS:Ansariel> Improved script debug floater
//void LLFloaterScriptDebug::setVisible(BOOL visible)
//{
//	if(visible)
//	{
//		LLFloaterScriptDebugOutput* floater_output = LLFloaterReg::findTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", LLUUID::null);
//		if (floater_output == NULL)
//		{
//			floater_output = dynamic_cast<LLFloaterScriptDebugOutput*>(LLFloaterReg::showInstance("script_debug_output", LLUUID::null, FALSE));
//			if (floater_output)
//			{
//				addFloater(floater_output, false);
//			}
//		}

//	}
//	LLMultiFloater::setVisible(visible);
//}

//void LLFloaterScriptDebug::closeFloater(bool app_quitting/* = false*/)
//{
//	if(app_quitting)
//	{
//		LLMultiFloater::closeFloater(app_quitting);
//	}
//	else
//	{
//		setVisible(false);
//	}
//}
// </FS:Ansariel>

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
			if(objectp->isHUDAttachment())
			{
				((LLViewerObject*)gAgentAvatarp)->setIcon(LLViewerTextureManager::getFetchedTextureFromFile("script_error.j2c", FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_UI));
			}
			else
			{
				objectp->setIcon(LLViewerTextureManager::getFetchedTextureFromFile("script_error.j2c", FTT_LOCAL_FILE, TRUE, LLGLTexture::BOOST_UI));
			}
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

	addOutputWindow(LLUUID::null); // <FS:Ansariel> Improved script debug floater
	// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
	// addOutputWindow(source_id);
	static LLCachedControl<U32> FSllOwnerSayRouting(gSavedSettings, "FSllOwnerSayToScriptDebugWindowRouting");

	// add to "All" floater
	if ((chat.mChatType == CHAT_TYPE_DEBUG_MSG) || (FSllOwnerSayRouting != 1))
	{
		LLFloaterScriptDebugOutput* floaterp = LLFloaterReg::findTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", LLUUID::null);
		if (floaterp)
		{
			floaterp->addLine(chat, chat.mFromName);
		}
	}

	// add to specific script instance floater
	if ((chat.mChatType == CHAT_TYPE_DEBUG_MSG) || (FSllOwnerSayRouting != 2))
	{	
		addOutputWindow(chat.mFromID);
		LLFloaterScriptDebugOutput* floaterp = LLFloaterReg::findTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", chat.mFromID);
		if (floaterp)
		{
			floaterp->addLine(chat, floater_label);
		}
	}

#if 0
	// add to "All" floater
	LLFloaterScriptDebugOutput* floaterp = 	LLFloaterReg::getTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", LLUUID::null);
	if (floaterp)
	{
		floaterp->addLine(message, user_name, color);
	}
	
	// add to specific script instance floater
	floaterp = LLFloaterReg::getTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", source_id);
	if (floaterp)
	{
		floaterp->addLine(message, floater_label, color);
	}
#endif
	// </FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
}

// <FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
// virtual
void LLFloaterScriptDebug::onClickCloseBtn(bool app_qutting)
{
	if (gSavedSettings.getBOOL("FSScriptDebugWindowClearOnClose"))
	{
		LLFloaterScriptDebugOutput* floaterp = LLFloaterReg::findTypedInstance<LLFloaterScriptDebugOutput>("script_debug_output", LLUUID::null);
		if (floaterp)
		{
			floaterp->clear();
		}
	}
	LLMultiFloater::onClickCloseBtn(app_qutting);
}
// </FS:Kadah> [FSllOwnerSayToScriptDebugWindow]

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

#if 0
void LLFloaterScriptDebugOutput::addLine(const std::string &utf8mesg, const std::string &user_name, const LLColor4& color)
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

	mHistoryEditor->appendText(utf8mesg, true, LLStyle::Params().color(color));
	mHistoryEditor->blockUndo();
}
#endif

void LLFloaterScriptDebugOutput::addLine(const LLChat& chat, const std::string &user_name)
{
	LLFontGL* fontp = LLViewerChat::getChatFont();
	std::string font_name = LLFontGL::nameFromFont(fontp);
	std::string font_size = LLFontGL::sizeFromFont(fontp);
	LLStyle::Params message_params;
	message_params.font.name(font_name);
	message_params.font.size(font_size);

	if (mObjectID.isNull())
	{
		setCanTearOff(FALSE);
		setCanClose(FALSE);
	}
	else
	{
		// Print object name slurl to output on first output or name change
		if (mHistoryEditor->getText().empty() || mUserName.compare(user_name) != 0)
		{
			mUserName = user_name;
			setTitle(user_name);
			setShortTitle(user_name);

			if (gSavedPerAccountSettings.getBOOL("FSllOwnerSayToScriptDebugWindow") && getKey().asUUID().isNull())
			{
				std::string url = chat.mURL;
				if ((url.empty()) || (std::string::npos == url.find("objectim")))
				{
					url = LLViewerChat::getSenderSLURL(chat, LLSD());
				}
				LLStyle::Params link_params(message_params);
				link_params.readonly_color(LLUIColorTable::instance().getColor("ChatNameObjectColor"));
				link_params.is_link = true;
				link_params.link_href = url;
				mHistoryEditor->appendText(chat.mFromName, (!mHistoryEditor->getText().empty()), link_params);
			}
		}
	}

	time_t utc_time;
	utc_time = time_corrected();
	std::string timeStr ="["+ LLTrans::getString("TimeHour")+"]:["+LLTrans::getString("TimeMin")+"]";
	LLSD substitution;
	substitution["datetime"] = (S32) utc_time;
	LLStringUtil::format (timeStr, substitution);
	
	mHistoryEditor->appendText("[" + timeStr + "] ", (!mHistoryEditor->getText().empty()), message_params.readonly_color(LLUIColorTable::instance().getColor("ChatTimestampColor")));

	if (mObjectID.isNull())
	{
		LLStyle::Params link_params(message_params);
		link_params.readonly_color(LLUIColorTable::instance().getColor("ChatNameObjectColor"));
		mHistoryEditor->appendText(chat.mFromName + ": ", false, link_params);
	}
	
	if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
	{
		mHistoryEditor->appendText(chat.mText, false, LLStyle::Params().readonly_color(LLUIColorTable::instance().getColor("ScriptErrorColor")));
	}
	else
	{
		mHistoryEditor->appendText(chat.mText, false, message_params.readonly_color(LLUIColorTable::instance().getColor("llOwnerSayChatColor")));
	}
	mHistoryEditor->blockUndo();
}

void LLFloaterScriptDebugOutput::clear()
{
	mHistoryEditor->clear();
}
// </FS:Kadah> [FSllOwnerSayToScriptDebugWindow]
