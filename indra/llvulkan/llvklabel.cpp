#include "llvklabel.h"
#include "llsd.h"
#include "llstring.h"

void LLVKLabel::assign(std::string text)
{
    mOriginal = std::move(text);
}

void LLVKLabel::assign(std::u32string text)
{
    const LLWString wide(text.begin(),text.end());
    mOriginal = wstring_to_utf8str(wide);
}

std::u32string LLVKLabel::resolveWide(const Context& context) const
{
    const auto text = utf8str_to_wstring(resolve(context));
    return std::u32string(text.begin(),text.end());
}

void LLVKLabel::setArgument(std::string key, std::string replacement)
{
    mArguments.insert_or_assign(std::move(key),std::move(replacement));
}

void LLVKLabel::setArguments(Arguments arguments)
{
    mArguments = std::move(arguments);
}

void LLVKLabel::clear()
{
    mOriginal.clear();
}

std::string LLVKLabel::resolve(const Context& context) const
{
    if (mOriginal.empty()) return {};
    LLStringUtil::format_map_t arguments;
    arguments.insert(context.defaults.begin(),context.defaults.end());
    arguments.insert(mArguments.begin(),mArguments.end());
    auto result = mOriginal;
    LLStringUtil::format(result,arguments);
    std::size_t position = 0;
    while ((position = result.find("L$",position)) != std::string::npos)
    {
        result.replace(position,2,context.currency);
        position += context.currency.size();
    }
    return result;
}