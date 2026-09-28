#ifndef BS_REMOTE_VALIDATION_H
#define BS_REMOTE_VALIDATION_H
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>

namespace BlazingStorm
{
    inline bool parseInventoryPage(const std::string& text, int& page)
    {
        page = 0;
        if (text.empty()) return true;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), page);
        return parsed.ec == std::errc() && parsed.ptr == text.data() + text.size()
            && page >= 0 && page <= 100000;
    }
    inline bool parseTeleportPosition(const std::string& text, double& x, double& y, double& z)
    {
        if (text.empty() || text.size() > 120) return false;
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        char c1, c2;
        return (in >> x >> c1 >> y >> c2 >> z) && c1 == ',' && c2 == ','
            && (in >> std::ws).eof() && std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
            && x >= 0 && y >= 0 && x <= 4294967295. && y <= 4294967295. && z >= 0 && z <= 4096;
    }
    // Inventory entries and links can carry different permissions: call this
    // only with the resolved Subject-owned object's current permission result.
    inline bool mayRezCopy(bool inSubjectInventory, bool object, bool finished,
                           bool link, bool copyable, bool permitted, bool rlvBlocked)
    {
        return inSubjectInventory && object && finished && !link && copyable && permitted && !rlvBlocked;
    }
}
#endif
