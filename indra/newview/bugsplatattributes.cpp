#include "bugsplatattributes.h"
#include "llstring.h"
#include <fstream>
#include <filesystem>

std::wstring BugSplatAttributes::mCrashContextFileName;

BugSplatAttributes& BugSplatAttributes::instance()
{
    static BugSplatAttributes sInstance;
    return sInstance;
}


#include <string>
#include <cctype>

std::wstring BugSplatAttributes::to_xml_token(const std::wstring& input)
{
    // Example usage:
    // std::wstring token = to_xml_token(L"Bandwidth (kbit/s)");
    // The result should be: L"Bandwidth_kbit_per_s"
    std::wstring result;
    result.reserve(input.size() * 2); // Reserve some space, since we might insert more chars for "/"

    for (wchar_t ch : input)
    {
        if (ch == L'/')
        {
            // Replace '/' with "_per_"
            result.append(L"_per_");
        }
        else if (std::iswalnum(ch) || ch == L'_' || ch == L'-')
        {
            // Letters, digits, underscore, and hyphen are okay
            result.push_back(ch);
        }
        else if (ch == L' ' || ch == L'(' || ch == L')')
        {
            // Replace spaces and parentheses with underscore
            result.push_back(L'_');
        }
        else
        {
            // Other characters map to underscore
            result.push_back(L'_');
        }
    }

    // Ensure the first character is a letter or underscore:
    // If not, prepend underscore.
    if (result.empty() || !(std::iswalpha(result.front()) || result.front() == L'_'))
    {
        result.insert(result.begin(), L'_');
    }
    // trim trailing underscores
    while (!result.empty() && result.back() == L'_')
    {
        result.pop_back();
    }
    return result;
}



bool BugSplatAttributes::writeToFile(const std::wstring& file_path)
{
    std::lock_guard<std::mutex> lock(mMutex);

    // Write to a temporary file first
    #ifdef LL_WINDOWS
    std::wstring tmp_file = file_path + L".tmp";
    #else // use non-wide characters for Linux and Mac
    std::string tmp_file = ll_convert_wide_to_string(file_path) + ".tmp";
    #endif

    {
    #ifdef LL_WINDOWS
        std::wofstream ofs(tmp_file, std::ios::out | std::ios::trunc);
    #else // use non-wide characters for Linux and Mac
        std::ofstream ofs(tmp_file, std::ios::out | std::ios::trunc);
    #endif
        if (!ofs.good())
        {
            return false;
        }

        ofs << L"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        ofs << L"<XmlCrashContext>\n";

        // First, write out attributes that have an empty category (top-level)
        auto empty_category_it = mAttributes.find(L"");
        if (empty_category_it != mAttributes.end())
        {
            for (const auto& kv : empty_category_it->second)
            {
                const std::wstring& key = kv.first;
                const std::wstring& val = kv.second;
                ofs << L"    <" << key << L">" << val << L"</" << key << L">\n";
            }
        }

        // Write out all other categories
        for (const auto& cat_pair : mAttributes)
        {
            const std::wstring& category = cat_pair.first;

            // Skip empty category since we already handled it above
            if (category.empty())
            {
                continue;
            }

            ofs << L"    <" << category << L">\n";
            for (const auto& kv : cat_pair.second)
            {
                const std::wstring& key = kv.first;
                const std::wstring& val = kv.second;
                ofs << L"        <" << key << L">" << val << L"</" << key << L">\n";
            }
            ofs << L"    </" << category << L">\n";
        }

        ofs << L"</XmlCrashContext>\n";

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
