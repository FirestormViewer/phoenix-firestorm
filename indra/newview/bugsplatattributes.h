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
    void setAttribute(const std::string& key, const T& value, const std::string& category = "FS")
    {
        std::lock_guard<std::mutex> lock(mMutex);
        const auto& xml_key = to_xml_token(key);
        if constexpr (std::is_same_v<T, std::string>)
        {
            mAttributes[category][xml_key] = value;
        }
        else if constexpr (std::is_same_v<T, std::wstring>)
        {
            // Wide to narrow
            mAttributes[category][xml_key] = ll_convert_wide_to_string(value);
        }
        else if constexpr (std::is_same_v<T, const char*> || std::is_array_v<T>)
        {
            // Handle string literals and arrays (which includes string literals)
            mAttributes[category][xml_key] = std::string(value);
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            // Convert boolean to "true"/"false"
            mAttributes[category][xml_key] = value ? "true" : "false";
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            // Convert arithmetic types (int, float, double, etc.) to wstring
            mAttributes[category][xml_key] = std::to_string(value);
        }
        else
        {
            static_assert(!sizeof(T*), "No known conversion for this type to std::string.");
        }
    }
    // Returns true on success, false on failure.
    bool writeToFile(const std::string& file_path);
    const static std::string& getCrashContextFileName() { return mCrashContextFileName; }
    static void setCrashContextFileName(const std::string& file_name) { mCrashContextFileName = file_name; }
private:
    BugSplatAttributes() = default;
    ~BugSplatAttributes() = default;
    BugSplatAttributes(const BugSplatAttributes&) = delete;
    BugSplatAttributes& operator=(const BugSplatAttributes&) = delete;
    std::string to_xml_token(const std::string& input);
    // Internal structure to hold attributes by category
    // For example:
    //   mAttributes["RuntimeProperties"]["CrashGUID"] = "649F57B7EE6E92A0_0000"
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> mAttributes;
    std::mutex mMutex;
    static std::string mCrashContextFileName;
};

#endif // LL_BUGSPLAT
#endif // BUGSPLATATTRIBUTES_INCLUDED