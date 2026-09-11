#ifndef LLVKSKINIMAGES_H
#define LLVKSKINIMAGES_H

#include "llvkskinfiles.h"
#include "llvkwidgetimage.h"

class LLVKSkinImages final
{
public:
    struct Declaration
    {
        std::string filename;
        bool preload = false;
        bool useMips = false;
        LLVKWidgetImage::Metadata metadata;
    };
    explicit LLVKSkinImages(std::shared_ptr<LLVKSkinFiles> files, std::size_t budget = 256 * 1024 * 1024);
    bool loadDeclarations(std::span<const std::string> files, std::string& error);
    bool loadDeclarations(std::string& error);
    std::shared_ptr<const LLVKWidgetImage> image(const std::string& name, std::string& error);
    const Declaration* declaration(const std::string& name) const;
    const std::map<std::string,Declaration>& declarations() const noexcept { return mDeclarations; }
    std::size_t residentBytes() const noexcept { return mResidentBytes; }
private:
    std::shared_ptr<LLVKSkinFiles> mFiles;
    std::map<std::string,Declaration> mDeclarations;
    std::map<std::string,std::shared_ptr<const LLVKWidgetImage>> mImages;
    std::map<std::filesystem::path,std::shared_ptr<const LLVKWidgetImage>> mPixels;
    std::size_t mBudget;
    std::size_t mResidentBytes = 0;
};

#endif