#ifndef BUGSPLATATTRIBUTES_INCLUDED
#define BUGSPLATATTRIBUTES_INCLUDED
#ifdef LL_BUGSPLAT
// Currently on Windows BugSplat supports the new attributes file, but it is expected to be added to Mac and Linux soon.

#include <string>
#include <unordered_map>
#include <mutex>

class BugSplatAttributes
{
public:
    // Obtain the singleton instance
    static BugSplatAttributes& instance();

    template<typename T>
    void setAttribute(const std::wstring& key, const T& value, const std::wstring& category = L"FS")
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto& xml_key = to_xml_token(key);
        if constexpr (std::is_same_v<T, std::wstring>)
        {
            // Already wide string
            mAttributes[category][xml_key] = value;
        }
        else if constexpr (std::is_same_v<T, const wchar_t*> || std::is_array_v<T>)
        {
            // Handle wide string literals and arrays (which includes string literals)
            mAttributes[category][xml_key] = std::wstring(value);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            // Convert narrow to wide
            std::wstring wide_value(value.begin(), value.end());
            mAttributes[category][xml_key] = wide_value;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            // Convert boolean to "true"/"false"
            mAttributes[category][xml_key] = value ? L"true" : L"false";
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            // Convert arithmetic types (int, float, double, etc.) to wstring
            mAttributes[category][xml_key] = std::to_wstring(value);
        }
        else
        {
            static_assert(!sizeof(T*), "No known conversion for this type to std::wstring.");
        }
    }
    // Returns true on success, false on failure.
    bool writeToFile(const std::wstring& file_path);
    const static std::wstring& getCrashContextFileName() { return mCrashContextFileName; }
    static void setCrashContextFileName(const std::wstring& file_name) { mCrashContextFileName = file_name; }
private:
    BugSplatAttributes() = default;
    ~BugSplatAttributes() = default;
    BugSplatAttributes(const BugSplatAttributes&) = delete;
    BugSplatAttributes& operator=(const BugSplatAttributes&) = delete;
    std::wstring to_xml_token(const std::wstring& input);
    // Internal structure to hold attributes by category
    // For example:
    //   attributes_["RuntimeProperties"]["CrashGUID"] = L"649F57B7EE6E92A0_0000"
    std::unordered_map<std::wstring, std::unordered_map<std::wstring, std::wstring>> mAttributes;
    std::mutex mMutex;
    static std::wstring mCrashContextFileName;
};

#endif // LL_BUGSPLAT
#endif // BUGSPLATATTRIBUTES_INCLUDED