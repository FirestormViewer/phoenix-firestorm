#ifndef LLVKKEYBINDINGS_H
#define LLVKKEYBINDINGS_H

#include "llsd.h"
#include "llkeybind.h"
#include <map>
#include <string>
#include <array>
#include <functional>
#include <filesystem>
#include <optional>

class LLVKKeyBindings final
{
public:
    enum class Mode { FirstPerson, ThirdPerson, EditAvatar, Sitting };
    struct Control
    {
        LLKeyBind binding;
        bool assignable = true;
        std::uint32_t conflicts = UINT32_MAX;
    };
    using Controls = std::map<std::string,Control>;
    using Reservation = std::function<bool(const LLKeyData&)>;
    void setControls(Mode mode, Controls controls);
    const Controls& controls(Mode mode) const { return mControls.at(static_cast<std::size_t>(mode)); }
    bool assign(Mode mode, const std::string& command, std::uint32_t index, const LLKeyData& data, const Reservation& reserved = {});
    bool clear(Mode mode, const std::string& command, std::uint32_t index);
    bool handles(Mode mode, const std::string& command, EMouseClickType mouse, KEY key, MASK mask) const;
    bool setClickAction(const std::string& command, EMouseClickType mouse, bool enabled);
    bool load(std::string_view xml, std::string& error);
    bool loadFile(const std::filesystem::path& path, std::string& error);
    std::optional<std::string> serialize(std::string& error) const;
    bool saveFile(const std::filesystem::path& path, std::string& error) const;
    static std::string label(const LLKeyData& data, const std::function<std::string(const std::string&)>& translate);
private:
    std::array<Controls,4> mControls;
};

#endif