#include "llviewerprecompiledheaders.h"
#include "blazingstorm/remote/bsremotefeatures.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "blazingstorm/remote/bsremotevalidation.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llappearancemgr.h"
#include "llavataractions.h"
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llviewerinventory.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "lltooldraganddrop.h"
#include "llviewermessage.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llvoavatarself.h"
#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvhandler.h"
#include "fscommon.h"
#include "message.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <sstream>
#include <set>

namespace BlazingStorm
{
    namespace
    {
        constexpr int PAGE_SIZE = 20;
        bool ownedInventory(const LLUUID& id)
        {
            const auto root = gInventory.getRootFolderID();
            const auto trash = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
            return id.notNull() && (id == root || gInventory.isObjectDescendentOf(id, root))
                && id != trash && !gInventory.isObjectDescendentOf(id, trash);
        }
        bool wearable(const LLViewerInventoryItem* item)
        {
            return item && (item->getType() == LLAssetType::AT_OBJECT
                || item->getType() == LLAssetType::AT_CLOTHING
                || item->getType() == LLAssetType::AT_BODYPART);
        }
        bool activeNotification(const LLNotificationPtr& n)
        {
            return n && !n->isCancelled() && !n->isRespondedTo();
        }
        bool teleportNotice(const LLNotificationPtr& n)
        {
            return activeNotification(n) && (n->getName() == "TeleportOffered"
                || n->getName() == "TeleportOffered_SLUrl" || n->getName() == "TeleportRequest")
                && !n->getPayload()["godlike"].asBoolean();
        }
        // Only the named native prompt created by this action may be answered.
        bool sendOffer(const LLUUID& target)
        {
            auto channel = LLNotifications::instance().getChannel("Unexpired");
            std::set<LLUUID> before;
            channel->forEachNotification([&](LLNotificationPtr n) { before.insert(n->getID()); });
            if (gAgent.isGodlike()) return false;
            handle_lure(target); // Native show-location and recipient restrictions.
            LLNotificationPtr prompt;
            channel->forEachNotification([&](LLNotificationPtr n)
            {
                if (n->getName() == "OfferTeleport" && activeNotification(n)
                    && !before.count(n->getID())
                    && n->getPayload()["ids"].size() == 1
                    && n->getPayload()["ids"][0].asUUID() == target) prompt = n;
            });
            if (!prompt) return false;
            auto response = prompt->getResponseTemplate(LLNotification::WITH_DEFAULT_BUTTON);
            response["message"] = "Teleport offered by my controller.";
            prompt->respond(response);
            return true;
        }
        bool rezCopy(LLViewerInventoryItem* item)
        {
            auto* region = gAgent.getRegion();
            if (!region || !item || !mayRezCopy(ownedInventory(item->getUUID()),
                item->getType() == LLAssetType::AT_OBJECT, item->isFinished(), item->getIsLinkType(),
                item->getPermissions().allowCopyBy(gAgent.getID()),
                RemoteSession::instance().hasPermission(RemotePermission::Inventory),
                gRlvHandler.hasBehaviour(RLV_BHVR_REZ) || gRlvHandler.hasBehaviour(RLV_BHVR_INTERACT))) return false;
            const LLVector3 start = region->getPosRegionFromGlobal(gAgent.getPositionGlobal());
            const LLVector3 end = start + gAgent.getAtAxis() * 2.f;
            // Same RezObject packing as the native drag/drop path. Never remove inventory.
            LLMessageSystem* msg = gMessageSystem;
            msg->newMessageFast(_PREHASH_RezObject);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
            msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
            msg->addUUIDFast(_PREHASH_GroupID, FSCommon::getGroupForRezzing());
            msg->nextBlock("RezData");
            msg->addUUIDFast(_PREHASH_FromTaskID, LLUUID::null);
            msg->addU8Fast(_PREHASH_BypassRaycast, 1);
            msg->addVector3Fast(_PREHASH_RayStart, start);
            msg->addVector3Fast(_PREHASH_RayEnd, end);
            msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
            msg->addBOOLFast(_PREHASH_RayEndIsIntersection, false);
            msg->addBOOLFast(_PREHASH_RezSelected, false);
            msg->addBOOLFast(_PREHASH_RemoveItem, false);
            pack_permissions_slam(msg, item->getFlags(), item->getPermissions());
            msg->nextBlockFast(_PREHASH_InventoryData);
            item->packMessage(msg);
            msg->sendReliable(region->getHost());
            return true;
        }
    }

    RemoteFeatures& RemoteFeatures::instance() { static RemoteFeatures f; return f; }
    void RemoteFeatures::reset()
    {
        mBrowsing = false; mFolder.setNull(); mPage = 0; mOffers.clear();
        mClient = LLSD(); mLastSnapshot.clear(); mResult.clear(); mNextSnapshot = {}; ++mRevision;
    }
    void RemoteFeatures::permissionsChanged()
    {
        if (!RemoteSession::instance().hasPermission(RemotePermission::Inventory)) mBrowsing = false;
        if (!RemoteSession::instance().hasPermission(RemotePermission::Teleport)) mOffers.clear();
        mNextSnapshot = {};
    }
    bool RemoteFeatures::dispatch(const RemoteCommand& command)
    {
        if (!RemoteSession::instance().isActive()) return false;
        bool ok = false;
        switch (command.type)
        {
            case RemoteCommandType::InventoryBrowse:
            case RemoteCommandType::InventoryWear:
            case RemoteCommandType::InventoryRemove:
            case RemoteCommandType::InventoryRez:
                ok = RemoteSession::instance().hasPermission(RemotePermission::Inventory)
                    && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV)
                    && inventoryCommand(command); break;
            default:
                ok = RemoteSession::instance().hasPermission(RemotePermission::Teleport)
                    && teleportCommand(command); break;
        }
        mResult = ok ? "Request submitted; simulator/appearance processing may still be pending."
                     : "Request rejected: check permissions, restrictions, selection, or inventory loading.";
        mNextSnapshot = {};
        if (ok && command.type != RemoteCommandType::InventoryBrowse)
            FSCommon::report_to_nearby_chat("[Blazing Storm] Controller submitted an inventory or teleport action.");
        return ok;
    }

    bool RemoteFeatures::inventoryCommand(const RemoteCommand& command)
    {
        LLUUID id;
        if (!command.targetId.empty() && !id.set(command.targetId, false)) return false;
        if (command.type == RemoteCommandType::InventoryBrowse)
        {
            if (id.isNull()) id = gInventory.getRootFolderID();
            int page = 0;
            if (!parseInventoryPage(command.text, page)) return false;
            if (!ownedInventory(id) || !gInventory.getCategory(id)) return false;
            mFolder = id; mPage = page; mBrowsing = true;
            LLInventoryModelBackgroundFetch::instance().start(id, false);
            return true;
        }
        if (!command.text.empty() || !ownedInventory(id) || !isAgentAvatarValid()) return false;
        auto* item = gInventory.getItem(id);
        if (item && item->getIsLinkType()) item = item->getLinkedItem();
        if (item && (!ownedInventory(item->getUUID()) || !item->isFinished())) return false;
        if (command.type == RemoteCommandType::InventoryRez) return rezCopy(item);
        uuid_vec_t ids;
        if (item) ids.push_back(item->getUUID());
        else if (gInventory.getCategory(id))
        {
            LLInventoryModel::cat_array_t cats;
            LLInventoryModel::item_array_t items;
            gInventory.collectDescendents(id, cats, items, LLInventoryModel::EXCLUDE_TRASH);
            if (!gInventory.isCategoryComplete(id))
            { LLInventoryModelBackgroundFetch::instance().start(id, true); return false; }
            for (const auto& cat : cats) if (!gInventory.isCategoryComplete(cat->getUUID()))
            { LLInventoryModelBackgroundFetch::instance().start(id, true); return false; }
            for (const auto& entry : items)
                if (wearable(entry)) ids.push_back(entry->getLinkedUUID());
        }
        if (ids.empty() || ids.size() > 256) return false;
        const bool wear = command.type == RemoteCommandType::InventoryWear;
        uuid_vec_t allowed;
        for (const auto& item_id : ids)
        {
            auto* candidate = gInventory.getItem(item_id);
            if (!ownedInventory(item_id) || !wearable(candidate) || !candidate->isFinished()) continue;
            if (!wear && candidate->getType() == LLAssetType::AT_BODYPART) continue;
            if (RlvActions::isRlvEnabled() && (wear
                ? !rlvPredCanWearItem(candidate, candidate->getType() == LLAssetType::AT_BODYPART ? RLV_WEAR_REPLACE : RLV_WEAR_ADD)
                : !rlvPredCanRemoveItem(candidate))) continue;
            allowed.push_back(item_id);
        }
        if (allowed.empty()) return false;
        if (wear) LLAppearanceMgr::instance().wearItemsOnAvatar(allowed, true, false);
        else LLAppearanceMgr::instance().removeItemsFromAvatar(allowed);
        return true;
    }

    LLSD RemoteFeatures::inventoryPage()
    {
        LLSD result;
        if (!mBrowsing || !ownedInventory(mFolder)) return result;
        auto* folder = gInventory.getCategory(mFolder);
        if (!folder) return result;
        result["folder"] = mFolder;
        result["parent"] = ownedInventory(folder->getParentUUID()) ? folder->getParentUUID() : mFolder;
        result["name"] = folder->getName().substr(0,128);
        result["page"] = mPage;
        result["loading"] = !gInventory.isCategoryComplete(mFolder);
        LLInventoryModel::cat_array_t* cats = nullptr;
        LLInventoryModel::item_array_t* items = nullptr;
        gInventory.getDirectDescendentsOf(mFolder, cats, items);
        std::vector<LLSD> rows;
        if (cats) for (const auto& cat : *cats) if (ownedInventory(cat->getUUID()))
        {
            LLSD row; row["id"] = cat->getUUID(); row["name"] = cat->getName().substr(0,128);
            row["folder"] = true; row["kind"] = "Folder / outfit"; rows.push_back(row);
        }
        if (items) for (const auto& item : *items)
        {
            LLSD row; row["id"] = item->getUUID(); row["name"] = item->getName().substr(0,128);
            row["folder"] = false;
            row["kind"] = LLAssetType::lookup(item->getType());
            const auto* actual = item->getIsLinkType() ? item->getLinkedItem() : item.get();
            row["copy"] = actual && actual->getPermissions().allowCopyBy(gAgent.getID());
            row["worn"] = LLAppearanceMgr::instance().isLinkedInCOF(item->getLinkedUUID());
            rows.push_back(row);
        }
        std::sort(rows.begin(), rows.end(), [](const LLSD& a, const LLSD& b)
        {
            if (a["folder"].asBoolean() != b["folder"].asBoolean()) return a["folder"].asBoolean();
            if (a["name"].asString() != b["name"].asString()) return a["name"].asString() < b["name"].asString();
            return a["id"].asUUID() < b["id"].asUUID();
        });
        mPage = std::min(mPage, rows.empty() ? 0 : static_cast<int>((rows.size()-1)/PAGE_SIZE));
        result["page"] = mPage; result["more"] = (mPage+1)*PAGE_SIZE < rows.size();
        result["rows"] = LLSD::emptyArray();
        for (int i = mPage*PAGE_SIZE; i < static_cast<int>(rows.size()) && i < (mPage+1)*PAGE_SIZE; ++i)
            result["rows"].append(rows[i]);
        return result;
    }

    LLSD RemoteFeatures::teleportOffers()
    {
        LLSD result = LLSD::emptyArray();
        const auto now = std::chrono::steady_clock::now();
        std::set<LLUUID> alive;
        LLNotifications::instance().getChannel("Unexpired")->forEachNotification([&](LLNotificationPtr n)
        {
            if (!teleportNotice(n)) return;
            const auto id = n->getID(); alive.insert(id);
            auto inserted = mOffers.emplace(id, now + std::chrono::minutes(2));
            if (now >= inserted.first->second || result.size() >= 20) return;
            LLSD row; row["id"] = id;
            const auto sender = n->getPayload()["from_id"].asUUID();
            row["from"] = RlvActions::canShowName(RlvActions::SNC_TELEPORTOFFER, sender)
                ? sender.asString() : "Hidden avatar";
            row["kind"] = n->getName() == "TeleportRequest" ? "Request to visit Subject" : "Offer to teleport Subject";
            result.append(row);
        });
        for (auto it = mOffers.begin(); it != mOffers.end(); )
            if (!alive.count(it->first)) it = mOffers.erase(it); else ++it;
        return result;
    }

    bool RemoteFeatures::teleportCommand(const RemoteCommand& command)
    {
        if (!gAgent.getRegion()) return false;
        if (command.type == RemoteCommandType::TeleportLocation)
        {
            // The Controller resolves locations; the Subject validates coordinates again.
            if (!command.targetId.empty() || command.text.size() > 120) return false;
            double x, y, z;
            if (!parseTeleportPosition(command.text, x, y, z)) return false;
            LLVector3d pos(x,y,z);
            if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE
                || (RlvActions::isLocalTp(pos) ? !RlvActions::canTeleportToLocal(pos) : !RlvActions::canTeleportToLocation())) return false;
            gAgent.teleportViaLocation(pos); return true;
        }
        LLUUID id;
        if (!id.set(command.targetId, false) || id.isNull() || !command.text.empty()) return false;
        if (command.type == RemoteCommandType::TeleportOffer) return id != gAgent.getID() && sendOffer(id);
        if (command.type == RemoteCommandType::TeleportRequest)
        {
            if (id == gAgent.getID() || !RlvActions::canAcceptTpOffer(id)
                || gSavedPerAccountSettings.getBOOL("FSRejectTeleportOffersMode")) return false;
            LLSD n; n["substitutions"]["uuid"] = id;
            // Native callback expects selected option 0; integer response is supported.
            LLAvatarActions::teleport_request_callback(n, LLSD(0)); return true;
        }
        if (command.type != RemoteCommandType::TeleportAccept && command.type != RemoteCommandType::TeleportDecline) return false;
        const auto found = mOffers.find(id);
        auto n = LLNotifications::instance().find(id);
        if (found == mOffers.end() || std::chrono::steady_clock::now() >= found->second || !teleportNotice(n)) return false;
        const bool accept = command.type == RemoteCommandType::TeleportAccept;
        const auto from = n->getPayload()["from_id"].asUUID();
        if (accept && (n->getName() == "TeleportRequest" ? !RlvActions::canAcceptTpRequest(from) : !RlvActions::canAcceptTpOffer(from))) return false;
        if (accept && n->getName() == "TeleportRequest" && gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) return false;
        if (accept && n->getName() != "TeleportRequest" && gAgent.getTeleportState() != LLAgent::TELEPORT_NONE) return false;
        LLSD response = n->getResponseTemplate();
        const auto form = n->getForm()->asLLSD();
        bool matched = false;
        for (LLSD::array_const_iterator it = form.beginArray(); it != form.endArray(); ++it)
            if ((*it)["type"].asString() == "button" && (*it)["index"].asInteger() == (accept ? 0 : 1))
            { response[(*it)["name"].asString()] = true; matched = true; break; }
        if (!matched) return false;
        mOffers.erase(found); n->respond(response); return true;
    }

    std::string RemoteFeatures::snapshot()
    {
        auto& session = RemoteSession::instance();
        const auto now = std::chrono::steady_clock::now();
        if (!session.isActive() || now < mNextSnapshot) return {};
        mNextSnapshot = now + std::chrono::seconds(1);
        LLSD data;
        data["inventory_allowed"] = session.hasPermission(RemotePermission::Inventory)
            && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV);
        data["teleport_allowed"] = session.hasPermission(RemotePermission::Teleport);
        data["inventory"] = data["inventory_allowed"].asBoolean() ? inventoryPage() : LLSD();
        data["offers"] = data["teleport_allowed"].asBoolean() ? teleportOffers() : LLSD::emptyArray();
        data["result"] = mResult;
        data["teleport_state"] = gAgent.getTeleportStateName();
        std::ostringstream out; LLSDSerialize::toXML(data, out);
        return out.str();
    }
    void RemoteFeatures::receiveSnapshot(const std::string& data)
    {
        if (data.size() > 28000 || data == mLastSnapshot) return;
        LLSD parsed; std::istringstream in(data);
        if (LLSDSerialize::fromXML(parsed, in) <= 0 || !parsed.isMap()
            || parsed["inventory"]["rows"].size() > PAGE_SIZE || parsed["offers"].size() > 20) return;
        mClient = parsed; mLastSnapshot = data; ++mRevision;
    }
}
