/**
 * @file llfloaterconversationpreview.h
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#ifndef LLFLOATERCONVERSATIONPREVIEW_H_
#define LLFLOATERCONVERSATIONPREVIEW_H_

// <FS:CR> [FS communication UI]
//#include "llchathistory.h"
#include "fschathistory.h"
// </FS:CR> [FS communication UI]
#include "llfloater.h"

extern const std::string LL_FCP_COMPLETE_NAME;  //"complete_name"
extern const std::string LL_FCP_ACCOUNT_NAME;       //"user_name"

class LLSpinCtrl;

class LLFloaterConversationPreview : public LLFloater
{
public:

    LLFloaterConversationPreview(const LLSD& session_id);
    virtual ~LLFloaterConversationPreview();

    virtual BOOL postBuild();
    void setPages(std::list<LLSD>* messages,const std::string& file_name);

    virtual void draw();
    virtual void onOpen(const LLSD& key);
    virtual void onClose(bool app_quitting);
    // <FS:Ansariel> CTRL-F for search history
    virtual BOOL handleKeyHere(KEY key, MASK mask);
    virtual bool hasAccelerators() const { return true; }
    // </FS:Ansariel>

private:
    void onMoreHistoryBtnClick();
    void showHistory();
    void onBtnOpenExternal();   // <FS:CR> Open chat history externally
    void onClickSearch();   // [FS:CR] FIRE-6545

    LLMutex         mMutex;
    LLSpinCtrl*     mPageSpinner;
    // <FS:CR> [FS communication UI]
    //LLChatHistory*    mChatHistory;
    FSChatHistory*  mChatHistory;
    // </FS:CR> [FS communication UI]
    LLUUID          mSessionID;
    int             mCurrentPage;
    int             mPageSize;

    std::list<LLSD>*    mMessages;
    std::string     mAccountName;
    std::string     mCompleteName;
    std::string     mChatHistoryFileName;
    bool            mShowHistory;
    bool            mHistoryThreadsBusy;
    bool            mOpened;
    bool            mIsGroup;
};

#endif /* LLFLOATERCONVERSATIONPREVIEW_H_ */
