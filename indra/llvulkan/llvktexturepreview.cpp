#include "llvktexturepreview.h"
#include "llimage.h"
#include <chrono>
#include <set>

LLVKTexturePreview::LLVKTexturePreview(LLVKTextureCache& cache,LLVKWidgetTree& tree,LLVKWidgetTree::Id root)
    : mCache(cache),mTree(tree),mRoot(root) {}

bool LLVKTexturePreview::update(std::string& error)
{
    error.clear();
    if (!mCache.update(error)) return false;
    std::size_t active=0;
    for (auto& [id,request] : mRequests)
    {
        if (request.read.valid() && request.read.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        {
            auto bytes=request.read.get();
            const auto* node=mTree.get(id);
            const bool current=node && node->textureControl && node->textureControl->current.asset==request.asset &&
                node->textureControl->generation==request.generation;
            if (!current || !bytes.success) request.status=Status::Missing;
            else if (bytes.imageSize<=0 || bytes.bytes.size()!=static_cast<std::size_t>(bytes.imageSize))
            { request.status=Status::Failed; request.error="Cached texture is incomplete; asset fetching is required"; }
            else
            {
                request.decode=std::async(std::launch::async,[bytes=std::move(bytes),name=request.asset.asString()]()
                {
                    Decoded result;
                    try
                    {
                        switch (bytes.codec)
                        {
                            case IMG_CODEC_J2C: result.image=LLVKWidgetImage::decodeJ2c(name,bytes.bytes,result.error); break;
                            case IMG_CODEC_JPEG: result.image=LLVKWidgetImage::decodeJpeg(name,bytes.bytes,result.error); break;
                            case IMG_CODEC_TGA: result.image=LLVKWidgetImage::decodeTga(name,bytes.bytes,result.error); break;
                            default: result.error="Unsupported cached preview codec"; break;
                        }
                    }
                    catch (const std::exception& exception) { result.error=exception.what(); }
                    return result;
                });
            }
        }
        if (request.decode.valid() && request.decode.wait_for(std::chrono::seconds(0))==std::future_status::ready)
        {
            auto decoded=request.decode.get();
            request.error=std::move(decoded.error);
            if (!decoded.image) request.status=Status::Failed;
            else request.status=mTree.publishTexturePreview(id,request.asset,request.generation,std::move(decoded.image)) ?
                Status::Ready : Status::Missing;
        }
        if (request.read.valid() || request.decode.valid()) ++active;
    }
    std::set<LLVKWidgetTree::Id> desired;
    std::vector<LLVKWidgetTree::Id> pending{mRoot};
    while (!pending.empty())
    {
        const auto id=pending.back(); pending.pop_back();
        const auto* node=mTree.get(id);
        if (!node || !node->params.visible) continue;
        pending.insert(pending.end(),node->children.begin(),node->children.end());
        if (!node->textureControl || !node->textureControl->valid || node->textureControl->current.asset.isNull()) continue;
        desired.insert(id);
        const auto& texture=*node->textureControl;
        auto found=mRequests.find(id);
        if (found!=mRequests.end())
        {
            if (found->second.asset==texture.current.asset && found->second.generation==texture.generation &&
                (found->second.status==Status::Ready || found->second.cacheRevision==mCache.revision())) continue;
            if (found->second.read.valid() || found->second.decode.valid()) continue;
            mRequests.erase(found);
        }
        if (texture.preview || active>=2) continue;
        Request request;
        request.asset=texture.current.asset; request.generation=texture.generation;
        request.cacheRevision=mCache.revision();
        request.read=mCache.read(request.asset,0,16*1024*1024,error);
        if (!request.read.valid()) return false;
        mRequests.emplace(id,std::move(request));
        ++active;
    }
    for (auto iterator=mRequests.begin(); iterator!=mRequests.end();)
        if (!desired.contains(iterator->first) && !iterator->second.read.valid() && !iterator->second.decode.valid())
            iterator=mRequests.erase(iterator);
        else ++iterator;
    return true;
}

std::optional<LLVKTexturePreview::Status> LLVKTexturePreview::status(LLVKWidgetTree::Id id) const
{
    const auto found=mRequests.find(id);
    return found==mRequests.end() ? std::nullopt : std::optional<Status>(found->second.status);
}

std::string LLVKTexturePreview::failure(LLVKWidgetTree::Id id) const
{
    const auto found=mRequests.find(id);
    return found==mRequests.end() ? std::string() : found->second.error;
}