#include "llvkfontregistry.h"
#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <exception>
#include <fstream>
#include <tuple>
#include <sstream>

namespace
{
    struct Node
    {
        std::string name;
        std::map<std::string,std::string> attributes;
        std::string text;
        std::vector<std::unique_ptr<Node>> children;
        std::string attribute(const std::string& key) const
        {
            const auto found = attributes.find(key);
            return found == attributes.end() ? std::string() : found->second;
        }
    };
    struct Parser
    {
        XML_Parser parser = nullptr;
        std::unique_ptr<Node> root;
        std::vector<Node*> stack;
        std::exception_ptr failure;
        std::size_t nodes = 0;
        bool rejected = false;
        ~Parser() { if (parser) XML_ParserFree(parser); }
        template<class Operation> void invoke(Operation operation) noexcept
        {
            try { operation(); }
            catch (...) { failure = std::current_exception(); XML_StopParser(parser,XML_FALSE); }
        }
        static void XMLCALL start(void* pointer,const XML_Char* name,const XML_Char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.invoke([&]
            {
                if (++state.nodes > 10000 || state.stack.size() >= 64)
                { state.rejected = true; XML_StopParser(state.parser,XML_FALSE); return; }
                auto node = std::make_unique<Node>();
                node->name = name;
                for (std::size_t index = 0; attributes[index]; index += 2)
                    node->attributes.emplace(attributes[index],attributes[index+1]);
                auto* current = node.get();
                if (state.stack.empty()) state.root = std::move(node);
                else state.stack.back()->children.push_back(std::move(node));
                state.stack.push_back(current);
            });
        }
        static void XMLCALL end(void* pointer,const XML_Char*)
        {
            auto& state = *static_cast<Parser*>(pointer);
            if (!state.stack.empty()) state.stack.pop_back();
        }
        static void XMLCALL text(void* pointer,const XML_Char* text,int length)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.invoke([&] { if (!state.stack.empty()) state.stack.back()->text.append(text,length); });
        }
        static void XMLCALL doctype(void* pointer,const XML_Char*,const XML_Char*,const XML_Char*,int)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.rejected = true;
            XML_StopParser(state.parser,XML_FALSE);
        }
    };
    std::string contents(std::string value)
    {
        const auto first = value.find_first_not_of(" \t\n");
        if (first == std::string::npos) return {};
        if (value[first] != '"')
        {
            value = value.substr(first,value.find_last_not_of(" \t\n")-first+1);
            value.erase(std::remove(value.begin(),value.end(),'\r'),value.end());
            return value;
        }
        std::string result;
        std::size_t position = first;
        unsigned lines = 0;
        while (position != std::string::npos)
        {
            ++position;
            std::string line;
            while (position < value.size() && value[position] != '"')
            {
                if (value[position] == '\\') ++position;
                if (position < value.size()) line += value[position++];
            }
            if (position == value.size()) break;
            ++lines;
            result += line + '\n';
            position = value.find('"',position+1);
        }
        if (lines == 1) result.pop_back();
        return result;
    }
    template<class Number> bool number(const Node& node,const std::string& key,Number& output)
    {
        if (!node.attributes.contains(key)) return true;
        const auto value = contents(node.attribute(key));
        const auto parsed = std::from_chars(value.data(),value.data()+value.size(),output);
        return parsed.ec == std::errc() && parsed.ptr == value.data()+value.size();
    }
    std::string lower(std::string value)
    {
        for (auto& character : value) if (character >= 'A' && character <= 'Z') character += 'a'-'A';
        return value;
    }
}

struct LLVKFontRegistry::Impl
{
    struct File
    {
        std::string name;
        LLVKFontFace::Hinting hinting = LLVKFontFace::Hinting::ForceAutohint;
        int weight = -1;
        float sizeDelta = 0.f;
        bool bold = false;
        LLVKFont::FallbackPolicy policy = LLVKFont::FallbackPolicy::Unrestricted;
    };
    using Key = std::tuple<std::string,std::uint8_t,std::string>;
    Configuration configuration;
    std::vector<std::string> documents;
    std::map<Key,std::vector<File>> definitions;
    std::map<std::string,float> sizes;
    std::map<Request,std::pair<std::shared_ptr<LLVKFont>,std::string>> cache;
    std::mutex mutex;

    bool files(const Node& node,std::vector<File>& result,std::string& error)
    {
        for (const auto& child : node.children)
        {
            if (child->name == "os" && child->attribute("name") == configuration.platform)
            { if (!files(*child,result,error)) return false; }
            else if (child->name == "file")
            {
                File file;
                file.name = contents(child->text);
                if (file.name.empty() || !number(*child,"font_weight",file.weight) ||
                    !number(*child,"size_delta",file.sizeDelta) || !std::isfinite(file.sizeDelta))
                { error = "Invalid native font file declaration"; return false; }
                const auto hinting = lower(child->attribute("font_hinting"));
                if (hinting == "default") file.hinting = LLVKFontFace::Hinting::Default;
                else if (hinting == "no_hinting") file.hinting = LLVKFontFace::Hinting::DisableAutohint;
                file.bold = lower(child->attribute("flags")) == "bold";
                const auto predicate = child->attribute("functor");
                if (predicate == "is_emoji") file.policy = LLVKFont::FallbackPolicy::Emoji;
                else if (predicate == "is_emoji_use_color") file.policy = LLVKFont::FallbackPolicy::ColorEmoji;
                else if (predicate == "is_emoji_use_bw") file.policy = LLVKFont::FallbackPolicy::MonochromeEmoji;
                result.push_back(std::move(file));
            }
        }
        return true;
    }
};

LLVKFontRegistry::LLVKFontRegistry(std::unique_ptr<Impl> impl) : mImpl(std::move(impl)) {}
LLVKFontRegistry::~LLVKFontRegistry() = default;

std::string LLVKFontRegistry::diagnostics() const
{
    std::lock_guard lock(mImpl->mutex);
    std::ostringstream output;
    output << "Native font registry dump:\n";
    for (const auto& [name,size] : mImpl->sizes) output << "Size: " << name << " => " << size << '\n';
    for (const auto& [key,files] : mImpl->definitions)
    {
        const auto& [name,style,size]=key;
        output << "Font: name=" << name << " style=[" << static_cast<int>(style) << "] size=[" << size << "] fileNames=\n";
        for (const auto& file : files) output << "  file: " << file.name << '\n';
    }
    for (const auto& [request,cached] : mImpl->cache)
    {
        output << "Resolved: name=" << request.name << " size=[" << request.size << "] style=[" << static_cast<int>(request.style)
            << "] tabular=" << request.tabularNumbers << " glyphs=" << (cached.first ? cached.first->cachedGlyphCount() : 0) << '\n';
        if (!cached.second.empty()) output << "  diagnostic: " << cached.second << '\n';
    }
    return output.str();
}

LLVKFontRegistry::Request LLVKFontRegistry::normalize(Request request)
{
    request.style &= 3;
    const auto remove = [&](const std::string& token)
    {
        const auto found = request.name.find(token);
        if (found == std::string::npos) return false;
        request.name.erase(found,token.size());
        return true;
    };
    for (const auto& [token,size] : {std::pair{"Small","Small"},{"Big","Large"},{"Medium","Medium"},
                                   {"Large","Large"},{"Huge","Huge"},{"Default","Default"}})
        if (remove(token)) request.size = size;
    for (const auto* special : {"Monospace","Scripting","Cascadia"})
        if (request.size.empty() && request.name.find(special) != std::string::npos) request.size = special;
    if (request.size.empty()) request.size = "Default";
    if (remove("Bold")) request.style |= 1;
    if (remove("Italic")) request.style |= 2;
    return request;
}

std::unique_ptr<LLVKFontRegistry> LLVKFontRegistry::create(std::span<const std::string> documents,
    Configuration configuration,std::string& error)
{
    error.clear();
    if (!std::isfinite(configuration.sizeAdjustment) || !std::isfinite(configuration.horizontalDpi) ||
        !std::isfinite(configuration.verticalDpi) || !std::isfinite(configuration.displayScale) || configuration.displayScale<=0.f ||
        configuration.horizontalDpi < 1.f || configuration.verticalDpi < 1.f)
    { error = "Invalid native font registry configuration"; return nullptr; }
    auto impl = std::make_unique<Impl>();
    impl->configuration = std::move(configuration);
    impl->documents.assign(documents.begin(),documents.end());
    for (const auto& document : documents)
    {
        if (document.size() > 4 * 1024 * 1024) { error = "Native font document exceeds limit"; return nullptr; }
        Parser parser;
        parser.parser = XML_ParserCreate(nullptr);
        if (!parser.parser) { error = "Native font XML parser allocation failed"; return nullptr; }
        XML_SetUserData(parser.parser,&parser);
        XML_SetElementHandler(parser.parser,Parser::start,Parser::end);
        XML_SetCharacterDataHandler(parser.parser,Parser::text);
        XML_SetStartDoctypeDeclHandler(parser.parser,Parser::doctype);
        XML_SetParamEntityParsing(parser.parser,XML_PARAM_ENTITY_PARSING_NEVER);
        const auto parsed = XML_Parse(parser.parser,document.data(),static_cast<int>(document.size()),XML_TRUE);
        if (parser.failure) std::rethrow_exception(parser.failure);
        if (parsed != XML_STATUS_OK || parser.rejected || !parser.root || parser.root->name != "fonts")
        { error = "Invalid native font XML (DTD and external entities are not permitted)"; return nullptr; }
        for (const auto& child : parser.root->children)
        {
            if (child->name == "font_size")
            {
                float size = 0.f;
                if (child->attribute("name").empty() || !child->attributes.contains("size") ||
                    !number(*child,"size",size) || !std::isfinite(size))
                { error = "Invalid native font size declaration"; return nullptr; }
                impl->sizes[child->attribute("name")] = size + impl->configuration.sizeAdjustment;
            }
            else if (child->name == "font")
            {
                Request request{child->attribute("name"),"TEMPLATE"};
                const auto style = child->attribute("font_style");
                if (style.find("BOLD") != std::string::npos) request.style |= 1;
                if (style.find("ITALIC") != std::string::npos) request.style |= 2;
                request = normalize(std::move(request));
                std::vector<Impl::File> files;
                if (!impl->files(*child,files,error)) return nullptr;
                auto& destination = impl->definitions[{request.name,request.style,request.size}];
                destination.insert(destination.begin(),files.begin(),files.end());
            }
        }
    }
    return std::unique_ptr<LLVKFontRegistry>(new LLVKFontRegistry(std::move(impl)));
}

std::shared_ptr<LLVKFont> LLVKFontRegistry::resolve(const Request& request,std::string& error)
{
    error.clear();
    std::lock_guard lock(mImpl->mutex);
    if (const auto found = mImpl->cache.find(request); found != mImpl->cache.end())
    { error = found->second.second; return found->second.first; }
    const auto normalized = normalize(request);
    const auto size = mImpl->sizes.find(normalized.size);
    if (size == mImpl->sizes.end()) { error = "Unknown native font size: " + normalized.size; return nullptr; }
    const std::vector<Impl::File>* selected = nullptr;
    std::uint8_t selectedStyle = 0;
    for (const auto& [key,files] : mImpl->definitions)
    {
        const auto& [name,style,templateSize] = key;
        if (name != normalized.name || templateSize != "TEMPLATE" || (style & ~normalized.style)) continue;
        if (!selected || std::popcount(style) > std::popcount(selectedStyle) ||
            (std::popcount(style) == std::popcount(selectedStyle) && (style & 1)))
        { selected = &files; selectedStyle = style; }
    }
    if (!selected) { error = "Unknown native font template: " + normalized.name; return nullptr; }
    auto files = *selected;
    if (const auto defaults = mImpl->definitions.find({"default",0,"TEMPLATE"}); defaults != mImpl->definitions.end())
        files.insert(files.end(),defaults->second.begin(),defaults->second.end());
    for (const auto& name : mImpl->configuration.ultimateFallbacks)
    { Impl::File file; file.name = name; files.push_back(std::move(file)); }
    std::optional<LLVKFont::FaceSource> primary;
    std::vector<LLVKFont::FallbackSource> fallbacks;
    for (const auto& file : files)
    {
        for (const auto& directory : mImpl->configuration.searchDirectories)
        {
            auto pathText = directory.u8string();
            if (!pathText.empty() && pathText.back() != u8'/' && pathText.back() != u8'\\') pathText += u8'/';
            pathText.append(file.name.begin(),file.name.end());
            const std::filesystem::path path(pathText);
            std::ifstream stream(path,std::ios::binary | std::ios::ate);
            if (!stream) continue;
            const auto length = stream.tellg();
            if (length <= 0 || length > 64 * 1024 * 1024) continue;
            LLVKFont::FaceSource source;
            source.bytes.resize(static_cast<std::size_t>(length));
            stream.seekg(0);
            stream.read(reinterpret_cast<char*>(source.bytes.data()),length);
            if (!stream) continue;
            source.options.pointSize = size->second + file.sizeDelta;
            source.options.horizontalDpi = mImpl->configuration.horizontalDpi;
            source.options.verticalDpi = mImpl->configuration.verticalDpi;
            source.options.weight = file.weight;
            source.options.hinting = file.hinting;
            source.options.descriptorBold = file.bold;
            std::string faceError;
            if (!LLVKFontFace::create(source.bytes,source.options,faceError)) continue;
            if (!primary) primary = std::move(source);
            else fallbacks.push_back({std::move(source),file.policy});
            break;
        }
    }
    std::shared_ptr<LLVKFont> font;
    if (primary) font = LLVKFont::create(*primary,fallbacks,mImpl->configuration.monochromeEmoji,error,mImpl->configuration.displayScale);
    else error = "No usable native font file for " + normalized.name;
    mImpl->cache.emplace(request,std::make_pair(font,error));
    return font;
}

bool LLVKFontRegistry::setDisplayScale(float scale,float horizontalBaseDpi,float verticalBaseDpi,std::string& error)
{
    std::lock_guard lock(mImpl->mutex);
    auto configuration=mImpl->configuration;
    configuration.displayScale=scale;
    configuration.horizontalDpi=std::floor(horizontalBaseDpi*scale);
    configuration.verticalDpi=std::floor(verticalBaseDpi*scale);
    auto replacement=create(mImpl->documents,configuration,error);
    if (!replacement) return false;
    for (const auto& [request,entry] : mImpl->cache)
        if (entry.first && !replacement->resolve(request,error)) return false;
    for (const auto& [request,entry] : mImpl->cache)
        if (entry.first)
        {
            auto& next=replacement->mImpl->cache.at(request).first;
            entry.first->replaceRasterState(*next);
            next=entry.first;
        }
    mImpl->cache.swap(replacement->mImpl->cache);
    mImpl->configuration=std::move(configuration);
    return true;
}