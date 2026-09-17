#pragma once

#include <memory>
#include <string>

class LLVKLoginHttp final
{
public:
    enum class Status { Idle, Pending, Complete, Failed };
    enum class Method { Post, Get };
    struct Configuration
    {
        std::string certificateBundle;
        std::string userAgent;
        std::string proxy;
        bool allowLoopbackHttpForTests=false;
    };
    explicit LLVKLoginHttp(Configuration configuration);
    ~LLVKLoginHttp();
    LLVKLoginHttp(const LLVKLoginHttp&)=delete;
    LLVKLoginHttp& operator=(const LLVKLoginHttp&)=delete;
    bool start(const std::string& url,std::string body,const std::string& contentType,std::string& error,Method method=Method::Post);
    Status pump(std::string& error);
    void cancel();
    long responseCode() const;
    bool timedOut() const;
    std::string takeResponse();
private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};