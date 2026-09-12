#include "llvkxmllayers.h"

#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
    constexpr std::size_t maximumBytes = 64 * 1024 * 1024;
    struct Element
    {
        std::string tag, text;
        std::map<std::string,std::string> attributes;
        std::vector<std::unique_ptr<Element>> children;
        const std::string& attribute(const std::string& name) const
        {
            static const std::string empty;
            const auto found = attributes.find(name);
            return found == attributes.end() ? empty : found->second;
        }
    };

    struct Parser
    {
        XML_Parser parser = XML_ParserCreate(nullptr);
        std::unique_ptr<Element> root;
        std::vector<Element*> stack;
        std::size_t nodes = 0;
        std::string error;
        std::exception_ptr exception;
        ~Parser() { if (parser) XML_ParserFree(parser); }
        void reject(const std::string& problem) { error = problem; XML_StopParser(parser,XML_FALSE); }
        template<class Operation> void guarded(Operation operation) noexcept
        {
            try { operation(); }
            catch (...) { exception = std::current_exception(); XML_StopParser(parser,XML_FALSE); }
        }
        static void XMLCALL start(void* pointer, const XML_Char* tag, const XML_Char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&]
            {
                if (++state.nodes > 10000 || state.stack.size() >= 64)
                { state.reject("Native XML layer exceeds node/depth limits"); return; }
                auto element = std::make_unique<Element>();
                element->tag = tag;
                for (std::size_t index = 0; attributes[index]; index += 2)
                    element->attributes.emplace(attributes[index],attributes[index+1]);
                auto* current = element.get();
                if (state.stack.empty()) state.root = std::move(element);
                else state.stack.back()->children.push_back(std::move(element));
                state.stack.push_back(current);
            });
        }
        static void XMLCALL end(void* pointer,const XML_Char*) { static_cast<Parser*>(pointer)->stack.pop_back(); }
        static void XMLCALL text(void* pointer,const XML_Char* text,int length)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&] { if (!state.stack.empty()) state.stack.back()->text.append(text,length); });
        }
        static void XMLCALL doctype(void* pointer,const XML_Char*,const XML_Char*,const XML_Char*,int)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&] { state.reject("Native XML layers do not allow DTDs or external entities"); });
        }
        bool parse(std::string_view xml)
        {
            if (!parser) { error = "Native XML layer parser allocation failed"; return false; }
            XML_SetUserData(parser,this);
            XML_SetElementHandler(parser,start,end);
            XML_SetCharacterDataHandler(parser,text);
            XML_SetStartDoctypeDeclHandler(parser,doctype);
            XML_SetParamEntityParsing(parser,XML_PARAM_ENTITY_PARSING_NEVER);
            const auto status = XML_Parse(parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE);
            if (exception) std::rethrow_exception(exception);
            if (status != XML_STATUS_OK || !root)
            {
                if (error.empty()) error = "Invalid native XML layer";
                error += " at line " + std::to_string(XML_GetCurrentLineNumber(parser));
                return false;
            }
            return true;
        }
    };

    void update(Element& base, const Element& overlay)
    {
        base.text = overlay.text;
        for (const auto& [name,value] : overlay.attributes)
        {
            const auto found = base.attributes.find(name);
            if (found != base.attributes.end()) found->second = value;
        }
        if (base.children.empty()) return;
        std::size_t current = 0, last = 0;
        for (const auto& child : overlay.children)
        {
            for (;;)
            {
                auto& candidate = *base.children[current];
                auto expected = child->attribute("name");
                auto actual = candidate.attribute("name");
                if (expected.empty())
                {
                    expected = child->attribute("value");
                    const auto value = candidate.attributes.find("value");
                    if (value != candidate.attributes.end()) actual = value->second;
                }
                if (!actual.empty() && actual == expected)
                {
                    update(candidate,*child);
                    last = current;
                    current = (current+1)%base.children.size();
                    break;
                }
                current = (current+1)%base.children.size();
                if (current == last) break;
            }
        }
    }

    void append(std::string& output, std::string_view text)
    {
        if (text.size() > maximumBytes-output.size()) throw std::length_error("Native layered XML exceeds output budget");
        output.append(text);
    }

    void escaped(std::string& output, std::string_view text, bool attribute)
    {
        for (const char value : text)
        {
            if (value == '&') append(output,"&amp;");
            else if (value == '<') append(output,"&lt;");
            else if (value == '>') append(output,"&gt;");
            else if (value == '"' && attribute) append(output,"&quot;");
            else if (value == '\r') append(output,"&#13;");
            else if (value == '\n' && attribute) append(output,"&#10;");
            else if (value == '\t' && attribute) append(output,"&#9;");
            else append(output,std::string_view(&value,1));
        }
    }

    void serialize(const Element& element, std::string& output)
    {
        append(output,"<"); append(output,element.tag);
        for (const auto& [name,value] : element.attributes)
        {
            append(output," "); append(output,name); append(output,"=\"");
            escaped(output,value,true); append(output,"\"");
        }
        append(output,">"); escaped(output,element.text,false);
        for (const auto& child : element.children) serialize(*child,output);
        append(output,"</"); append(output,element.tag); append(output,">");
    }
}

std::optional<std::string> LLVKXmlLayers::merge(std::span<const std::string_view> layers, std::string& error)
{
    error.clear();
    if (layers.empty()) { error = "Native XML layering requires a base declaration"; return std::nullopt; }
    std::unique_ptr<Element> base;
    std::size_t total = 0;
    for (std::size_t index = 0; index < layers.size(); ++index)
    {
        const auto layer = layers[index];
        if (layer.size() > 4 * 1024 * 1024 || layer.size() > maximumBytes-total)
        { error = "Native XML layers exceed input budget"; return std::nullopt; }
        total += layer.size();
        Parser parser;
        if (!parser.parse(layer)) { error = "Layer " + std::to_string(index) + ": " + parser.error; return std::nullopt; }
        if (!base) base = std::move(parser.root);
        else if (base->attribute("name") == parser.root->attribute("name")) update(*base,*parser.root);
    }
    std::string result;
    try { serialize(*base,result); }
    catch (const std::length_error& problem) { error = problem.what(); return std::nullopt; }
    return result;
}