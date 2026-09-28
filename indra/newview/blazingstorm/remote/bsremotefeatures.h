#ifndef BS_REMOTE_FEATURES_H
#define BS_REMOTE_FEATURES_H

#include "blazingstorm/remote/bsremoteprotocol.h"
#include "llsd.h"
#include "lluuid.h"
#include <chrono>
#include <map>
#include <string>

namespace BlazingStorm
{
    // Inventory and teleport operations execute only against the Subject's state.
    class RemoteFeatures final
    {
    public:
        static RemoteFeatures& instance();
        bool dispatch(const RemoteCommand& command);
        void reset();
        void permissionsChanged();
        std::string snapshot();
        void receiveSnapshot(const std::string& data);
        const LLSD& inventory() const { return mClient["inventory"]; }
        const LLSD& offers() const { return mClient["offers"]; }
        const LLSD& state() const { return mClient; }
        unsigned revision() const { return mRevision; }
    private:
        bool inventoryCommand(const RemoteCommand& command);
        bool teleportCommand(const RemoteCommand& command);
        LLSD inventoryPage();
        LLSD teleportOffers();
        LLUUID mFolder;
        int mPage = 0;
        bool mBrowsing = false;
        std::string mResult;
        LLSD mClient;
        std::string mLastSnapshot;
        unsigned mRevision = 0;
        std::chrono::steady_clock::time_point mNextSnapshot{};
        std::map<LLUUID, std::chrono::steady_clock::time_point> mOffers;
    };
}
#endif
