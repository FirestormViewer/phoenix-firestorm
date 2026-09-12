#include "linden_common.h"
#include "llvktexturecache.h"
#include "llimage.h"
#include "llimagej2c.h"
#include "lltexturecache.h"
#include "llmemory.h"
#include "lltracethreadrecorder.h"
#include <chrono>
#include <mutex>
#include <thread>

namespace
{
    struct CacheAprRuntime
    {
        bool owned=!ll_apr_is_initialized();
        std::unique_ptr<LLTrace::ThreadRecorder> recorder;
        CacheAprRuntime()
        {
            if (owned) ll_init_apr();
            if (!LLTrace::get_master_thread_recorder())
            {
                recorder=std::make_unique<LLTrace::ThreadRecorder>();
                LLTrace::set_master_thread_recorder(recorder.get());
            }
        }
        ~CacheAprRuntime()
        {
            if (recorder)
            {
                recorder.reset();
                LLTrace::set_master_thread_recorder(nullptr);
            }
            if (owned) ll_cleanup_apr();
        }
        static std::shared_ptr<CacheAprRuntime> acquire()
        {
            static std::mutex mutex;
            static std::weak_ptr<CacheAprRuntime> current;
            std::lock_guard lock(mutex);
            auto runtime=current.lock();
            if (!runtime) { runtime=std::make_shared<CacheAprRuntime>(); current=runtime; }
            return runtime;
        }
    };
}

struct LLVKTextureCache::Impl
{
    struct Storage final : LLTextureCache
    {
        explicit Storage(Environment environment) : LLTextureCache(true,std::move(environment)) {}
        void beginShutdown() { setQuitting(); unpause(); }
    };
    struct Job
    {
        LLTextureCache::handle_t handle=LLTextureCache::nullHandle();
        bool writing=false, done=false;
        Result result;
        std::promise<Result> promise;
    };
    struct Read final : LLTextureCache::ReadResponder
    {
        std::shared_ptr<Job> job;
        explicit Read(std::shared_ptr<Job> value) : job(std::move(value)) {}
        void setData(U8* data,S32 size,S32 imageSize,S32 codec,bool local) override
        {
            const auto release=[](U8* pointer) { ll_aligned_free_16(pointer); };
            std::unique_ptr<U8,decltype(release)> owned(data,release);
            job->result.bytes.assign(data,data+size);
            job->result.imageSize=imageSize; job->result.codec=codec; job->result.local=local;
        }
        void completed(bool success) override { job->result.success=success; job->done=true; }
    };
    struct Write final : LLTextureCache::WriteResponder
    {
        std::shared_ptr<Job> job;
        explicit Write(std::shared_ptr<Job> value) : job(std::move(value)) {}
        void completed(bool success) override { job->result.success=success; job->done=true; }
    };
    std::thread::id thread=std::this_thread::get_id();
    std::shared_ptr<CacheAprRuntime> runtime;
    LLAPRFile executionLock, cacheLock;
    std::unique_ptr<Storage> cache;
    std::vector<std::shared_ptr<Job>> jobs;
    std::uint32_t validation=0;
    std::uint64_t revision=0;
    std::filesystem::path localAssets;
    bool stopping=false, readOnly=false, ready=false;
    bool onThread(std::string& error) const
    {
        error.clear();
        if (thread==std::this_thread::get_id()) return true;
        error="Native texture cache requires its creating thread"; return false;
    }
    static std::string pathString(const std::filesystem::path& path)
    {
        const auto bytes=path.u8string(); return {bytes.begin(),bytes.end()};
    }
    bool lock(LLAPRFile& file,const std::filesystem::path& path,std::string& error,bool shared=false)
    {
        const auto flags=shared ? APR_READ|APR_BINARY : APR_CREATE|APR_READ|APR_WRITE|APR_BINARY;
        if (file.open(pathString(path),flags)!=APR_SUCCESS ||
            !file.getFileHandle() || apr_file_lock(file.getFileHandle(),APR_FLOCK_NONBLOCK|
                (shared ? APR_FLOCK_SHARED : APR_FLOCK_EXCLUSIVE))!=APR_SUCCESS)
        { file.close(); error="Native texture cache cannot acquire compatible ownership"; return false; }
        return true;
    }
};

LLVKTextureCache::LLVKTextureCache() : mImpl(std::make_unique<Impl>()) {}
std::optional<LLVKTextureCache::StartupPlan> LLVKTextureCache::planStartup(
    const std::map<std::string,LLSD>& settings,const std::filesystem::path& defaultDirectory,
    const std::filesystem::path& localAssets,const std::filesystem::path& executionMarker,
    bool readOnly,std::string& error)
{
    error.clear();
    for (const auto name : {"CacheLocation","NewCacheLocation","CacheSize","CacheValidateCounter",
        "LocalCacheVersion","LastJ2CVersion","PurgeCacheOnStartup","PurgeCacheOnNextStartup"})
        if (!settings.contains(name)) { error="Missing texture cache setting: "+std::string(name); return {}; }
    StartupPlan plan;
    auto& configuration=plan.configuration;
    const auto location=settings.at(readOnly ? "CacheLocation" : "NewCacheLocation").asString();
    if (location.find('\0')!=std::string::npos) { error="Invalid texture cache location"; return {}; }
    configuration.directory=location.empty() ? defaultDirectory :
        std::filesystem::path(std::u8string(location.begin(),location.end()));
    configuration.localAssets=localAssets;
    configuration.executionMarker=executionMarker;
    configuration.readOnly=readOnly;
    if (!configuration.directory.is_absolute() || !localAssets.is_absolute() ||
        (!executionMarker.empty() && !executionMarker.is_absolute()))
    { error="Texture cache startup requires absolute paths"; return {}; }
    configuration.bytes=static_cast<std::uint64_t>(std::clamp(settings.at("CacheSize").asInteger(),256,100*1024))*1024*1024;
    const auto validation=settings.at("CacheValidateCounter").asInteger();
    if (validation<0 || validation>255) { error="Invalid texture cache validation counter"; return {}; }
    configuration.validationIndex=static_cast<std::uint32_t>(validation);
    const auto encoder=encoderVersion();
    const auto previousEncoder=settings.at("LastJ2CVersion").asString();
    const bool encoderChanged=!previousEncoder.empty() && previousEncoder!=encoder;
    configuration.versionMismatch=settings.at("LocalCacheVersion").asInteger()!=9 || (readOnly && encoderChanged);
    configuration.purge=!readOnly && (settings.at("PurgeCacheOnStartup").asBoolean() ||
        settings.at("PurgeCacheOnNextStartup").asBoolean() || encoderChanged);
    if (!readOnly)
    {
        plan.metadata={{"LocalCacheVersion",LLSD(9)},{"LastJ2CVersion",LLSD(encoder)},
            {"CacheLocation",LLSD(location)},
            {"CacheLocationTopFolder",LLSD(Impl::pathString(configuration.directory.filename()))}};
    }
    return plan;
}

LLVKTextureCache::~LLVKTextureCache()
{
    std::string error;
    if (!stop(error))
    {
        LL_WARNS("NativeTextureCache") << error << "; retaining worker resources until process exit" << LL_ENDL;
        mImpl.release();
    }
}

bool LLVKTextureCache::start(const Configuration& configuration,std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    if (mImpl->cache || mImpl->stopping) { error="Native texture cache has already started or stopped"; return false; }
    if (!configuration.directory.is_absolute() || !configuration.localAssets.is_absolute() ||
        (!configuration.executionMarker.empty() && !configuration.executionMarker.is_absolute()) ||
        configuration.bytes<256ull*1024*1024 || configuration.bytes>100ull*1024*1024*1024)
    { error="Invalid native texture cache paths or capacity"; return false; }
    mImpl->readOnly=configuration.readOnly;
    if (configuration.readOnly && configuration.versionMismatch)
    { error="Read-only texture cache version is incompatible"; return false; }
    mImpl->runtime=CacheAprRuntime::acquire();
    std::error_code status;
    if (!configuration.executionMarker.empty())
    {
        if (!configuration.readOnly) std::filesystem::create_directories(configuration.executionMarker.parent_path(),status);
        if (status || !mImpl->lock(mImpl->executionLock,configuration.executionMarker,error,configuration.readOnly))
        { if (error.empty()) error=status.message(); return false; }
    }
    if (!configuration.readOnly) std::filesystem::create_directories(configuration.directory,status);
    if (status || !mImpl->lock(mImpl->cacheLock,configuration.directory/"native-texture-cache.lock",error,configuration.readOnly))
    { mImpl->executionLock.close(); if (error.empty()) error=status.message(); return false; }
    mImpl->validation=configuration.validationIndex;
    mImpl->localAssets=configuration.localAssets;
    LLTextureCache::Environment environment;
    environment.workerName="NativeTextureCache-"+LLUUID::generateNewID().asString();
    environment.encodedReadLimit=16*1024*1024;
    environment.path=[root=configuration.directory,assets=configuration.localAssets](ELLPath location,const std::string& directory,const std::string& file)
    { return Impl::pathString((location==LL_PATH_LOCAL_ASSETS ? assets : root)/directory/file); };
    environment.validationIndex=[owner=mImpl.get()] { return owner->validation; };
    environment.saveValidationIndex=[owner=mImpl.get()](U32 value) { owner->validation=value; };
    mImpl->cache=std::make_unique<Impl::Storage>(std::move(environment));
    if (configuration.readOnly)
    {
        if (!mImpl->cache->initReadOnlyCache(LL_PATH_CACHE))
        { error="Read-only texture cache index is missing, corrupt or incompatible"; return false; }
        mImpl->ready=true;
        return true;
    }
    mImpl->cache->setReadOnly(false);
    if (configuration.purge) mImpl->cache->purgeCache(LL_PATH_CACHE);
    mImpl->cache->initCache(LL_PATH_CACHE,static_cast<S64>(configuration.bytes),configuration.versionMismatch);
    if (!std::filesystem::is_regular_file(configuration.directory/"texturecache"/"texture.entries") ||
        !std::filesystem::is_regular_file(configuration.directory/"texturecache"/"FastCache.cache"))
    { error="Persistent texture cache initialization did not create its storage"; return false; }
    mImpl->ready=true;
    return true;
}

bool LLVKTextureCache::update(std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    if (!mImpl->cache) { error="Native texture cache is not started"; return false; }
    mImpl->cache->update(1.f);
    for (auto iterator=mImpl->jobs.begin(); iterator!=mImpl->jobs.end();)
    {
        auto& job=**iterator;
        if (job.done && (job.writing ? mImpl->cache->writeComplete(job.handle) : mImpl->cache->readComplete(job.handle,false)))
        {
            if (job.writing)
            {
                if (job.result.success) ++mImpl->revision;
                job.result.bytes.clear();
            }
            job.promise.set_value(std::move(job.result));
            iterator=mImpl->jobs.erase(iterator);
        }
        else ++iterator;
    }
    return true;
}

bool LLVKTextureCache::stop(std::string& error)
{
    if (!mImpl->onThread(error)) return false;
    mImpl->stopping=true;
    if (mImpl->cache)
    {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while (!mImpl->jobs.empty())
        {
            if (!update(error)) return false;
            if (std::chrono::steady_clock::now()>=deadline)
            { error="Native texture cache drain timed out"; return false; }
            std::this_thread::yield();
        }
        mImpl->cache->beginShutdown();
        do
        {
            mImpl->cache->update(1.f);
            if (std::chrono::steady_clock::now()>=deadline)
            { error="Native texture cache queue retirement timed out"; return false; }
            std::this_thread::yield();
        }
        while (mImpl->cache->getPending());
        mImpl->cache->shutdown();
        if (!mImpl->cache->isStopped()) { error="Native texture cache worker did not stop"; return false; }
        mImpl->cache.reset();
    }
    mImpl->cacheLock.close(); mImpl->executionLock.close();
    mImpl->runtime.reset();
    return true;
}

std::future<LLVKTextureCache::Result> LLVKTextureCache::read(const LLUUID& id,int offset,int size,std::string& error)
{
    if (!mImpl->onThread(error)) return {};
    if (!mImpl->ready || mImpl->stopping || id.isNull() || offset<0 || size<=0 ||
        size>16*1024*1024 || offset>16*1024*1024-size || mImpl->jobs.size()>=64)
    { error="Invalid or unavailable native texture cache read"; return {}; }
    auto job=std::make_shared<Impl::Job>();
    auto future=job->promise.get_future();
    std::filesystem::path local;
    if (!offset)
        for (const auto extension : {".j2c",".jpg",".tga"})
        {
            const auto candidate=mImpl->localAssets/(id.asString()+extension);
            std::error_code status;
            if (std::filesystem::is_regular_file(candidate,status)) { local=candidate; break; }
        }
    job->handle=local.empty() ? mImpl->cache->readFromCache(id,offset,size,new Impl::Read(job)) :
        mImpl->cache->readFromCache(Impl::pathString(local),id,offset,size,new Impl::Read(job));
    mImpl->jobs.push_back(std::move(job)); return future;
}

std::future<LLVKTextureCache::Result> LLVKTextureCache::write(const LLUUID& id,std::vector<std::uint8_t> bytes,int imageSize,std::string& error)
{
    if (!mImpl->onThread(error)) return {};
    if (!mImpl->ready || mImpl->stopping || mImpl->readOnly || id.isNull() || bytes.empty() || bytes.size()>16*1024*1024 ||
        imageSize<static_cast<int>(bytes.size()) || mImpl->jobs.size()>=64)
    { error="Invalid or unavailable native texture cache write"; return {}; }
    auto job=std::make_shared<Impl::Job>(); job->writing=true; job->result.bytes=std::move(bytes);
    auto future=job->promise.get_future();
    job->handle=mImpl->cache->writeEncoded(id,job->result.bytes.data(),static_cast<S32>(job->result.bytes.size()),imageSize,new Impl::Write(job));
    if (job->handle==LLTextureCache::nullHandle()) { error="Persistent texture cache rejected write"; return {}; }
    mImpl->jobs.push_back(std::move(job)); return future;
}

std::uint32_t LLVKTextureCache::validationIndex() const { return mImpl->validation; }
bool LLVKTextureCache::readOnly() const { return mImpl->readOnly; }
std::uint64_t LLVKTextureCache::revision() const { return mImpl->revision; }
std::string LLVKTextureCache::encoderVersion() { return LLImageJ2C::getEngineInfo(); }