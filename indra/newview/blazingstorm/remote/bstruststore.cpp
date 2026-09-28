/**
 * @file bstruststore.cpp
 * @brief Persistent trusted-controller permission profiles.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bstruststore.h"

#include "lldir.h"
#include "llfile.h"
#include "llsdserialize.h"

namespace
{
    const char* TRUST_FILE = "blazing_storm_trusted_controllers.xml";
}

namespace BlazingStorm
{
    TrustStore& TrustStore::instance()
    {
        static TrustStore store;
        return store;
    }

    void TrustStore::ensureLoaded()
    {
        if (!mLoaded)
        {
            load();
            mLoaded = true;
        }
    }

    void TrustStore::load()
    {
        mEntries.clear();

        const std::string filename =
            gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, TRUST_FILE);

        if (!LLFile::isfile(filename))
        {
            return;
        }

        llifstream file(filename.c_str());
        if (!file.is_open())
        {
            LL_WARNS("BlazingStorm") << "Unable to open trusted-controller file." << LL_ENDL;
            return;
        }

        LLSD data;
        LLSDSerialize::fromXMLDocument(data, file);
        file.close();

        if (!data.isMap())
        {
            return;
        }

        for (LLSD::map_const_iterator it = data.beginMap(); it != data.endMap(); ++it)
        {
            TrustedController entry;
            entry.avatarId = it->first;
            entry.avatarName = it->second["name"].asString();
            entry.permissions =
                static_cast<RemotePermissionMask>(it->second["permissions"].asInteger());

            // Persisted data still goes through the session allowlist when used.
            mEntries[entry.avatarId] = entry;
        }
    }

    void TrustStore::save()
    {
        const std::string filename =
            gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, TRUST_FILE);

        LLSD data(LLSD::emptyMap());
        for (const auto& pair : mEntries)
        {
            LLSD item;
            item["name"] = pair.second.avatarName;
            item["permissions"] = static_cast<S32>(pair.second.permissions);
            data[pair.first] = item;
        }

        llofstream file(filename.c_str());
        if (!file.is_open())
        {
            LL_WARNS("BlazingStorm") << "Unable to save trusted-controller file." << LL_ENDL;
            return;
        }

        LLSDSerialize::toPrettyXML(data, file);
        file.close();
    }

    const TrustedController* TrustStore::find(const std::string& avatar_id)
    {
        ensureLoaded();
        const auto found = mEntries.find(avatar_id);
        return found == mEntries.end() ? nullptr : &found->second;
    }

    std::vector<TrustedController> TrustStore::entries()
    {
        ensureLoaded();

        std::vector<TrustedController> result;
        result.reserve(mEntries.size());
        for (const auto& pair : mEntries)
        {
            result.push_back(pair.second);
        }
        return result;
    }

    void TrustStore::upsert(const TrustedController& entry)
    {
        ensureLoaded();
        if (entry.avatarId.empty())
        {
            return;
        }

        mEntries[entry.avatarId] = entry;
        save();
    }

    void TrustStore::remove(const std::string& avatar_id)
    {
        ensureLoaded();
        if (mEntries.erase(avatar_id) != 0)
        {
            save();
        }
    }
}
