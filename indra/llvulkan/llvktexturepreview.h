#ifndef LLVKTEXTUREPREVIEW_H
#define LLVKTEXTUREPREVIEW_H

#include "llvktexturecache.h"
#include "llvkwidgettree.h"
#include <map>

class LLVKTexturePreview final
{
public:
    enum class Status { Pending, Ready, Missing, Failed };
    LLVKTexturePreview(LLVKTextureCache& cache,LLVKWidgetTree& tree,LLVKWidgetTree::Id root);
    bool update(std::string& error);
    std::optional<Status> status(LLVKWidgetTree::Id id) const;
    std::string failure(LLVKWidgetTree::Id id) const;
private:
    struct Decoded
    {
        std::shared_ptr<const LLVKWidgetImage> image;
        std::string error;
    };
    struct Request
    {
        LLUUID asset;
        std::uint64_t generation=0;
        std::uint64_t cacheRevision=0;
        Status status=Status::Pending;
        std::string error;
        std::future<LLVKTextureCache::Result> read;
        std::future<Decoded> decode;
    };
    LLVKTextureCache& mCache;
    LLVKWidgetTree& mTree;
    LLVKWidgetTree::Id mRoot;
    std::map<LLVKWidgetTree::Id,Request> mRequests;
};

#endif