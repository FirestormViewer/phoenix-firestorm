/**
* @file LLIMProcessing.cpp
* @brief Container for Instant Messaging
*
* $LicenseInfo:firstyear=2001&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2018, Linden Research, Inc.
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

#include "llimprocessing.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llfirstuse.h"
#include "llfloaterreg.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaterimnearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
#include "llimview.h"
#include "llinventoryobserver.h"
#include "llinventorymodel.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llnotificationmanager.h"
#include "llpanelgroup.h"
#include "llregex.h"
#include "llregionhandle.h"
#include "llsdserialize.h"
#include "llslurl.h"
#include "llstring.h"
#include "lltoastnotifypanel.h"
#include "lltrans.h"
#include "llviewergenericmessage.h"
#include "llviewerobjectlist.h"
#include "llviewermessage.h"
#include "llviewerwindow.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llworld.h"

#include "boost/lexical_cast.hpp"

// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0a)
#include "rlvactions.h"
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "rlvui.h"
// [/RLVa:KB]

// Firestorm includes
#include "exogroupmutelist.h"
#include "fscommon.h"
#include "fsdata.h"
#include "fskeywords.h"
#include "llagentui.h"
#include "llavataractions.h"
#include "llgiveinventory.h"
#include "lllandmarkactions.h"
#include "llviewernetwork.h"
#include "sound_ids.h"
#include "NACLantispam.h"

extern void on_new_message(const LLSD& msg);

// Strip out "Resident" for display, but only if the message came from a user
// (rather than a script)
static std::string clean_name_from_im(const std::string& name, EInstantMessage type)
{
    switch (type)
    {
        case IM_NOTHING_SPECIAL:
        case IM_MESSAGEBOX:
        case IM_GROUP_INVITATION:
        case IM_INVENTORY_OFFERED:
        case IM_INVENTORY_ACCEPTED:
        case IM_INVENTORY_DECLINED:
        case IM_GROUP_VOTE:
        case IM_GROUP_MESSAGE_DEPRECATED:
            //IM_TASK_INVENTORY_OFFERED
            //IM_TASK_INVENTORY_ACCEPTED
            //IM_TASK_INVENTORY_DECLINED
        case IM_NEW_USER_DEFAULT:
        case IM_SESSION_INVITE:
        case IM_SESSION_P2P_INVITE:
        case IM_SESSION_GROUP_START:
        case IM_SESSION_CONFERENCE_START:
        case IM_SESSION_SEND:
        case IM_SESSION_LEAVE:
            //IM_FROM_TASK
        case IM_DO_NOT_DISTURB_AUTO_RESPONSE:
        case IM_CONSOLE_AND_CHAT_HISTORY:
        case IM_LURE_USER:
        case IM_LURE_ACCEPTED:
        case IM_LURE_DECLINED:
        case IM_GODLIKE_LURE_USER:
        case IM_TELEPORT_REQUEST:
        case IM_GROUP_ELECTION_DEPRECATED:
            //IM_GOTO_URL
            //IM_FROM_TASK_AS_ALERT
        case IM_GROUP_NOTICE:
        case IM_GROUP_NOTICE_INVENTORY_ACCEPTED:
        case IM_GROUP_NOTICE_INVENTORY_DECLINED:
        case IM_GROUP_INVITATION_ACCEPT:
        case IM_GROUP_INVITATION_DECLINE:
        case IM_GROUP_NOTICE_REQUESTED:
        case IM_FRIENDSHIP_OFFERED:
        case IM_FRIENDSHIP_ACCEPTED:
        case IM_FRIENDSHIP_DECLINED_DEPRECATED:
            //IM_TYPING_START
            //IM_TYPING_STOP
            return LLCacheName::cleanFullName(name);
        default:
            return name;
    }
}

static std::string clean_name_from_task_im(const std::string& msg,
    bool from_group)
{
    boost::smatch match;
    static const boost::regex returned_exp(
        "(.*been returned to your inventory lost and found folder by )(.+)( (from|near).*)");
    if (ll_regex_match(msg, match, returned_exp))
    {
        // match objects are 1-based for groups
        std::string final = match[1].str();
        std::string name = match[2].str();
        // Don't try to clean up group names
        if (!from_group)
        {
            final += LLCacheName::buildUsername(name);
        }
        final += match[3].str();
        return final;
    }
    return msg;
}

const std::string NOT_ONLINE_MSG("User not online - message will be stored and delivered later.");
const std::string NOT_ONLINE_INVENTORY("User not online - inventory has been saved.");
void translate_if_needed(std::string& message)
{
    if (message == NOT_ONLINE_MSG)
    {
        message = LLTrans::getString("not_online_msg");
    }
    else if (message == NOT_ONLINE_INVENTORY)
    {
        message = LLTrans::getString("not_online_inventory");
    }
}

class LLPostponedIMSystemTipNotification : public LLPostponedNotification
{
protected:
    /* virtual */
    void modifyNotificationParams()
    {
        LLSD payload = mParams.payload;
        // <FS:Ansariel> FIRE-29943: Item shared messaged logging to wrong IM logfile if user is offline
        //payload["SESSION_NAME"] = mName;
        LLAvatarName av_name;
        // This should work since modifyNotificationParams() is invoked after we already
        // retrieved the avatar name
        if (mFromId.notNull() && LLAvatarNameCache::instance().getName(mFromId, &av_name))
        {
            // LLHandlerUtil::logToIM() will transform this into the correct filename
            payload["SESSION_NAME"] = av_name.getLegacyName();
        }
        else
        {
            payload["SESSION_NAME"] = mName;
        }
        // </FS:Ansariel>

        mParams.payload = payload;
    }
};

class LLPostponedOfferNotification : public LLPostponedNotification
{
protected:
    /* virtual */
    void modifyNotificationParams()
    {
        LLSD substitutions = mParams.substitutions;
        substitutions["NAME"] = mName;
        mParams.substitutions = substitutions;
    }
};

void inventory_offer_handler(LLOfferInfo* info)
{
    // If muted, don't even go through the messaging stuff.  Just curtail the offer here.
    // Passing in a null UUID handles the case of where you have muted one of your own objects by_name.
    // The solution for STORM-1297 seems to handle the cases where the object is owned by someone else.
    if (LLMuteList::getInstance()->isMuted(info->mFromID, info->mFromName) ||
        LLMuteList::getInstance()->isMuted(LLUUID::null, info->mFromName))
    {
        info->forceResponse(IOR_MUTE);
        return;
    }


    bool bAutoAccept(false);
    // Avoid the Accept/Discard dialog if the user so desires. JC
    // <FS:Ansariel> Auto-accept any kind of inventory (FIRE-4128)
    //if (gSavedSettings.getBOOL("AutoAcceptNewInventory")
    //  && (info->mType == LLAssetType::AT_NOTECARD
    //      || info->mType == LLAssetType::AT_LANDMARK
    //      || info->mType == LLAssetType::AT_TEXTURE))
//  if (gSavedSettings.getBOOL("AutoAcceptNewInventory"))
    // </FS:Ansariel> Auto-accept any kind of inventory (FIRE-4128)
// [RLVa:KB]
    // Don't auto-accept give-to-RLV inventory offers
    if ( (gSavedSettings.getBOOL("AutoAcceptNewInventory")) &&
         ( (!rlv_handler_t::isEnabled()) || (!RlvInventory::instance().isGiveToRLVOffer(*info)) ) )
// [/RLVa:KB]
    {
        // For certain types, just accept the items into the inventory,
        // and possibly open them on receipt depending upon "ShowNewInventory".
        bAutoAccept = true;
    }

    // Strip any SLURL from the message display. (DEV-2754)
    std::string msg = info->mDesc;
    auto indx = msg.find(" ( http://slurl.com/secondlife/");
    if (indx == std::string::npos)
    {
        // try to find new slurl host
        indx = msg.find(" ( http://maps.secondlife.com/secondlife/");
    }
    if (indx >= 0)
    {
        LLStringUtil::truncate(msg, indx);
    }

    LLSD args;
    args["[OBJECTNAME]"] = msg;

    LLSD payload;

    // must protect against a NULL return from lookupHumanReadable()
    std::string typestr = ll_safe_string(LLAssetType::lookupHumanReadable(info->mType));
    if (!typestr.empty())
    {
        // human readable matches string name from strings.xml
        // lets get asset type localized name
        args["OBJECTTYPE"] = LLTrans::getString(typestr);
    }
    else
    {
        LL_WARNS("Messaging") << "LLAssetType::lookupHumanReadable() returned NULL - probably bad asset type: " << info->mType << LL_ENDL;
        args["OBJECTTYPE"] = "";

        // This seems safest, rather than propagating bogosity
        LL_WARNS("Messaging") << "Forcing an inventory-decline for probably-bad asset type." << LL_ENDL;
        info->forceResponse(IOR_DECLINE);
        return;
    }

    // If mObjectID is null then generate the object_id based on msg to prevent
    // multiple creation of chiclets for same object.
    LLUUID object_id = info->mObjectID;
    if (object_id.isNull())
        object_id.generate(msg);

    payload["from_id"] = info->mFromID;
    // Needed by LLScriptFloaterManager to bind original notification with
    // faked for toast one.
    payload["object_id"] = object_id;
    // Flag indicating that this notification is faked for toast.
    payload["give_inventory_notification"] = false;
    args["OBJECTFROMNAME"] = info->mFromName;
    args["NAME"] = info->mFromName;
    if (info->mFromGroup)
    {
        args["NAME_SLURL"] = LLSLURL("group", info->mFromID, "about").getSLURLString();
    }
    else
    {
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
        args["NAME_LABEL"] = LLSLURL("agent", info->mFromID, "completename").getSLURLString();
// [/SL:KB]
        args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "about").getSLURLString();
    }
    std::string verb = "select?name=" + LLURI::escape(msg);
    args["ITEM_SLURL"] = LLSLURL("inventory", info->mObjectID, verb.c_str()).getSLURLString();

    LLNotification::Params p;

    // Object -> Agent Inventory Offer
    if (info->mFromObject && !bAutoAccept)
    {
// [RLVa:KB] - Checked: RLVa-1.2.2
        // Only filter if the object owner is a nearby agent
        if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) && (RlvUtil::isNearbyAgent(info->mFromID)) )
        {
            payload["rlv_shownames"] = true;
            args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "rlvanonym").getSLURLString();
        }
// [/RLVa:KB]

        // Inventory Slurls don't currently work for non agent transfers, so only display the object name.
        args["ITEM_SLURL"] = msg;
        // Note: sets inventory_task_offer_callback as the callback
        p.substitutions(args).payload(payload).functor.responder(LLNotificationResponderPtr(info));
        info->mPersist = true;

        // Offers from your own objects need a special notification template.
        p.name = info->mFromID == gAgentID ? "OwnObjectGiveItem" : "ObjectGiveItem";

        // Pop up inv offer chiclet and let the user accept (keep), or reject (and silently delete) the inventory.
        LLPostponedNotification::add<LLPostponedOfferNotification>(p, info->mFromID, info->mFromGroup);
    }
    else // Agent -> Agent Inventory Offer
    {
// [RLVa:KB] - Checked: RLVa-2.0.1
        // Only filter if the offer is from a nearby agent and if there's no open IM session (doesn't necessarily have to be focused)
        bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
            (RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) || (!RlvUtil::isNearbyAgent(info->mFromID)) || (RlvUIEnabler::hasOpenIM(info->mFromID)) || (RlvUIEnabler::hasOpenProfile(info->mFromID));
        if (!fRlvCanShowName)
        {
            payload["rlv_shownames"] = true;
            LLAvatarName av_name;
            if (LLAvatarNameCache::get(info->mFromID, &av_name))
            {
                args["NAME"] = RlvStrings::getAnonym(av_name);
            }
            else
            {
                args["NAME"] = RlvStrings::getAnonym(info->mFromName);
            }
            args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "rlvanonym").getSLURLString();
        }
// [/RLVa:KB]

        p.responder = info;
        // Note: sets inventory_offer_callback as the callback
        // *TODO fix memory leak
        // inventory_offer_callback() is not invoked if user received notification and
        // closes viewer(without responding the notification)
        p.substitutions(args).payload(payload).functor.responder(LLNotificationResponderPtr(info));
        info->mPersist = true;
        // <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
        //p.name = "UserGiveItem";
        p.name = (gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages") ? "UserGiveItemLegacy" : "UserGiveItem");
        // </FS:Ansariel>
        p.offer_from_agent = true;

        // Prefetch the item into your local inventory.
        LLInventoryFetchItemsObserver* fetch_item = new LLInventoryFetchItemsObserver(info->mObjectID);
        fetch_item->startFetch();
        if(fetch_item->isFinished())
        {
            fetch_item->done();
        }
        else
        {
            gInventory.addObserver(fetch_item);
        }

        // In viewer 2 we're now auto receiving inventory offers and messaging as such (not sending reject messages).
        // <FS:Ansariel> Optional V1-like inventory accept messages
        //info->send_auto_receive_response();
        // Also needs to be send on auto-accept so the item gets into the inventory!
        if (bAutoAccept || !gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages"))
        {
            info->send_auto_receive_response();
        }
        // </FS:Ansariel> Optional V1-like inventory accept messages

        if (gAgent.isDoNotDisturb())
        {
            send_do_not_disturb_message(gMessageSystem, info->mFromID);
        }

        if (!bAutoAccept) // if we auto accept, do not pester the user
        {
            // Inform user that there is a script floater via toast system
            payload["give_inventory_notification"] = true;
            p.payload = payload;
            LLPostponedNotification::add<LLPostponedOfferNotification>(p, info->mFromID, false);
        }
        // <FS:Ansariel> FIRE-19540: Log auto-accepted inventory to nearby chat
        else if (gSavedSettings.getBOOL("FSLogAutoAcceptInventoryToChat") || gSavedSettings.getBOOL("FSShowAutoAcceptInventoryInNotifications"))
        {
            std::string message_type;
            LLStringUtil::format_map_t chat_args;

            chat_args["OBJECT_TYPE"] = (!typestr.empty() ? LLTrans::getString(typestr) : "");
            chat_args["DESC"] = msg;

            if (info->mFromObject)
            {
                std::string str_pos;
                std::string::size_type idx_start = info->mDesc.rfind(" ( http://");
                std::string::size_type idx_end = info->mDesc.find(" )", idx_start);
                if (idx_start != std::string::npos && idx_end != std::string::npos)
                {
                    LLSLURL url_pos(info->mDesc.substr(idx_start + 3, idx_end - (idx_start + 3)));
                    str_pos = "&slurl=" + LLURI::escape(url_pos.getLocationString());
                }

                chat_args["OBJECT_NAME"] = "[" + LLSLURL("objectim", info->mObjectID, "").getSLURLString() + "?name=" + LLURI::escape(info->mFromName) + "&owner=" + info->mFromID.asString() + (info->mFromGroup ? "&groupowned=true" : "") + str_pos + " " + info->mFromName + "]";
                message_type = "InvOfferAutoAcceptObject";
            }
            else
            {
                bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
                    (RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) || (!RlvUtil::isNearbyAgent(info->mFromID)) || (RlvUIEnabler::hasOpenIM(info->mFromID)) || (RlvUIEnabler::hasOpenProfile(info->mFromID));

                std::string name_slurl = LLSLURL("agent", info->mFromID, (fRlvCanShowName ? "inspect" : "rlvanonym")).getSLURLString();

                chat_args["USER_NAME"] = name_slurl;
                message_type = "InvOfferAutoAcceptUser";
            }

            std::string inv_offer_message = LLTrans::getString(message_type, chat_args);
            if (gSavedSettings.getBOOL("FSLogAutoAcceptInventoryToChat"))
            {
                FSCommon::report_to_nearby_chat(inv_offer_message);
            }
            if (gSavedSettings.getBOOL("FSShowAutoAcceptInventoryInNotifications"))
            {
                LLSD args;
                args["MESSAGE"] = inv_offer_message;
                LLNotificationsUtil::add("IMSystemMessageTip", args);
            }

            make_ui_sound("UISndInventoryOffer");
        }
        // </FS:Ansariel>

        // <FS:Ansariel> Show offered inventory also if auto-accept is enabled (FIRE-5101)
        if (bAutoAccept && gSavedSettings.getBOOL("ShowNewInventory"))
        {
            LLViewerInventoryCategory* catp = NULL;
            catp = (LLViewerInventoryCategory*)gInventory.getCategory(info->mObjectID);
            LLViewerInventoryItem* itemp = NULL;
            if(!catp)
            {
                itemp = (LLViewerInventoryItem*)gInventory.getItem(info->mObjectID);
            }

            LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(info->mObjectID, info->mFromName, false);
            open_agent_offer->startFetch();
            if(catp || (itemp && itemp->isFinished()))
            {
                open_agent_offer->done();
            }
            else
            {
                gInventory.addObserver(open_agent_offer);
            }
        }
        // </FS:Ansariel> Show offered inventory also if auto-accept is enabled (FIRE-5101)
    }

    LLFirstUse::newInventory();
}

// Callback for name resolution of a god/estate message
static void god_message_name_cb(const LLAvatarName& av_name, LLChat chat, std::string message)
{
    LLSD args;
    args["NAME"] = av_name.getCompleteName();
    args["MESSAGE"] = message;
    LLNotificationsUtil::add("GodMessage", args);

    // Treat like a system message and put in chat history.
    chat.mSourceType = CHAT_SOURCE_SYSTEM;
    chat.mText = message;

    // <FS:Ansariel> [FS communication UI]
    //LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
    FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
    // </FS:Ansariel> [FS communication UI]
    if (nearby_chat)
    {
        nearby_chat->addMessage(chat);
    }
}

// <FS:Ansariel> FIRE-505: Group name not shown in notification well
static void notification_group_name_cb(const std::string& group_name,
                                        const std::string& sender,
                                        const std::string& subject,
                                        const std::string& message,
                                        const LLSD& payload,
                                        U32 timestamp)
{
    LLAvatarName av_name;
    av_name.fromString(sender);
    LLSD args;
    args["SENDER"] = av_name.getUserNameForDisplay();
    args["GROUP"] = group_name;
    args["SUBJECT"] = subject;
    args["MESSAGE"] = message;
    LLDate notice_date = LLDate(timestamp).notNull() ? LLDate(timestamp) : LLDate::now();
    LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).time_stamp(notice_date));
    make_ui_sound("UISndGroupNotice"); // <FS:PP> Group notice sound
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-6786: Always show teleport location in teleport offer
static void teleport_region_info_cb(const std::string& slurl, LLSD args, const LLSD& payload, const LLUUID& from_id, const LLUUID& session_id, bool can_user_access_dst_region, bool does_user_require_maturity_increase)
{
    if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
    {
        args["POS_SLURL"] = RlvStrings::getString(RlvStringKeys::Hidden::Generic);
    }
    else
    {
        args["POS_SLURL"] = slurl;
    }

    LLNotification::Params params;

    if (!can_user_access_dst_region)
    {
        params.name = "TeleportOffered_MaturityBlocked_SLUrl";
        send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
        send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
    }
    else if (does_user_require_maturity_increase)
    {
        params.name = "TeleportOffered_MaturityExceeded_SLUrl";
    }
    else
    {
        params.name = "TeleportOffered_SLUrl";
        params.functor.name = "TeleportOffered";
    }

    params.substitutions = args;
    params.payload = payload;
    LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);

    LLWindow* viewer_window = gViewerWindow->getWindow();
    static LLCachedControl<bool> sFlashIcon(gSavedSettings, "FSFlashOnMessage");
    if (viewer_window && sFlashIcon)
    {
        viewer_window->flashIcon(5.f);
    }
}
// </FS:Ansariel>

static bool parse_lure_bucket(const std::string& bucket,
    U64& region_handle,
    LLVector3& pos,
    LLVector3& look_at,
    U8& region_access)
{
    // tokenize the bucket
    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
    boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
    tokenizer tokens(bucket, sep);
    tokenizer::iterator iter = tokens.begin();

    S32 gx, gy, rx, ry, rz, lx, ly, lz;
    try
    {
        gx = boost::lexical_cast<S32>((*(iter)).c_str());
        gy = boost::lexical_cast<S32>((*(++iter)).c_str());
        rx = boost::lexical_cast<S32>((*(++iter)).c_str());
        ry = boost::lexical_cast<S32>((*(++iter)).c_str());
        rz = boost::lexical_cast<S32>((*(++iter)).c_str());
        lx = boost::lexical_cast<S32>((*(++iter)).c_str());
        ly = boost::lexical_cast<S32>((*(++iter)).c_str());
        lz = boost::lexical_cast<S32>((*(++iter)).c_str());
    }
    catch (boost::bad_lexical_cast&)
    {
        LL_WARNS("parse_lure_bucket")
            << "Couldn't parse lure bucket."
            << LL_ENDL;
        return false;
    }
    // Grab region access
    region_access = SIM_ACCESS_MIN;
    if (++iter != tokens.end())
    {
        std::string access_str((*iter).c_str());
        LLStringUtil::trim(access_str);
        if (access_str == "A")
        {
            region_access = SIM_ACCESS_ADULT;
        }
        else if (access_str == "M")
        {
            region_access = SIM_ACCESS_MATURE;
        }
        else if (access_str == "PG")
        {
            region_access = SIM_ACCESS_PG;
        }
    }

    pos.setVec((F32)rx, (F32)ry, (F32)rz);
    look_at.setVec((F32)lx, (F32)ly, (F32)lz);

    region_handle = to_region_handle(gx, gy);
    return true;
}

static void notification_display_name_callback(const LLUUID& id,
    const LLAvatarName& av_name,
    const std::string& name,
    LLSD& substitutions,
    const LLSD& payload)
{
    substitutions["NAME"] = av_name.getDisplayName();
    LLNotificationsUtil::add(name, substitutions, payload);
}

void LLIMProcessing::processNewMessage(LLUUID from_id,
    bool from_group,
    LLUUID to_id,
    U8 offline,
    EInstantMessage dialog, // U8
    LLUUID session_id,
    U32 timestamp,
    std::string agentName,
    std::string message,
    U32 parent_estate_id,
    LLUUID region_id,
    LLVector3 position,
    U8 *binary_bucket,
    S32 binary_bucket_size,
    LLHost &sender,
    LLSD metadata,
    LLUUID aux_id)
{
    LLChat chat;
    std::string buffer;
    std::string name = agentName;

    // NaCl - Antispam Registry
    if (dialog != IM_TYPING_START && dialog != IM_TYPING_STOP &&                                            // Typing notifications
        !(dialog == IM_NOTHING_SPECIAL && offline == IM_OFFLINE && from_id.notNull() && to_id.notNull()) && // Saved offline IMs
        !(dialog == IM_FROM_TASK && offline == IM_OFFLINE)                                                  // Saved offline IMs from objects
        )
    {
        if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_IM, from_id, ANTISPAM_SOURCE_AGENT))
        {
            return;
        }
    }
    // NaCl End

    // make sure that we don't have an empty or all-whitespace name
    LLStringUtil::trim(name);
    if (name.empty())
    {
        name = LLTrans::getString("Unnamed");
    }

    // Preserve the unaltered name for use in group notice mute checking.
    std::string original_name = name;

    // IDEVO convert new-style "Resident" names for display
    name = clean_name_from_im(name, dialog);

    bool is_do_not_disturb = gAgent.isDoNotDisturb();
    bool is_autorespond = gAgent.getAutorespond();
    bool is_autorespond_nonfriends = gAgent.getAutorespondNonFriends();
    // <FS:PP> FIRE-1245: Option to block/reject teleport offers
    bool is_rejecting_tp_offers = gAgent.getRejectTeleportOffers();
    static LLCachedControl<bool> FSDontRejectTeleportOffersFromFriends(gSavedPerAccountSettings, "FSDontRejectTeleportOffersFromFriends");
    // </FS:PP>
    bool is_rejecting_group_invites = gAgent.getRejectAllGroupInvites(); // <FS:PP> Option to block/reject all group invites
    bool is_rejecting_friendship_requests = gAgent.getRejectFriendshipRequests(); // <FS:PP> FIRE-15233: Automatic friendship request refusal
    bool is_autorespond_muted = gSavedPerAccountSettings.getBOOL("FSSendMutedAvatarResponse");
    bool is_muted = LLMuteList::getInstance()->isMuted(from_id, name, LLMute::flagTextChat)
        // object IMs contain sender object id in session_id (STORM-1209)
        || (dialog == IM_FROM_TASK && LLMuteList::getInstance()->isMuted(session_id));
    bool is_owned_by_me = false;
    bool is_friend = LLAvatarTracker::instance().getBuddyInfo(from_id) != NULL;
    static LLCachedControl<bool> accept_im_from_only_friend(gSavedPerAccountSettings, "VoiceCallsFriendsOnly");
    //bool is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
    //      LLMuteList::isLinden(name); <:FS:TM> Bear compile fix - is_linden not referenced

    // <FS:PP> FIRE-10500: Autoresponse for (Away)
    static LLCachedControl<bool> FSSendAwayAvatarResponse(gSavedPerAccountSettings, "FSSendAwayAvatarResponse");
    bool is_afk = gAgent.getAFK();
    // </FS:PP>

    /***
    * The simulator may have flagged this sender as a bot, if the viewer would like to display
    * the chat text in a different color or font, the below code is how the viewer can
    * tell if the sender is a bot.
    *-----------------------------------------------------
    bool is_bot = false;
    if (metadata.has("sender"))
    {   // The server has identified this sender as a bot.
        is_bot = metadata["sender"]["bot"].asBoolean();
    }
    *-----------------------------------------------------
    */

    std::string notice_name;
    LLSD notice_args;
    if (metadata.has("notice"))
    {   // The server has injected a notice into the IM conversation.
        // These will be things like bot notifications, etc.
        notice_name = metadata["notice"]["id"].asString();
        notice_args = metadata["notice"]["data"];
    }

    chat.mMuted = is_muted;
    chat.mFromID = from_id;
    chat.mFromName = name;
    chat.mSourceType = (from_id.isNull() || (name == std::string(SYSTEM_FROM))) ? CHAT_SOURCE_SYSTEM : CHAT_SOURCE_AGENT;

    if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
    { // Translate server message if required (MAINT-6109)
        translate_if_needed(message);
    }

    LLViewerObject *source = gObjectList.findObject(session_id); //Session ID is probably the wrong thing.
    if (source)
    {
        is_owned_by_me = source->permYouOwner();
    }

    // NaCl - Newline flood protection
    static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
    if (useAntiSpam && dialog != IM_GROUP_INVITATION)
    {
        bool doCheck = true;
        if (from_id.isNull() || gAgentID == from_id)
        {
            doCheck = false;
        }
        if (doCheck && is_owned_by_me)
        {
            doCheck = false;
        }
        if (doCheck && NACLAntiSpamRegistry::instance().checkNewlineFlood(ANTISPAM_QUEUE_IM, from_id, message))
        {
            return;
        }
    }
    // NaCl End

    std::string separator_string(": ");

    LLSD args;
    LLSD payload;
    LLNotification::Params params;

    switch (dialog)
    {
        case IM_CONSOLE_AND_CHAT_HISTORY:
            args["MESSAGE"] = message;
            payload["from_id"] = from_id;

            params.name = "IMSystemMessageTip";
            params.substitutions = args;
            params.payload = payload;
            LLPostponedNotification::add<LLPostponedIMSystemTipNotification>(params, from_id, false);
            break;

        case IM_NOTHING_SPECIAL:    // p2p IM
            // Don't show dialog, just do IM
            if (!gAgent.isGodlike()
                && gAgent.inPrelude()
                && to_id.isNull())
            {
                // do nothing -- don't distract newbies in
                // Prelude with global IMs
            }
// [RLVa:KB] - Checked: RLVa-2.1.0
            else if ( (RlvActions::isRlvEnabled()) && (offline == IM_ONLINE) && (!is_muted) && ((!accept_im_from_only_friend) || (is_friend)) &&
                      (message.length() > 3) && (RLV_CMD_PREFIX == message[0]) && (RlvHandler::instance().processIMQuery(from_id, message)) )
            {
                // Eat the message and do nothing
            }
// [/RLVa:KB]
//          else if (offline == IM_ONLINE
//                      && is_do_not_disturb
//                      && from_id.notNull() //not a system message
//                      && to_id.notNull()) //not global message
// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
            // <FS:Ansariel> Only send the busy reponse if either the sender is not
            //               muted OR the sender is muted and we explicitely want
            //               to inform him about that fact.
            else if (offline == IM_ONLINE
                        && (!accept_im_from_only_friend || is_friend)                                    // is friend or accept IMs from friend only disabled
                        && ((is_do_not_disturb && (!is_muted || (is_muted && !is_autorespond_muted))) || // do not disturb
                            (is_autorespond && !is_muted) ||                                             // autorespond everyone
                            (is_autorespond_nonfriends && !is_friend && !is_muted) ||                    // autorespond friends only
                            (is_afk && FSSendAwayAvatarResponse && !is_muted))                           // away
                        && from_id.notNull() //not a system message
                        && to_id.notNull() //not global message
                        && RlvActions::canReceiveIM(from_id))
    // [/RLVa:KB]
            {
                // <FS:Ansariel> Log autoresponse notification after initial message
                bool has_session = true;

                // <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
                // Session id will be null if avatar answers from offline IM via email
                std::string response;
                if (!gIMMgr->hasSession(session_id) && session_id.notNull())
                {
                // </FS:Ansariel>
                    // <FS:Ansariel> Log autoresponse notification after initial message
                    has_session = false;
                    // <FS:Ansariel> FS autoresponse feature
                    std::string my_name;
                    LLAgentUI::buildFullname(my_name);
                    if (is_do_not_disturb)
                    {
                        response = gSavedPerAccountSettings.getString("DoNotDisturbModeResponse");
                    }
                    else if (is_autorespond_nonfriends && !is_friend)
                    {
                        response = gSavedPerAccountSettings.getString("FSAutorespondNonFriendsResponse");
                    }
                    else if (is_autorespond)
                    {
                        response = gSavedPerAccountSettings.getString("FSAutorespondModeResponse");
                    }
                    // <FS:PP> FIRE-10500: Autoresponse for (Away)
                    else if (is_afk && FSSendAwayAvatarResponse)
                    {
                        response = gSavedPerAccountSettings.getString("FSAwayAvatarResponse");
                    }
                    // </FS:PP>
                    else
                    {
                        LL_WARNS() << "Unknown auto-response mode" << LL_ENDL;
                    }
                    pack_instant_message(
                        gMessageSystem,
                        gAgent.getID(),
                        false,
                        gAgent.getSessionID(),
                        from_id,
                        my_name,
                        response,
                        IM_ONLINE,
                        IM_DO_NOT_DISTURB_AUTO_RESPONSE,
                        session_id);
                    gAgent.sendReliableMessage();
                    // </FS:Ansariel> FS autoresponse feature
                // <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
                }
                // </FS:Ansariel>

                // <FS:Ansariel> checkfor and process reqinfo
                if (has_session)
                {
                    message = FSData::getInstance()->processRequestForInfo(from_id,message,name,session_id);
                }
                // </FS:Ansariel>

                // now store incoming IM in chat history

                buffer = message;

                LL_DEBUGS("Messaging") << "session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

                // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
                chat.mText = buffer;
                bool keyword_alert_performed = false;
                if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
                {
                    FSKeywords::notify(chat);
                    keyword_alert_performed = true;
                }
                // </FS:PP>

                // add to IM panel, but do not bother the user
                gIMMgr->addMessage(
                    session_id,
                    from_id,
                    name,
                    buffer,
                    IM_OFFLINE == offline,
                    LLStringUtil::null,
                    dialog,
                    parent_estate_id,
                    region_id,
                    position,
                    false,
                    timestamp,
                    LLUUID::null,
                    "",
                    false,
                    keyword_alert_performed);

                // <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
                //if (!gIMMgr->isDNDMessageSend(session_id))
                //{
                //  // return a standard "do not disturb" message, but only do it to online IM
                //  // (i.e. not other auto responses and not store-and-forward IM)
                //  send_do_not_disturb_message(msg, from_id, session_id);
                //  gIMMgr->setDNDMessageSent(session_id, true);
                //}
                // </FS:Ansariel>

                if (!has_session)
                {
                    // <FS:LO> Fire-5389 - "Autoresponse Sent" message added to Firestorm as was in Phoenix
                    LLStringUtil::format_map_t args;
                    args["MESSAGE"] = response;

                    gIMMgr->addMessage(
                        session_id,
                        gAgentID,
                        LLStringUtil::null, // Pass null value so no name gets prepended
                        LLTrans::getString("IM_autoresponse_sent", args),
                        false,
                        name,
                        IM_NOTHING_SPECIAL,
                        parent_estate_id,
                        region_id,
                        position,
                        false,
                        0,
                        LLUUID::null,
                        "",
                        true
                        );
                    // </FS:LO>

                    // <FS:Ansariel> Send inventory item on autoresponse
                    LLUUID item_id(gSavedPerAccountSettings.getString("FSAutoresponseItemUUID"));
                    if (item_id.notNull())
                    {
                        LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(gInventory.getItem(item_id));
                        if (item)
                        {
                            gIMMgr->addMessage(
                                    session_id,
                                    gAgentID,
                                    LLStringUtil::null, // Pass null value so no name gets prepended
                                    LLTrans::getString("IM_autoresponse_item_sent", LLSD().with("[ITEM_NAME]", item->getName())),
                                    false,
                                    name,
                                    IM_NOTHING_SPECIAL,
                                    parent_estate_id,
                                    region_id,
                                    position,
                                    false,
                                    0,
                                    LLUUID::null,
                                    "",
                                    true);
                            LLGiveInventory::doGiveInventoryItem(from_id, item, session_id);
                        }
                    }
                    // </FS:Ansariel>
                }
            }
            else if (from_id.isNull())
            {
                LLSD args;
                args["MESSAGE"] = message;
                LLNotificationsUtil::add("SystemMessage", args);
            }
            else if (to_id.isNull())
            {
                // Message to everyone from GOD, look up the fullname since
                // server always slams name to legacy names
                LLAvatarNameCache::get(from_id, boost::bind(god_message_name_cb, _2, chat, message));
            }
            else
            {
                // standard message, server may have injected a notice into the conversation.
                std::string saved;
                if (offline == IM_OFFLINE)
                {
                    LLStringUtil::format_map_t args;
                    args["[LONG_TIMESTAMP]"] = formatted_time(timestamp);
                    saved = LLTrans::getString("Saved_message", args);
                }
                buffer = saved + message;

                LL_DEBUGS("Messaging") << "session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

                bool mute_im = is_muted;
                if (accept_im_from_only_friend && !is_friend)
                {
                    if (!gIMMgr->isNonFriendSessionNotified(session_id))
                    {
                        // <FS:Ansariel> Disable this - doesn't make sense it will be skipped by LLIMMgr::addMessage() anyway
                        //std::string message = LLTrans::getString("IM_unblock_only_groups_friends");
                        //gIMMgr->addMessage(session_id, from_id, name, message, IM_OFFLINE == offline);
                        // </FS:Ansariel>
                        gIMMgr->addNotifiedNonFriendSessionID(session_id);
                    }

                    mute_im = true;
                }

// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
                // Don't block offline IMs, or IMs from Lindens
                if ( (rlv_handler_t::isEnabled()) && (offline != IM_OFFLINE) && (!RlvActions::canReceiveIM(from_id)) && (!LLMuteList::getInstance()->isLinden(original_name) ))
                {
                    if (!mute_im)
                        RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RlvStringKeys::Blocked::RecvImRemote), session_id);
                    message = RlvStrings::getString(RlvStringKeys::Blocked::RecvIm);
                }
// [/RLVa:KB]

                if (!mute_im)
                {
                    // checkfor and process reqinfo
                    message = FSData::getInstance()->processRequestForInfo(from_id, message, name, session_id);

                    // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
                    chat.mText = message;
                    bool keyword_alert_performed = false;
                    if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
                    {
                        FSKeywords::notify(chat);
                        keyword_alert_performed = true;
                    }
                    // </FS:PP>

                    buffer = saved + message;

                    bool region_message = false;
                    if (region_id.isNull())
                    {
                        LLViewerRegion* regionp = LLWorld::instance().getRegionFromID(from_id);
                        if (regionp)
                        {
                            region_message = true;
                        }
                    }

                    std::string real_name;

                    if (!notice_name.empty())
                    {   // The simulator has injected some sort of notice into the conversation.
                        // findString will only replace the contents of buffer if the notice_id is found.
                        LLTrans::findString(buffer, notice_name, notice_args);
                        real_name = SYSTEM_FROM;
                    }

                    gIMMgr->addMessage(session_id,
                        from_id,
                        name,
                        buffer,
                        IM_OFFLINE == offline,
                        LLStringUtil::null,
                        dialog,
                        parent_estate_id,
                        region_id,
                        position,
                        region_message,
                        timestamp,
                        LLUUID::null,
                        real_name,
                        false,
                        keyword_alert_performed);
                }
                else
                {
                    /*
                    EXT-5099
                    */
                    static LLCachedControl<bool> fsSendMutedAvatarResponse(gSavedPerAccountSettings, "FSSendMutedAvatarResponse");
                    if (fsSendMutedAvatarResponse && (!accept_im_from_only_friend || is_friend))
                    {
                        std::string my_name;
                        LLAgentUI::buildFullname(my_name);
                        std::string response = gSavedPerAccountSettings.getString("FSMutedAvatarResponse");
                        pack_instant_message(
                            gMessageSystem,
                            gAgent.getID(),
                            false,
                            gAgent.getSessionID(),
                            from_id,
                            my_name,
                            response,
                            IM_ONLINE,
                            IM_DO_NOT_DISTURB_AUTO_RESPONSE,
                            session_id);
                        gAgent.sendReliableMessage();
                    }

                    // <FS:Ansariel> Don't flash for muted IMs
                    return;
                }
            }
            break;

        case IM_TYPING_START:
        {
            gIMMgr->processIMTypingStart(from_id, dialog);
        }
        break;

        case IM_TYPING_STOP:
        {
            gIMMgr->processIMTypingStop(from_id, dialog);
        }
        break;

        case IM_MESSAGEBOX:
        {
            // This is a block, modeless dialog.
            args["MESSAGE"] = message;
            LLNotificationsUtil::add("SystemMessageTip", args);
        }
        break;
        case IM_GROUP_NOTICE:
        case IM_GROUP_NOTICE_REQUESTED:
        {
            LL_INFOS("Messaging") << "Received IM_GROUP_NOTICE message." << LL_ENDL;

            LLUUID agent_id;
            U8 has_inventory;
            U8 asset_type = 0;
            LLUUID group_id;
            std::string item_name;

            if (aux_id.notNull())
            {
                // aux_id contains group id, binary bucket contains name and asset type
                group_id = aux_id;
                has_inventory = binary_bucket_size > 1;
                from_group = true; // inaccurate value correction
                if (has_inventory)
                {
                    std::string str_bucket = ll_safe_string((char*)binary_bucket, binary_bucket_size);

                    typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
                    boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
                    tokenizer tokens(str_bucket, sep);
                    tokenizer::iterator iter = tokens.begin();

                    asset_type = (LLAssetType::EType)(atoi((*(iter++)).c_str()));
                    iter++; // wearable type if applicable, otherwise asset type
                    item_name = std::string((*(iter++)).c_str());
                }
            }
            else
            {
                // All info is in binary bucket, read it for more information.
                struct notice_bucket_header_t
                {
                    U8 has_inventory;
                    U8 asset_type;
                    LLUUID group_id;
                };
                struct notice_bucket_full_t
                {
                    struct notice_bucket_header_t header;
                    U8 item_name[DB_INV_ITEM_NAME_BUF_SIZE];
                }*notice_bin_bucket;

                // Make sure the binary bucket is big enough to hold the header
                // and a null terminated item name.
                if ((binary_bucket_size < (S32)((sizeof(notice_bucket_header_t) + sizeof(U8))))
                    || (binary_bucket[binary_bucket_size - 1] != '\0'))
                {
                    LL_WARNS("Messaging") << "Malformed group notice binary bucket" << LL_ENDL;
                    // <FS:Ansariel> Don't flash task icon
                    //break;
                    return;
                }

                notice_bin_bucket = (struct notice_bucket_full_t*) &binary_bucket[0];
                has_inventory = notice_bin_bucket->header.has_inventory;
                asset_type = notice_bin_bucket->header.asset_type;
                group_id = notice_bin_bucket->header.group_id;
                item_name = ll_safe_string((const char*)notice_bin_bucket->item_name);
            }

            if (group_id != from_id)
            {
                agent_id = from_id;
            }
            else
            {
                auto index = original_name.find(" Resident");
                if (index != std::string::npos)
                {
                    original_name = original_name.substr(0, index);
                }

                // The group notice packet does not have an AgentID.  Obtain one from the name cache.
                // If last name is "Resident" strip it out so the cache name lookup works.
                std::string legacy_name = gCacheName->buildLegacyName(original_name);
                agent_id = LLAvatarNameCache::getInstance()->findIdByName(legacy_name);

                if (agent_id.isNull())
                {
                    LL_WARNS("Messaging") << "buildLegacyName returned null while processing " << original_name << LL_ENDL;
                }
            }

            if (agent_id.notNull() && LLMuteList::getInstance()->isMuted(agent_id))
            {
                // <FS:Ansariel> Don't flash task icon
                //break;
                return;
            }

            // If there is inventory, give the user the inventory offer.
            LLOfferInfo* info = NULL;

            if (has_inventory)
            {
                info = new LLOfferInfo();

                info->mIM = dialog;
                info->mFromID = from_id;
                info->mFromGroup = from_group;
                info->mTransactionID = session_id;
                info->mType = (LLAssetType::EType) asset_type;
                info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));
                std::string from_name;

                // <FS:Ansariel> FIRE-17714: Make group notice attachment confirmation localizable
                //from_name += "A group member named ";
                //from_name += name;
                LLStringUtil::format_map_t args;
                args["NAME"] = name;
                from_name += LLTrans::getString("InvOfferGroupNoticeName", args);

                // <FS:Ansariel> FIRE-29677 / SL-13720 workaround
                if (LLUUID::validate(name))
                {
                    LLStringUtil::format_map_t args;
                    args["NAME"] = "%s";
                    std::string placeholder_from_name = LLTrans::getString("InvOfferGroupNoticeName", args);
                    from_name = name + "|" + placeholder_from_name;
                }
                // </FS:Ansariel>

                info->mFromName = from_name;
                info->mDesc = item_name;
                info->mHost = sender;
            }

            std::string str(message);

            // Tokenize the string.
            // TODO: Support escaped tokens ("||" -> "|")
            typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
            boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
            tokenizer tokens(str, sep);
            tokenizer::iterator iter = tokens.begin();

            std::string subj(*iter++);
            std::string mes(*iter++);

            // Send the notification down the new path.
            // For requested notices, we don't want to send the popups.
            if (dialog != IM_GROUP_NOTICE_REQUESTED)
            {
                payload["subject"] = subj;
                payload["message"] = mes;
                payload["sender_name"] = name;
                payload["sender_id"] = agent_id;
                payload["group_id"] = group_id;
                payload["inventory_name"] = item_name;
                payload["received_time"] = LLDate::now();
                if (info && info->asLLSD())
                {
                    payload["inventory_offer"] = info->asLLSD();
                }

                // <FS:Ansariel> FIRE-505: Group name not shown in notification well
                //LLSD args;
                //args["SUBJECT"] = subj;
                //args["MESSAGE"] = mes;
                //LLDate notice_date = LLDate(timestamp).notNull() ? LLDate(timestamp) : LLDate::now();
                //LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).time_stamp(notice_date));
                //make_ui_sound("UISndGroupNotice"); // <FS:PP> Group notice sound
                if (group_id.isNull())
                {
                    LL_WARNS() << "Received group notice with null id!" << LL_ENDL;
                }
                gCacheName->get(group_id, true, boost::bind(&notification_group_name_cb, _2, name, subj, mes, payload, timestamp));
                // </FS:Ansariel>
            }

            // Also send down the old path for now.
            if (IM_GROUP_NOTICE_REQUESTED == dialog)
            {

                LLPanelGroup::showNotice(subj, mes, group_id, has_inventory, item_name, info);
            }
            else
            {
                delete info;
            }
        }
        break;
        case IM_GROUP_INVITATION:
        {
            // <FS:Ansariel> FIRE-20385: Don't show group invitation for groups agent is already a member of
            if (gAgent.isInGroup(from_id) && !gSavedSettings.getBOOL("FSShowJoinedGroupInvitations"))
            {
                LL_INFOS("Messaging") << "Received group invitation for group " << from_id << " but we are already a member" << LL_ENDL;
                return;
            }
            // </FS:Ansariel>

            if (!is_muted)
            {
                // group is not blocked, but we still need to check agent that sent the invitation
                // and we have no agent's id
                // Note: server sends username "first.last".
                // <FS:Ansariel> Fix broken mute check for avatars without last name:
                //               Names without last names are stored with capital first letter
                //               in the mute list, but they arrive here completely in lower case.
                //               So we need to change the first letter to upper case in order to
                //               make the mute check actually work.
                //is_muted |= LLMuteList::getInstance()->isMuted(name);
                std::string check_name(name);
                if (!check_name.empty() && check_name.find('.') == std::string::npos)
                {
                    check_name[0] = toupper(check_name[0]);
                }
                is_muted |= LLMuteList::getInstance()->isMuted(check_name);
                // </FS:Ansariel>
            }
            if (is_do_not_disturb || is_muted)
            {
                send_do_not_disturb_message(gMessageSystem, from_id);
            }

            if (!is_muted)
            {
                LL_INFOS("Messaging") << "Received IM_GROUP_INVITATION message." << LL_ENDL;
                // Read the binary bucket for more information.
                struct invite_bucket_t
                {
                    S32 membership_fee;
                    LLUUID role_id;
                }*invite_bucket;

                // Make sure the binary bucket is the correct size.
                if (binary_bucket_size != sizeof(invite_bucket_t))
                {
                    LL_WARNS("Messaging") << "Malformed group invite binary bucket" << LL_ENDL;
                    // <FS:Ansariel> Don't flash task icon
                    //break;
                    return;
                }

                invite_bucket = (struct invite_bucket_t*) &binary_bucket[0];
                S32 membership_fee = ntohl(invite_bucket->membership_fee);

                LLSD payload;
                payload["transaction_id"] = session_id;
                payload["group_id"] = from_group ? from_id : aux_id;
                payload["name"] = name;
                payload["message"] = message;
                payload["fee"] = membership_fee;
                payload["use_offline_cap"] = session_id.isNull() && (offline == IM_OFFLINE);

                LLSD args;
                args["MESSAGE"] = message;
                // we shouldn't pass callback functor since it is registered in LLFunctorRegistration

                // <FS:PP> Option to block/reject all group invites
                // LLNotificationsUtil::add("JoinGroup", args, payload);
                if (is_rejecting_group_invites)
                {
                    LL_INFOS("Messaging") << "Group invite automatically rejected because of the user setting..." << LL_ENDL;
                    return;
                }
                else
                {
                    make_ui_sound("UISndGroupInvitation"); // <FS:PP> Group invitation sound
                    LLNotificationsUtil::add("JoinGroup", args, payload);
                }
                // </FS:PP>
            }
            // <FS:Ansariel> Don't flash task icon for group messages from muted senders
            else
            {
                return;
            }
            // </FS:Ansariel>
        }
        break;

        case IM_INVENTORY_OFFERED:
        case IM_TASK_INVENTORY_OFFERED:
            // Someone has offered us some inventory.
        {
            LLOfferInfo* info = new LLOfferInfo;
            if (IM_INVENTORY_OFFERED == dialog)
            {
                struct offer_agent_bucket_t
                {
                    S8      asset_type;
                    LLUUID  object_id;
                }*bucketp;

                if (sizeof(offer_agent_bucket_t) != binary_bucket_size)
                {
                    LL_WARNS("Messaging") << "Malformed inventory offer from agent" << LL_ENDL;
                    delete info;
                    // <FS:Ansariel> Don't flash task icon
                    //break;
                    return;
                }
                bucketp = (struct offer_agent_bucket_t*) &binary_bucket[0];
                info->mType = (LLAssetType::EType) bucketp->asset_type;
                info->mObjectID = bucketp->object_id;
                info->mFromObject = false;
            }
            else // IM_TASK_INVENTORY_OFFERED
            {
                if (sizeof(S8) == binary_bucket_size)
                {
                    info->mType = (LLAssetType::EType) binary_bucket[0];
                }
                else
                {
                    /*RIDER*/ // The previous version of the protocol returned the wrong binary bucket... we
                    // still might be able to figure out the type... even though the offer is not retrievable.

                    // Should be safe to remove once DRTSIM-451 fully deploys
                    std::string str_bucket(reinterpret_cast<char *>(binary_bucket));
                    std::string str_type(str_bucket.substr(0, str_bucket.find('|')));

                    std::stringstream type_convert(str_type);

                    S32 type;
                    type_convert >> type;

                    // We could try AT_UNKNOWN which would be more accurate, but that causes an auto decline
                    info->mType = static_cast<LLAssetType::EType>(type);
                    // Don't break in the case of a bad binary bucket.  Go ahead and show the
                    // accept/decline popup even though it will not do anything.
                    LL_WARNS("Messaging") << "Malformed inventory offer from object, type might be " << info->mType << LL_ENDL;
                }
                info->mObjectID = LLUUID::null;
                info->mFromObject = true;
            }

            info->mIM = dialog;
            info->mFromID = from_id;
            info->mFromGroup = from_group;
            info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));

            info->mTransactionID = session_id.notNull() ? session_id : aux_id;

            info->mFromName = name;
            info->mDesc = message;
            info->mHost = sender;
            //if (((is_do_not_disturb && !is_owned_by_me) || is_muted))
            if (is_muted)
            {
                // Prefetch the offered item so that it can be discarded by the appropriate observer. (EXT-4331)
                if (IM_INVENTORY_OFFERED == dialog)
                {
                    LLInventoryFetchItemsObserver* fetch_item = new LLInventoryFetchItemsObserver(info->mObjectID);
                    fetch_item->startFetch();
                    delete fetch_item;
                    // Same as closing window
                    info->forceResponse(IOR_DECLINE);
                }
                else
                {
                    info->forceResponse(IOR_MUTE);
                }
            }
            // old logic: busy mode must not affect interaction with objects (STORM-565)
            // new logic: inventory offers from in-world objects should be auto-declined (CHUI-519)
            else if (is_do_not_disturb && dialog == IM_TASK_INVENTORY_OFFERED)
            {
                // Until throttling is implemented, do not disturb mode should reject inventory instead of silently
                // accepting it.  SEE SL-39554
                info->forceResponse(IOR_DECLINE);
            }
            else
            {
                inventory_offer_handler(info);
            }
        }
        break;

        case IM_INVENTORY_ACCEPTED:
        {
//          args["NAME"] = LLSLURL("agent", from_id, "completename").getSLURLString();;
//          args["ORIGINAL_NAME"] = original_name;
// [RLVa:KB] - Checked: RLVa-1.2.2
            // Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
            LLAvatarName av_name;
            bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
                (RlvActions::canShowName(RlvActions::SNC_DEFAULT, from_id)) || (!RlvUtil::isNearbyAgent(from_id)) || (RlvUIEnabler::hasOpenProfile(from_id)) || (RlvUIEnabler::hasOpenIM(from_id));
            args["NAME"] = LLSLURL("agent", from_id, (fRlvCanShowName) ? "completename" : "rlvanonym").getSLURLString();;
            args["ORIGINAL_NAME"] = fRlvCanShowName ? original_name : (LLAvatarNameCache::get(from_id, &av_name) ? RlvStrings::getAnonym(av_name) : RlvStrings::getAnonym(original_name));
// [/RLVa:KB]
            LLSD payload;
            payload["from_id"] = from_id;
            // Passing the "SESSION_NAME" to use it for IM notification logging
            // in LLTipHandler::processNotification(). See STORM-941.
            payload["SESSION_NAME"] = name;
            LLNotificationsUtil::add("InventoryAccepted", args, payload);
            break;
        }
        case IM_INVENTORY_DECLINED:
        {
//          args["NAME"] = LLSLURL("agent", from_id, "completename").getSLURLString();;
// [RLVa:KB] - Checked: RLVa-1.2.2
            // Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
            bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
                (RlvActions::canShowName(RlvActions::SNC_DEFAULT, from_id)) || (!RlvUtil::isNearbyAgent(from_id)) || (RlvUIEnabler::hasOpenProfile(from_id)) || (RlvUIEnabler::hasOpenIM(from_id));
            args["NAME"] = LLSLURL("agent", from_id, (fRlvCanShowName) ? "completename" : "rlvanonym").getSLURLString();;
// [/RLVa:KB]
            LLSD payload;
            payload["from_id"] = from_id;
            LLNotificationsUtil::add("InventoryDeclined", args, payload);
            break;
        }
        // TODO: _DEPRECATED suffix as part of vote removal - DEV-24856
        case IM_GROUP_VOTE:
        {
            LL_WARNS("Messaging") << "Received IM: IM_GROUP_VOTE_DEPRECATED" << LL_ENDL;
        }
        break;

        case IM_GROUP_ELECTION_DEPRECATED:
        {
            LL_WARNS("Messaging") << "Received IM: IM_GROUP_ELECTION_DEPRECATED" << LL_ENDL;
        }
        break;

        case IM_FROM_TASK:
        {

            if (is_do_not_disturb && !is_owned_by_me)
            {
                return;
            }

            // <FS:PP> FIRE-6406: Feature to disable Object Return notification
            static LLCachedControl<bool> FSDisableReturnObjectNotification(gSavedSettings, "FSDisableReturnObjectNotification");
            if (FSDisableReturnObjectNotification)
            {
                if (message.find("been returned to your inventory") != -1)
                {
                    return;
                }
            }
            // </FS:PP>

            // Build a link to open the object IM info window.
            std::string location = ll_safe_string((char*)binary_bucket, binary_bucket_size - 1);

            if (session_id.notNull())
            {
                chat.mFromID = session_id;
            }
            else
            {
                // This message originated on a region without the updated code for task id and slurl information.
                // We just need a unique ID for this object that isn't the owner ID.
                // If it is the owner ID it will overwrite the style that contains the link to that owner's profile.
                // This isn't ideal - it will make 1 style for all objects owned by the the same person/group.
                // This works because the only thing we can really do in this case is show the owner name and link to their profile.
                chat.mFromID = from_id ^ gAgent.getSessionID();
            }

            chat.mSourceType = CHAT_SOURCE_OBJECT;
            chat.mChatType = CHAT_TYPE_IM;

            // To conclude that the source type of message is CHAT_SOURCE_SYSTEM it's not
            // enough to check only from name (i.e. fromName = "Second Life"). For example
            // source type of messages from objects called "Second Life" should not be CHAT_SOURCE_SYSTEM.
            bool chat_from_system = (SYSTEM_FROM == name) && region_id.isNull() && position.isNull();
            if (chat_from_system)
            {
                // System's UUID is NULL (fixes EXT-4766)
                chat.mFromID = LLUUID::null;
                chat.mSourceType = CHAT_SOURCE_SYSTEM;
            }

            // IDEVO Some messages have embedded resident names
            message = clean_name_from_task_im(message, from_group);

            LLSD query_string;
            query_string["owner"] = from_id;
// [RLVa:KB] - Checked: RLVa-1.2.0
            if (RlvActions::isRlvEnabled())
            {
                // NOTE: the chat message itself will be filtered in LLNearbyChatHandler::processChat()
                if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) && (!from_group) && (RlvUtil::isNearbyAgent(from_id)) )
                {
                    query_string["rlv_shownames"] = true;

                    RlvUtil::filterNames(name);
                    chat.mFromName = name;
                }
                if (!RlvActions::canShowLocation())
                {
                    std::string::size_type idxPos = location.find('/');
                    if ( (std::string::npos != idxPos) && (RlvUtil::isNearbyRegion(location.substr(0, idxPos))) )
                        location = RlvStrings::getString(RlvStringKeys::Hidden::Region);
                }
            }
// [/RLVa:KB]
            query_string["slurl"] = location;
            query_string["name"] = name;
            if (from_group)
            {
                query_string["groupowned"] = "true";
            }

//              chat.mURL = LLSLURL("objectim", session_id, "").getSLURLString();
// [SL:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
            chat.mURL = LLSLURL("objectim", session_id, LLURI::mapToQueryString(query_string)).getSLURLString();
// [/SL:KB]
            chat.mText = message;

            // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of Task IM)
            if (FSKeywords::getInstance()->chatContainsKeyword(chat, true))
            {
                FSKeywords::notify(chat);
            }
            // </FS:PP>

            // Note: lie to Nearby Chat, pretending that this is NOT an IM, because
            // IMs from obejcts don't open IM sessions.
            // <FS:Ansariel> [FS communication UI]
            //LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
            FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
            // </FS:Ansariel> [FS communication UI]
            if (!chat_from_system && nearby_chat)
            {
                chat.mOwnerID = from_id;
                LLSD args;
                args["slurl"] = location;

                // Look for IRC-style emotes here so object name formatting is correct
                // <FS:Ansariel> Consolidate IRC /me prefix checks
                //std::string prefix = message.substr(0, 4);
                //if (prefix == "/me " || prefix == "/me'")
                if (FSCommon::is_irc_me_prefix(message))
                // </FS:Ansariel>
                {
                    chat.mChatStyle = CHAT_STYLE_IRC;
                }

                LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
                if (message != "")
                {
                    LLSD msg_notify;
                    msg_notify["session_id"] = LLUUID();
                    msg_notify["from_id"] = chat.mFromID;
                    msg_notify["source_type"] = chat.mSourceType;
                    on_new_message(msg_notify);
                }
            }


            //Object IMs send with from name: 'Second Life' need to be displayed also in notification toasts (EXT-1590)
            if (!chat_from_system) break;

            LLSD substitutions;
            substitutions["NAME"] = name;
            substitutions["MSG"] = message;

            LLSD payload;
            payload["object_id"] = session_id;
            payload["owner_id"] = from_id;
            payload["from_id"] = from_id;
            payload["slurl"] = location;
            payload["name"] = name;

            if (from_group)
            {
                payload["group_owned"] = "true";
            }

            LLNotificationsUtil::add("ServerObjectMessage", substitutions, payload);
        }
        break;

        case IM_SESSION_SEND:       // ad-hoc or group IMs

            // Only show messages if we have a session open (which
            // should happen after you get an "invitation"
// [SL:KB] - Patch: Chat-GroupSnooze | Checked: 2012-06-16 (Catznip-3.3)
            //if ( !gIMMgr->hasSession(session_id) )
            if (!gIMMgr->hasSession(session_id) &&
                 (!gAgent.isInGroup(session_id) || LLAvatarActions::isBlocked(from_id) || (!exoGroupMuteList::instance().restoreDeferredGroupChat(session_id) && (!gIMMgr->checkSnoozeExpiration(session_id) || !gIMMgr->restoreSnoozedSession(session_id)) )))
// [/SL:KB]
            {
                return;
            }

            else if (offline == IM_ONLINE && is_do_not_disturb)
            {

                // return a standard "do not disturb" message, but only do it to online IM
                // (i.e. not other auto responses and not store-and-forward IM)
                if (!gIMMgr->hasSession(session_id))
                {
                    // if there is not a panel for this conversation (i.e. it is a new IM conversation
                    // initiated by the other party) then...
                    send_do_not_disturb_message(gMessageSystem, from_id, session_id);
                }

                // now store incoming IM in chat history

                buffer = message;

                LL_DEBUGS("Messaging") << "message in dnd; session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

                // add to IM panel, but do not bother the user
                gIMMgr->addMessage(
                    session_id,
                    from_id,
                    name,
                    buffer,
                    IM_OFFLINE == offline,
                    ll_safe_string((char*)binary_bucket),
                    IM_SESSION_INVITE,
                    parent_estate_id,
                    region_id,
                    position,
                    false,      // is_region_msg
                    timestamp);
            }
            else
            {

                // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
                chat.mText = message;
                bool keyword_alert_performed = false;
                if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
                {
                    FSKeywords::notify(chat);
                    keyword_alert_performed = true;
                }
                // </FS:PP>

                // standard message, not from system
                std::string saved;
                if (offline == IM_OFFLINE)
                {
                    saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
                }

                buffer = saved + message;

                LL_DEBUGS("Messaging") << "standard message session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

                gIMMgr->addMessage(
                    session_id,
                    from_id,
                    name,
                    buffer,
                    (IM_OFFLINE == offline),
                    ll_safe_string((char*)binary_bucket),   // session name
                    IM_SESSION_INVITE,
                    parent_estate_id,
                    region_id,
                    position,
                    false,
                    timestamp,
                    LLUUID::null,
                    "",
                    false,
                    keyword_alert_performed);
            }
            break;

        case IM_FROM_TASK_AS_ALERT:
            if (is_do_not_disturb && !is_owned_by_me)
            {
                return;
            }
            {
                // Construct a viewer alert for this message.
                args["NAME"] = name;
                args["MESSAGE"] = message;
                LLNotificationsUtil::add("ObjectMessage", args);
            }
            break;
        case IM_DO_NOT_DISTURB_AUTO_RESPONSE:
            if (is_muted)
            {
                LL_DEBUGS("Messaging") << "Ignoring do-not-disturb response from " << from_id << LL_ENDL;
                return;
            }
            else
            {
                // <FS:Ansariel> FIRE-12908: Add busy response indicator back to busy messages
                //gIMMgr->addMessage(session_id, from_id, name, message);
                buffer = llformat("(%s): %s", LLTrans::getString("BusyResponse").c_str(), message.c_str());
                gIMMgr->addMessage(session_id, from_id, name, buffer);
                // </FS:Ansariel>
            }
            break;

        case IM_LURE_USER:
        case IM_TELEPORT_REQUEST:
        {
// [RLVa:KB] - Checked: RLVa-1.4.9
            // If we auto-accept the offer/request then this will override DnD status (but we'll still let the other party know later)
            bool fRlvAutoAccept = (rlv_handler_t::isEnabled()) &&
                ( ((IM_LURE_USER == dialog) && (RlvActions::autoAcceptTeleportOffer(from_id))) ||
                  ((IM_TELEPORT_REQUEST == dialog) && (RlvActions::autoAcceptTeleportRequest(from_id))) );
// [/RLVa:KB]

            if (is_muted)
            {
                return;
            }

            // <FS:PP> FIRE-1245: Option to block/reject teleport offers
            //else if (gSavedPerAccountSettings.getBOOL("VoiceCallsFriendsOnly") && (LLAvatarTracker::instance().getBuddyInfo(from_id) == NULL))
            //{
            //  return;
            //}
            else if ( (is_rejecting_tp_offers && (!FSDontRejectTeleportOffersFromFriends || (FSDontRejectTeleportOffersFromFriends && !is_friend))) && (!fRlvAutoAccept) )
            {
                send_rejecting_tp_offers_message(gMessageSystem, from_id);
            }
            // </FS:PP>
            else
            {
                // <FS:Ansariel> FS autoresponse feature
                //if (is_do_not_disturb)
                //{
                //    send_do_not_disturb_message(gMessageSystem, from_id);
                //}
// [RLVa:KB] - Checked: RLVa-1.4.9
                if (!fRlvAutoAccept)
// [/RLVa:KB]
                {
                    std::string my_name;
                    std::string response;
                    LLAgentUI::buildFullname(my_name);
                    if (is_do_not_disturb)
                    {
                        response = gSavedPerAccountSettings.getString("DoNotDisturbModeResponse");
                    }
                    else if (is_autorespond_nonfriends && !is_friend)
                    {
                        response = gSavedPerAccountSettings.getString("FSAutorespondNonFriendsResponse");
                    }
                    else if (is_autorespond)
                    {
                        response = gSavedPerAccountSettings.getString("FSAutorespondModeResponse");
                    }
                    else if (is_afk && FSSendAwayAvatarResponse)
                    {
                        response = gSavedPerAccountSettings.getString("FSAwayAvatarResponse");
                    }

                    if (!response.empty())
                    {
                        pack_instant_message(
                            gMessageSystem,
                            gAgentID,
                            false,
                            gAgentSessionID,
                            from_id,
                            my_name,
                            response,
                            IM_ONLINE,
                            IM_DO_NOT_DISTURB_AUTO_RESPONSE,
                            LLUUID::null);
                        gAgent.sendReliableMessage();
                    }
                }
                // </FS:Ansariel> FS autoresponse feature

                LLVector3 pos, look_at;
                U64 region_handle(0);
                U8 region_access(SIM_ACCESS_MIN);
                std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
                std::string region_access_str = LLStringUtil::null;
                std::string region_access_icn = LLStringUtil::null;
                std::string region_access_lc = LLStringUtil::null;

                bool canUserAccessDstRegion = true;
                bool doesUserRequireMaturityIncrease = false;

                // Do not parse the (empty) lure bucket for TELEPORT_REQUEST
                if (IM_TELEPORT_REQUEST != dialog && parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
                {
                    region_access_str = LLViewerRegion::accessToString(region_access);
                    region_access_icn = LLViewerRegion::getAccessIcon(region_access);
                    region_access_lc = region_access_str;
                    LLStringUtil::toLower(region_access_lc);

                    if (!gAgent.isGodlike())
                    {
                        switch (region_access)
                        {
                            case SIM_ACCESS_MIN:
                            case SIM_ACCESS_PG:
                                break;
                            case SIM_ACCESS_MATURE:
                                if (gAgent.isTeen())
                                {
                                    canUserAccessDstRegion = false;
                                }
                                else if (gAgent.prefersPG())
                                {
                                    doesUserRequireMaturityIncrease = true;
                                }
                                break;
                            case SIM_ACCESS_ADULT:
                                if (!gAgent.isAdult())
                                {
                                    canUserAccessDstRegion = false;
                                }
                                else if (!gAgent.prefersAdult())
                                {
                                    doesUserRequireMaturityIncrease = true;
                                }
                                break;
                            default:
                                llassert(0);
                                break;
                        }
                    }
                }

// [RLVa:KB] - Checked: RLVa-1.4.9
                if (rlv_handler_t::isEnabled())
                {
                    if ( ((IM_LURE_USER == dialog) && (!RlvActions::canAcceptTpOffer(from_id))) ||
                         ((IM_TELEPORT_REQUEST == dialog) && (!RlvActions::canAcceptTpRequest(from_id))) )
                    {
                        RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RlvStringKeys::Blocked::TpLureRequestRemote));
                        if (is_do_not_disturb)
                            send_do_not_disturb_message(gMessageSystem, from_id);
                        return;
                    }

                    // Censor message if: 1) restricted from receiving IMs from the sender, or 2) teleport offer/request and @showloc=n restricted
                    if ( (!RlvActions::canReceiveIM(from_id)) ||
                         ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (IM_LURE_USER == dialog || IM_TELEPORT_REQUEST == dialog)) )
                    {
                        message = RlvStrings::getString(RlvStringKeys::Hidden::Generic);
                    }
                }
// [/RLVa:KB]

                LLSD args;
                // *TODO: Translate -> [FIRST] [LAST] (maybe)
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
                args["NAME_LABEL"] = LLSLURL("agent", from_id, "completename").getSLURLString();
// [/SL:KB]
                args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();
                args["MESSAGE"] = message;
                args["MATURITY_STR"] = region_access_str;
                args["MATURITY_ICON"] = region_access_icn;
                args["REGION_CONTENT_MATURITY"] = region_access_lc;
                LLSD payload;
                payload["from_id"] = from_id;
                payload["lure_id"] = session_id;
                payload["godlike"] = false;
                payload["region_maturity"] = region_access;

                // <FS:Ansariel> FIRE-6786: Always show teleport location in teleport offer
                if (dialog == IM_LURE_USER && (!rlv_handler_t::isEnabled() || !fRlvAutoAccept) && LLGridManager::instance().isInSecondLife())
                {
                    LLVector3d pos_global = from_region_handle(region_handle);
                    pos_global += LLVector3d(pos);
                    LLLandmarkActions::getSLURLfromPosGlobal(pos_global, boost::bind(&teleport_region_info_cb, _1, args, payload, from_id, session_id, canUserAccessDstRegion, doesUserRequireMaturityIncrease));
                    return;
                }
                // </FS:Ansariel>

                if (!canUserAccessDstRegion)
                {
                    LLNotification::Params params("TeleportOffered_MaturityBlocked");
                    params.substitutions = args;
                    params.payload = payload;
                    LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                    send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
                    send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
                }
                else if (doesUserRequireMaturityIncrease)
                {
                    LLNotification::Params params("TeleportOffered_MaturityExceeded");
                    params.substitutions = args;
                    params.payload = payload;
                    LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                }
                else
                {
                    LLNotification::Params params;
                    if (IM_LURE_USER == dialog)
                    {
                        params.name = "TeleportOffered";
                        params.functor.name = "TeleportOffered";
                    }
                    else if (IM_TELEPORT_REQUEST == dialog)
                    {
                        params.name = "TeleportRequest";
                        params.functor.name = "TeleportRequest";
                    }

                    params.substitutions = args;
                    params.payload = payload;

// [RLVa:KB] - Checked: RLVa-1.4.9
                    if (fRlvAutoAccept)
                    {
                        if (IM_LURE_USER == dialog)
                            gRlvHandler.setCanCancelTp(false);
                        if (is_do_not_disturb)
                            send_do_not_disturb_message(gMessageSystem, from_id);
                        LLNotifications::instance().forceResponse(LLNotification::Params(params.name).payload(payload), 0);
                    }
                    else
                    {
                        LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                    }
// [/RLVa:KB]
//                      LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                }
            }
        }
        break;

        case IM_GODLIKE_LURE_USER:
        {
            LLVector3 pos, look_at;
            U64 region_handle(0);
            U8 region_access(SIM_ACCESS_MIN);
            std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
            std::string region_access_str = LLStringUtil::null;
            std::string region_access_icn = LLStringUtil::null;
            std::string region_access_lc = LLStringUtil::null;

            bool canUserAccessDstRegion = true;
            bool doesUserRequireMaturityIncrease = false;

            if (parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
            {
                region_access_str = LLViewerRegion::accessToString(region_access);
                region_access_icn = LLViewerRegion::getAccessIcon(region_access);
                region_access_lc = region_access_str;
                LLStringUtil::toLower(region_access_lc);

                if (!gAgent.isGodlike())
                {
                    switch (region_access)
                    {
                        case SIM_ACCESS_MIN:
                        case SIM_ACCESS_PG:
                            break;
                        case SIM_ACCESS_MATURE:
                            if (gAgent.isTeen())
                            {
                                canUserAccessDstRegion = false;
                            }
                            else if (gAgent.prefersPG())
                            {
                                doesUserRequireMaturityIncrease = true;
                            }
                            break;
                        case SIM_ACCESS_ADULT:
                            if (!gAgent.isAdult())
                            {
                                canUserAccessDstRegion = false;
                            }
                            else if (!gAgent.prefersAdult())
                            {
                                doesUserRequireMaturityIncrease = true;
                            }
                            break;
                        default:
                            llassert(0);
                            break;
                    }
                }
            }

            LLSD args;
            // *TODO: Translate -> [FIRST] [LAST] (maybe)
            args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();
            args["MESSAGE"] = message;
            args["MATURITY_STR"] = region_access_str;
            args["MATURITY_ICON"] = region_access_icn;
            args["REGION_CONTENT_MATURITY"] = region_access_lc;
            LLSD payload;
            payload["from_id"] = from_id;
            payload["lure_id"] = session_id;
            payload["godlike"] = true;
            payload["region_maturity"] = region_access;

            if (!canUserAccessDstRegion)
            {
                LLNotification::Params params("TeleportOffered_MaturityBlocked");
                params.substitutions = args;
                params.payload = payload;
                LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
                send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
            }
            else if (doesUserRequireMaturityIncrease)
            {
                LLNotification::Params params("TeleportOffered_MaturityExceeded");
                params.substitutions = args;
                params.payload = payload;
                LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
            }
            else
            {
                // do not show a message box, because you're about to be
                // teleported.
                LLNotifications::instance().forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
            }
        }
        break;

        case IM_GOTO_URL:
        {
            LLSD args;
            // n.b. this is for URLs sent by the system, not for
            // URLs sent by scripts (i.e. llLoadURL)
            if (binary_bucket_size <= 0)
            {
                LL_WARNS("Messaging") << "bad binary_bucket_size: "
                    << binary_bucket_size
                    << " - aborting function." << LL_ENDL;
                return;
            }

            std::string url;

            url.assign((char*)binary_bucket, binary_bucket_size - 1);
            args["MESSAGE"] = message;
            args["URL"] = url;
            LLSD payload;
            payload["url"] = url;
            LLNotificationsUtil::add("GotoURL", args, payload);
        }
        break;

        case IM_FRIENDSHIP_OFFERED:
        {

            // <FS:PP> FIRE-15233: Automatic friendship request refusal
            if (is_rejecting_friendship_requests)
            {
                send_rejecting_friendship_requests_message(gMessageSystem, from_id);
                return;
            }
            // </FS:PP>

            LLSD payload;
            payload["from_id"] = from_id;
            payload["session_id"] = session_id;
            payload["online"] = (offline == IM_ONLINE);
            payload["sender"] = sender.getIPandPort();

            bool add_notification = true;
            for (auto& panel : LLToastNotifyPanel::instance_snapshot())
            {
                const std::string& notification_name = panel.getNotificationName();
                if (notification_name == "OfferFriendship" && panel.isControlPanelEnabled())
                {
                    add_notification = false;
                    break;
                }
            }

            if (is_muted && add_notification)
            {
                LLNotifications::instance().forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
            }
            else
            {
                if (is_do_not_disturb)
                {
                    send_do_not_disturb_message(gMessageSystem, from_id);
                }
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
                args["NAME_LABEL"] = LLSLURL("agent", from_id, "completename").getSLURLString();
// [/SL:KB]
                args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();

                if (add_notification)
                {
                    if (message.empty())
                    {
                        //support for frienship offers from clients before July 2008
                        LLNotificationsUtil::add("OfferFriendshipNoMessage", args, payload);
                        make_ui_sound("UISndFriendshipOffer"); // <FS:PP> Friendship offer sound
                    }
                    else
                    {
                        args["[MESSAGE]"] = message;
                        LLNotification::Params params("OfferFriendship");
                        params.substitutions = args;
                        params.payload = payload;
                        LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
                        make_ui_sound("UISndFriendshipOffer"); // <FS:PP> Friendship offer sound
                    }
                }
            }
        }
        break;

        case IM_FRIENDSHIP_ACCEPTED:
        {
            // In the case of an offline IM, the formFriendship() may be extraneous
            // as the database should already include the relationship.  But it
            // doesn't hurt for dupes.
            LLAvatarTracker::formFriendship(from_id);

            std::vector<std::string> strings;
            strings.push_back(from_id.asString());
            send_generic_message("requestonlinenotification", strings);

            args["NAME"] = name;
            LLSD payload;
            payload["from_id"] = from_id;
            LLAvatarNameCache::get(from_id, boost::bind(&notification_display_name_callback, _1, _2, "FriendshipAccepted", args, payload));
        }
        break;

        case IM_FRIENDSHIP_DECLINED_DEPRECATED:
        default:
            LL_WARNS("Messaging") << "Instant message calling for unknown dialog "
                << (S32)dialog << LL_ENDL;
            break;
    }

    LLWindow* viewer_window = gViewerWindow->getWindow();
    // <FS:CR> Make osx dashboard icon bounce when window isn't in focus
    //if (viewer_window && viewer_window->getMinimized())
    static LLCachedControl<bool> sFlashIcon(gSavedSettings, "FSFlashOnMessage");
    static LLCachedControl<bool> sFSFlashOnObjectIM(gSavedSettings, "FSFlashOnObjectIM");
    if (viewer_window && dialog != IM_TYPING_START && dialog != IM_TYPING_STOP && sFlashIcon && (sFSFlashOnObjectIM || (chat.mChatType != CHAT_TYPE_IM)) && !is_muted)
    {
        viewer_window->flashIcon(5.f);
    }
}

void LLIMProcessing::requestOfflineMessages()
{
    static bool requested = false;
    if (!requested
        && gMessageSystem
        && !gDisconnected
        && isAgentAvatarValid()
        && gAgent.getRegion()
        && gAgent.getRegion()->capabilitiesReceived()
        && (LLMuteList::getInstance()->isLoaded() || LLMuteList::getInstance()->getLoadFailed()))
    {
        std::string cap_url = gAgent.getRegionCapability("ReadOfflineMsgs");

        // <FS:Ansariel> Optional legacy offline messages
        if (!gSavedSettings.getBOOL("FSUseReadOfflineMsgsCap"))
        {
            cap_url = "";
        }
        // </FS:Ansariel>

        // Auto-accepted inventory items may require the avatar object
        // to build a correct name.  Likewise, inventory offers from
        // muted avatars require the mute list to properly mute.
        if (cap_url.empty()
            || gAgent.getRegionCapability("AcceptFriendship").empty()
            || gAgent.getRegionCapability("AcceptGroupInvite").empty())
        {
            // Offline messages capability provides no session/transaction ids for message AcceptFriendship and IM_GROUP_INVITATION to work
            // So make sure we have the caps before using it.
            requestOfflineMessagesLegacy();
        }
        else
        {
            LLCoros::instance().launch("LLIMProcessing::requestOfflineMessagesCoro",
                boost::bind(&LLIMProcessing::requestOfflineMessagesCoro, cap_url));
        }
        requested = true;
    }
}

void LLIMProcessing::requestOfflineMessagesCoro(std::string url)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
        httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("requestOfflineMessagesCoro", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);

    LLSD result = httpAdapter->getAndSuspend(httpRequest, url);

    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    if (!status) // success = httpResults["success"].asBoolean();
    {
        LL_WARNS("Messaging") << "Error requesting offline messages via capability " << url << ", Status: " << status.toString() << "\nFalling back to legacy method." << LL_ENDL;

        requestOfflineMessagesLegacy();
        return;
    }

    LLSD contents = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT];

    if (!contents.size())
    {
        LL_WARNS("Messaging") << "No contents received for offline messages via capability " << url << LL_ENDL;
        return;
    }

    // Todo: once dirtsim-369 releases, remove one of the map/array options
    LLSD messages;
    if (contents.isArray())
    {
        messages = *contents.beginArray();
    }
    else if (contents.has("messages"))
    {
        messages = contents["messages"];
    }
    else
    {
        LL_WARNS("Messaging") << "Invalid offline message content received via capability " << url << LL_ENDL;
        return;
    }

    if (!messages.isArray())
    {
        LL_WARNS("Messaging") << "Invalid offline message content received via capability " << url << LL_ENDL;
        return;
    }

    if (messages.size() == 0)
    {
        // Nothing to process
        return;
    }

    if (!gAgent.getRegion())
    {
        LL_WARNS("Messaging") << "Region null while attempting to load messages." << LL_ENDL;
        return;
    }

    LL_INFOS("Messaging") << "Processing offline messages." << LL_ENDL;

    LLHost sender = gAgent.getRegionHost();

    LLSD::array_iterator i = messages.beginArray();
    LLSD::array_iterator iEnd = messages.endArray();
    for (; i != iEnd; ++i)
    {
        const LLSD &message_data(*i);

        /* RIDER: Many fields in this message are using a '_' rather than the standard '-'.  This
         * should be changed but would require tight coordination with the simulator.
         */
        LLVector3 position;
        if (message_data.has("position"))
        {
            position.setValue(message_data["position"]);
        }
        else
        {
            position.set((F32)message_data["local_x"].asReal(), (F32)message_data["local_y"].asReal(), (F32)message_data["local_z"].asReal());
        }

        std::vector<U8> bin_bucket;
        if (message_data.has("binary_bucket"))
        {
            bin_bucket = message_data["binary_bucket"].asBinary();
        }
        else
        {
            bin_bucket.push_back(0);
        }

        // Todo: once drtsim-451 releases, remove the string option
        bool from_group;
        if (message_data["from_group"].isInteger())
        {
            from_group = message_data["from_group"].asInteger();
        }
        else
        {
            from_group = message_data["from_group"].asString() == "Y";
        }

        LLSD metadata;
        if (message_data.has("metadata"))
        {
            metadata = message_data["metadata"];
        }

        EInstantMessage dialog = static_cast<EInstantMessage>(message_data["dialog"].asInteger());
        LLUUID session_id = message_data["transaction-id"].asUUID();
        if (session_id.isNull() && dialog == IM_FROM_TASK)
        {
            session_id = message_data["asset_id"].asUUID();
        }

        LLAppViewer::instance()->postToMainCoro([=]()
            {
                std::vector<U8> local_bin_bucket = bin_bucket;
                LLHost local_sender = sender;
                LLIMProcessing::processNewMessage(
                    message_data["from_agent_id"].asUUID(),
                    from_group,
                    message_data["to_agent_id"].asUUID(),
                    message_data.has("offline") ? static_cast<U8>(message_data["offline"].asInteger()) : IM_OFFLINE,
                    dialog,
                    session_id,
                    static_cast<U32>(message_data["timestamp"].asInteger()),
                    message_data["from_agent_name"].asString(),
                    message_data["message"].asString(),
                    static_cast<U32>((message_data.has("parent_estate_id")) ? message_data["parent_estate_id"].asInteger() : 1), // 1 - IMMainland
                    message_data["region_id"].asUUID(),
                    position,
                    local_bin_bucket.data(),
                    S32(local_bin_bucket.size()),
                    local_sender,
                    metadata,
                    message_data["asset_id"].asUUID());
            });

    }
}

void LLIMProcessing::requestOfflineMessagesLegacy()
{
    LL_INFOS("Messaging") << "Requesting offline messages (Legacy)." << LL_ENDL;

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_RetrieveInstantMessages);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    gAgent.sendReliableMessage();
}

