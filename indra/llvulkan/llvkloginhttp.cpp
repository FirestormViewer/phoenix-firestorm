#include "linden_common.h"
#include "llvkloginhttp.h"
#include <curl/curl.h>
#include <boost/url.hpp>
#include <utility>

struct LLVKLoginHttp::Impl
{
    Configuration configuration;
    CURLM* multi=nullptr;
    CURL* easy=nullptr;
    curl_slist* headers=nullptr;
    bool initialized=false;
    bool attached=false;
    bool overflow=false;
    bool timedOut=false;
    Status status=Status::Idle;
    long responseCode=0;
    std::string request,response;
    static void erase(std::string& value)
    {
        volatile char* bytes=value.empty() ? nullptr : value.data();
        for (std::size_t index=0; index<value.size(); ++index) bytes[index]=0;
        value.clear();
    }
    void release()
    {
        if (attached) curl_multi_remove_handle(multi,easy);
        attached=false;
        if (easy) curl_easy_cleanup(easy);
        easy=nullptr;
        if (headers) curl_slist_free_all(headers);
        headers=nullptr;
        erase(request);
    }
    ~Impl()
    {
        release();
        erase(response);
        if (multi) curl_multi_cleanup(multi);
        if (initialized) curl_global_cleanup();
    }
    static std::size_t receive(char* bytes,std::size_t size,std::size_t count,void* opaque) noexcept
    {
        auto& self=*static_cast<Impl*>(opaque);
        constexpr std::size_t limit=8*1024*1024;
        if (size && count>(limit-self.response.size())/size) { self.overflow=true; return 0; }
        const auto length=size*count;
        try { self.response.append(bytes,length); }
        catch (...) { return 0; }
        return length;
    }
};

LLVKLoginHttp::LLVKLoginHttp(Configuration configuration) : mImpl(std::make_unique<Impl>())
{
    mImpl->configuration=std::move(configuration);
    mImpl->initialized=curl_global_init(CURL_GLOBAL_DEFAULT)==CURLE_OK;
    if (mImpl->initialized) mImpl->multi=curl_multi_init();
}

LLVKLoginHttp::~LLVKLoginHttp()=default;

bool LLVKLoginHttp::start(const std::string& url,std::string body,const std::string& contentType,std::string& error,Method method)
{
    error.clear();
    const auto parsed=boost::urls::parse_uri(url);
    const bool testHttp=parsed && mImpl->configuration.allowLoopbackHttpForTests && parsed->scheme()=="http" &&
        (parsed->host()=="127.0.0.1" || parsed->host()=="[::1]");
    if (!parsed || url.size()>16384 || parsed->has_userinfo() || parsed->has_fragment() || parsed->host().empty() ||
        (parsed->scheme()!="https" && !testHttp) || body.size()>65536 || (method==Method::Get && !body.empty()) ||
        (contentType!="text/xml" && contentType!="application/llsd+xml"))
    { Impl::erase(body); error="Invalid native login HTTP request"; return false; }
    if (mImpl->status==Status::Pending) { Impl::erase(body); error="Native login HTTP request is already pending"; return false; }
    cancel();
    if (!mImpl->multi) { Impl::erase(body); error="Native login HTTP initialization failed"; return false; }
    mImpl->easy=curl_easy_init();
    if (!mImpl->easy) { Impl::erase(body); error="Native login HTTP allocation failed"; return false; }
    mImpl->request=std::move(body);
    mImpl->headers=curl_slist_append(nullptr,("Content-Type: "+contentType).c_str());
    if (!mImpl->headers) { cancel(); error="Native login HTTP header allocation failed"; return false; }
    bool configured=true;
    const auto option=[&](CURLoption name,auto value)
    { if (curl_easy_setopt(mImpl->easy,name,value)!=CURLE_OK) configured=false; };
    option(CURLOPT_URL,url.c_str());
    if (method==Method::Get) option(CURLOPT_HTTPGET,1L);
    else
    {
        option(CURLOPT_POST,1L);
        option(CURLOPT_POSTFIELDS,mImpl->request.data());
        option(CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(mImpl->request.size()));
    }
    option(CURLOPT_HTTPHEADER,mImpl->headers);
    option(CURLOPT_WRITEFUNCTION,&Impl::receive);
    option(CURLOPT_WRITEDATA,mImpl.get());
    option(CURLOPT_SSL_VERIFYPEER,1L);
    option(CURLOPT_SSL_VERIFYHOST,2L);
    option(CURLOPT_FOLLOWLOCATION,0L);
    option(CURLOPT_PROTOCOLS,static_cast<long>(testHttp ? CURLPROTO_HTTP|CURLPROTO_HTTPS : CURLPROTO_HTTPS));
    option(CURLOPT_NOSIGNAL,1L);
    option(CURLOPT_CONNECTTIMEOUT_MS,15000L);
    option(CURLOPT_TIMEOUT_MS,40000L);
    option(CURLOPT_USERAGENT,mImpl->configuration.userAgent.c_str());
    option(CURLOPT_PROXY,mImpl->configuration.proxy.c_str());
    if (!mImpl->configuration.certificateBundle.empty()) option(CURLOPT_CAINFO,mImpl->configuration.certificateBundle.c_str());
    if (!configured || curl_multi_add_handle(mImpl->multi,mImpl->easy)!=CURLM_OK)
    { cancel(); error="Native login HTTP configuration failed"; return false; }
    mImpl->attached=true;
    mImpl->status=Status::Pending;
    return true;
}

LLVKLoginHttp::Status LLVKLoginHttp::pump(std::string& error)
{
    error.clear();
    if (mImpl->status!=Status::Pending) return mImpl->status;
    int running=0;
    if (curl_multi_perform(mImpl->multi,&running)!=CURLM_OK)
    { mImpl->release(); mImpl->status=Status::Failed; error="Native login HTTP progress failed"; return mImpl->status; }
    int remaining=0;
    while (auto* message=curl_multi_info_read(mImpl->multi,&remaining))
    {
        if (message->msg!=CURLMSG_DONE || message->easy_handle!=mImpl->easy) continue;
        const auto code=message->data.result;
        mImpl->timedOut=code==CURLE_OPERATION_TIMEDOUT;
        curl_easy_getinfo(mImpl->easy,CURLINFO_RESPONSE_CODE,&mImpl->responseCode);
        mImpl->status=code==CURLE_OK && mImpl->responseCode>=200 && mImpl->responseCode<300 ? Status::Complete : Status::Failed;
        if (mImpl->status==Status::Failed)
            error=mImpl->overflow ? "Native login HTTP response exceeds its limit" : "Native login HTTPS request failed";
        mImpl->release();
    }
    return mImpl->status;
}

void LLVKLoginHttp::cancel()
{
    mImpl->release();
    Impl::erase(mImpl->response);
    mImpl->responseCode=0;
    mImpl->overflow=false;
    mImpl->timedOut=false;
    mImpl->status=Status::Idle;
}

long LLVKLoginHttp::responseCode() const { return mImpl->responseCode; }
bool LLVKLoginHttp::timedOut() const { return mImpl->timedOut; }
std::string LLVKLoginHttp::takeResponse() { return std::exchange(mImpl->response,{}); }