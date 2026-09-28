/**
 * @file bstruststore.h
 * @brief Persistent trusted-controller permission profiles.
 */

#ifndef BS_TRUST_STORE_H
#define BS_TRUST_STORE_H

#include "blazingstorm/remote/bsremotesession.h"

#include <map>
#include <string>
#include <vector>

namespace BlazingStorm
{
    struct TrustedController
    {
        std::string avatarId;
        std::string avatarName;
        RemotePermissionMask permissions = 0;
    };

    class TrustStore final
    {
    public:
        static TrustStore& instance();

        const TrustedController* find(const std::string& avatar_id);
        std::vector<TrustedController> entries();

        void upsert(const TrustedController& entry);
        void remove(const std::string& avatar_id);

    private:
        TrustStore() = default;

        void ensureLoaded();
        void load();
        void save();

        bool mLoaded = false;
        std::map<std::string, TrustedController> mEntries;
    };
}

#endif // BS_TRUST_STORE_H
