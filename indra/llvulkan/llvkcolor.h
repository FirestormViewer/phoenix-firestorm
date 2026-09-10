#ifndef LLVKCOLOR_H
#define LLVKCOLOR_H

#include <array>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class LLVKColorTable;

class LLVKColor final
{
public:
    using Value = std::array<float,4>;
    LLVKColor() = default;
    LLVKColor(float red, float green, float blue, float alpha) : mLiteral{red,green,blue,alpha} {}
    LLVKColor(Value value) : mLiteral(value) {}
    const Value& get() const noexcept { return mReference ? mReference->value : mLiteral; }
    const float& operator[](std::size_t index) const { return get()[index]; }
    auto begin() const { return get().begin(); }
    auto end() const { return get().end(); }
    bool isReference() const noexcept { return bool(mReference); }
    bool operator==(const LLVKColor& other) const noexcept
    { return mReference || other.mReference ? mReference == other.mReference : mLiteral == other.mLiteral; }
private:
    friend class LLVKColorTable;
    struct Slot { Value value; };
    Value mLiteral{1,1,1,1};
    std::shared_ptr<const Slot> mReference;
};

class LLVKColorTable final
{
public:
    bool define(const std::string& name, LLVKColor::Value color);
    bool set(const std::string& name, LLVKColor::Value color);
    std::optional<LLVKColor> find(const std::string& name) const;
    bool isDefault(const std::string& name) const;
    bool resetToDefault(const std::string& name);
    void clear();
    enum class Layer { Loaded, User };
    bool load(std::string_view xml, Layer layer, std::vector<std::string>& warnings, std::string& error);
private:
    std::map<std::string,std::shared_ptr<LLVKColor::Slot>> mLoaded;
    std::map<std::string,std::shared_ptr<LLVKColor::Slot>> mUser;
};

#endif