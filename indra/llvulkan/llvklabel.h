#ifndef LLVKLABEL_H
#define LLVKLABEL_H

#include <map>
#include <string>

class LLVKLabel final
{
public:
    using Arguments = std::map<std::string,std::string>;
    struct Context
    {
        Arguments defaults;
        std::string currency = "L$";
    };
    void assign(std::string text);
    void assign(std::u32string text);
    void setArgument(std::string key, std::string replacement);
    void setArguments(Arguments arguments);
    void clear();
    std::string resolve(const Context& context) const;
    std::u32string resolveWide(const Context& context) const;
    const std::string& original() const noexcept { return mOriginal; }
private:
    std::string mOriginal;
    Arguments mArguments;
};

#endif