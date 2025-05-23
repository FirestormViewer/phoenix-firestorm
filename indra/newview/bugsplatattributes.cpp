#include "linden_common.h"
#include "bugsplatattributes.h"
#include <filesystem>

std::string BugSplatAttributes::mCrashContextFileName;

BugSplatAttributes& BugSplatAttributes::instance()
{
    static BugSplatAttributes sInstance;
    return sInstance;
}


#include <string>
#include <cctype>

std::string BugSplatAttributes::to_xml_token(const std::string& input)
{
    // Example usage:
    // std::string token = to_xml_token("Bandwidth (kbit/s)");
    // The result should be: "Bandwidth_kbit_per_s"
    std::string result;
    result.reserve(input.size() * 2); // Reserve some space, since we might insert more chars for "/"

    for (char ch : input)
    {
        if (ch == '/')
        {
            // Replace '/' with "_per_"
            result.append("_per_");
        }
        else if (std::isalnum(ch) || ch == '_' || ch == '-')
        {
            // Letters, digits, underscore, and hyphen are okay
            result.push_back(ch);
        }
        else if (ch == ' ' || ch == '(' || ch == ')')
        {
            // Replace spaces and parentheses with underscore
            result.push_back('_');
        }
        else
        {
            // Other characters map to underscore
            result.push_back('_');
        }
    }

    // Ensure the first character is a letter or underscore:
    // If not, prepend underscore.
    if (result.empty() || !(std::isalpha(result.front()) || result.front() == '_'))
    {
        result.insert(result.begin(), '_');
    }
    // trim trailing underscores
    while (!result.empty() && result.back() == '_')
    {
        result.pop_back();
    }
    return result;
}



bool BugSplatAttributes::writeToFile(const std::string& file_path)
{
    LL_PROFILE_ZONE_SCOPED_CATEGORY_LOGGING;
    std::lock_guard<std::mutex> lock(mMutex);

    // Write to a temporary file first
    std::string tmp_file = file_path + ".tmp";
    {
        llofstream ofs(tmp_file.c_str(), std::ios::out | std::ios::trunc);
        if (!ofs.good())
        {
            return false;
        }

        ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        ofs << "<XmlCrashContext>\n";

        // First, write out attributes that have an empty category (top-level)
        auto empty_category_it = mAttributes.find("");
        if (empty_category_it != mAttributes.end())
        {
            for (const auto& kv : empty_category_it->second)
            {
                const std::string& key = kv.first;
                const std::string& val = kv.second;
                ofs << "    <" << key << ">" << val << "</" << key << ">\n";
            }
        }

        // Write out all other categories
        // BugSplat chaanged the XML format and there is no strict category support now. For now we'll prefix the category to each attribute
        for (const auto& cat_pair : mAttributes)
        {
            const std::string& category = cat_pair.first;

            // Skip empty category since we already handled it above
            if (category.empty())
            {
                continue;
            }

            for (const auto& kv : cat_pair.second)
            {
                const std::string& key = kv.first;
                const std::string& val = kv.second;
                ofs << "    <" << category << "-" << key << ">" << val << "</" << category << "-" << key << ">\n";
            }
        }

        ofs << "</XmlCrashContext>\n";

        // Close the file before renaming
        ofs.close();
    }

    // Rename the temp file to the final file. If rename fails, leave the old file intact.
    std::error_code ec;
    std::filesystem::rename(tmp_file, file_path, ec);
    if (ec)
    {
        // Rename failed, remove the temp file and return false
        std::filesystem::remove(tmp_file, ec);
        return false;
    }

    return true;
}
