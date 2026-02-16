 /**
 * @file fsnearbychatcontrol.h
 * @brief Nearby chat input control implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

#ifndef FS_NEARBYCHATCONTROL_H
#define FS_NEARBYCHATCONTROL_H

#include "llchatentry.h"
#include "fschatparticipants.h"
#include "rlvhandler.h"

class FSNearbyChatControl : public LLChatEntry, public FSChatParticipants
{
public:
    struct Params : public LLInitParam::Block<Params, LLChatEntry::Params>
    {
        Optional<bool>  is_default;
        Optional<S32>   text_pad_left;
        Optional<S32>   text_pad_right;

        Params()
            : is_default("default", false)
            , text_pad_left("text_pad_left", 0)
            , text_pad_right("text_pad_right", 0)
        {
        }
    };

    FSNearbyChatControl(const Params& p);
    ~FSNearbyChatControl();

    virtual void onFocusReceived();
    virtual void onFocusLost();
    virtual void setFocus(bool focus);
    virtual void draw();

    virtual bool handleKeyHere(KEY key, MASK mask);

    bool    isDefault() const { return mDefault; }

    void    setTextPadding(S32 left, S32 right);

    uuid_vec_t getSessionParticipants() const override;

private:
    // Typing in progress, expand gestures etc.
    void    onKeystroke(LLTextEditor* caller);

    void    applyTextPadding();

    // Unfocus and autohide chat bar accordingly if we are the default chat bar
    void    autohide();

    void    updateRlvRestrictions(ERlvBehaviour behavior);
    void    updateEmojiHelperSetting(const LLSD& data);

    bool    mDefault;
    S32     mTextPadLeft;
    S32     mTextPadRight;
    boost::signals2::connection mRlvBehaviorCallbackConnection;
    boost::signals2::connection mEmojiHelperSettingConnection;
};

#endif // FS_NEARBYCHATCONTROL_H
