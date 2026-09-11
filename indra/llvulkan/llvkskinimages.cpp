#include "llvkskinimages.h"

#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

#include <charconv>
#include <exception>
#include <fstream>
#include <stdexcept>

namespace
{
    struct ImageParser
    {
        XML_Parser parser = XML_ParserCreate(nullptr);
        std::map<std::string,LLVKSkinImages::Declaration>& entries;
        std::size_t depth = 0, nodes = 0;
        bool requireVersion;
        std::string error;
        std::exception_ptr exception;
        ImageParser(std::map<std::string,LLVKSkinImages::Declaration>& declarations,bool first)
            : entries(declarations), requireVersion(first) {}
        ~ImageParser() { if (parser) XML_ParserFree(parser); }
        void reject(const std::string& reason) { error = reason; XML_StopParser(parser,XML_FALSE); }
        template<class Operation> void guarded(Operation operation) noexcept
        {
            try { operation(); }
            catch (...) { exception = std::current_exception(); XML_StopParser(parser,XML_FALSE); }
        }
        static void XMLCALL start(void* pointer,const XML_Char* tag,const XML_Char** attributes)
        {
            auto& state = *static_cast<ImageParser*>(pointer);
            state.guarded([&]
            {
                if (++state.nodes > 10000 || state.depth >= 64)
                { state.reject("Native image declarations exceed node/depth budget"); return; }
                std::map<std::string,std::string> fields;
                for (std::size_t index = 0; attributes[index]; index += 2) fields.emplace(attributes[index],attributes[index+1]);
                if (!state.depth && (std::string_view(tag) != "textures" || (state.requireVersion && !fields.contains("version"))))
                { state.reject("Native image declaration root requires textures/version"); return; }
                if (state.depth == 1 && std::string_view(tag) == "texture")
                {
                    const auto name = fields.find("name");
                    if (name != fields.end() && !name->second.empty())
                    {
                        auto& entry = state.entries[name->second];
                        const auto file = fields.find("file_name");
                        if (file != fields.end() && !file->second.empty()) entry.filename = file->second;
                        for (const auto& [field,output] : {std::pair{"preload",&entry.preload},std::pair{"use_mips",&entry.useMips}})
                        {
                            const auto flag = fields.find(field);
                            if (flag == fields.end()) continue;
                            if (flag->second == "true" || flag->second == "1") *output = true;
                            else if (flag->second == "false" || flag->second == "0") *output = false;
                        }
                        const auto rectangle = [&](const std::string& prefix, std::optional<LLVKWidgetImage::Rect>& output)
                        {
                            LLVKWidgetImage::Rect rect;
                            for (const auto& [name,target] : {std::pair{"left",&rect.left},std::pair{"bottom",&rect.bottom},
                                                            std::pair{"right",&rect.right},std::pair{"top",&rect.top}})
                            {
                                const auto field = fields.find(prefix+"."+name);
                                if (field == fields.end()) return;
                                const auto& text = field->second;
                                const auto parsed = std::from_chars(text.data(),text.data()+text.size(),*target);
                                if (parsed.ec != std::errc() || parsed.ptr != text.data()+text.size()) return;
                            }
                            output = rect;
                        };
                        rectangle("clip",entry.metadata.clip);
                        rectangle("scale",entry.metadata.scale);
                        const auto style = fields.find("scale_type");
                        if (style != fields.end()) entry.metadata.style = style->second == "scale_outer" ?
                            LLVKWidgetImage::Scale::Outer : LLVKWidgetImage::Scale::Inner;
                    }
                }
                ++state.depth;
            });
        }
        static void XMLCALL end(void* pointer,const XML_Char*) { --static_cast<ImageParser*>(pointer)->depth; }
        static void XMLCALL doctype(void* pointer,const XML_Char*,const XML_Char*,const XML_Char*,int)
        {
            auto& state = *static_cast<ImageParser*>(pointer);
            state.guarded([&] { state.reject("Native image declarations forbid DTDs and external entities"); });
        }
    };
}

LLVKSkinImages::LLVKSkinImages(std::shared_ptr<LLVKSkinFiles> files,std::size_t budget)
    : mFiles(std::move(files)), mBudget(budget)
{
    if (!mFiles || !mBudget || mBudget > 1024ull*1024*1024)
        throw std::invalid_argument("Invalid native skin image resolver or byte budget");
}

bool LLVKSkinImages::loadDeclarations(std::string& error)
{
    const auto files = mFiles->read("textures","textures.xml",LLVKSkinFiles::Policy::All,error);
    return files && loadDeclarations(*files,error);
}

bool LLVKSkinImages::loadDeclarations(std::span<const std::string> files,std::string& error)
{
    error.clear();
    if (files.empty() || !mImages.empty())
    { error = "Native image declarations require files and no published images"; return false; }
    std::map<std::string,Declaration> entries;
    std::size_t total = 0;
    for (std::size_t index = 0; index < files.size(); ++index)
    {
        const auto& file = files[index];
        if (file.size() > 4*1024*1024 || file.size() > 64*1024*1024-total)
        { error = "Native image declarations exceed byte budget"; return false; }
        total += file.size();
        ImageParser parser(entries,index == 0);
        if (!parser.parser) { error = "Native image declaration parser allocation failed"; return false; }
        XML_SetUserData(parser.parser,&parser);
        XML_SetElementHandler(parser.parser,ImageParser::start,ImageParser::end);
        XML_SetStartDoctypeDeclHandler(parser.parser,ImageParser::doctype);
        XML_SetParamEntityParsing(parser.parser,XML_PARAM_ENTITY_PARSING_NEVER);
        const auto status = XML_Parse(parser.parser,file.data(),static_cast<int>(file.size()),XML_TRUE);
        if (parser.exception) std::rethrow_exception(parser.exception);
        if (status != XML_STATUS_OK)
        {
            error = parser.error.empty() ? "Invalid native image declaration XML" : parser.error;
            error += " at line " + std::to_string(XML_GetCurrentLineNumber(parser.parser));
            return false;
        }
        if (entries.size() > 10000) { error = "Native image catalog exceeds entry budget"; return false; }
    }
    mDeclarations.swap(entries);
    return true;
}

const LLVKSkinImages::Declaration* LLVKSkinImages::declaration(const std::string& name) const
{
    const auto found = mDeclarations.find(name);
    return found == mDeclarations.end() ? nullptr : &found->second;
}

std::shared_ptr<const LLVKWidgetImage> LLVKSkinImages::image(const std::string& name,std::string& error)
{
    error.clear();
    if (name.empty() || name == "none") return nullptr;
    const auto found = mImages.find(name);
    if (found != mImages.end()) return found->second;
    const auto* entry = declaration(name);
    const auto filename = entry && !entry->filename.empty() ? entry->filename : name;
    const auto paths = mFiles->find("textures",filename,LLVKSkinFiles::Policy::Current,error);
    if (!paths) return nullptr;
    if (paths->empty()) { error = "Native skin image file not found: " + filename; return nullptr; }
    const auto existing = mPixels.find(paths->back());
    const auto metadata = entry ? entry->metadata : LLVKWidgetImage::Metadata{};
    if (existing != mPixels.end())
    {
        auto view = LLVKWidgetImage::skinView(name,existing->second,metadata,error);
        if (view) mImages.emplace(name,view);
        return view;
    }
    std::ifstream file(paths->back(),std::ios::binary|std::ios::ate);
    if (!file) { error = "Native skin image could not be opened: " + filename; return nullptr; }
    const auto size = file.tellg();
    if (size <= 0 || size > 64*1024*1024) { error = "Native skin image encoded byte budget exceeded"; return nullptr; }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))
    { error = "Native skin image read failed: " + filename; return nullptr; }
    auto image = LLVKWidgetImage::decodeSkin(filename,bytes,{},error);
    if (!image) { error = filename + ": " + error; return nullptr; }
    auto view = LLVKWidgetImage::skinView(name,image,metadata,error);
    if (!view) return nullptr;
    const auto allocation = image->bottomUpRgba().size();
    if (allocation > mBudget-mResidentBytes)
    { error = "Native skin image residency budget exhausted"; return nullptr; }
    mPixels.emplace(paths->back(),image);
    mResidentBytes += allocation;
    mImages.emplace(name,view);
    return view;
}