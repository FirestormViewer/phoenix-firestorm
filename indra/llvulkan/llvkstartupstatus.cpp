#include "linden_common.h"
#include "llvkstartupstatus.h"
#include "llvkxmllayers.h"
#include "llstring.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <sstream>
#include <windows.h>

struct LLVKStartupStatus::Impl
{
    HWND window=nullptr;
    std::string title;
    std::map<std::string,std::string> strings;
    static INT_PTR CALLBACK procedure(HWND,UINT,WPARAM,LPARAM) { return FALSE; }
};

LLVKStartupStatus::LLVKStartupStatus() : mImpl(std::make_unique<Impl>()) {}
LLVKStartupStatus::~LLVKStartupStatus() { hide(); }

bool LLVKStartupStatus::load(const LLVKSkinFiles::Configuration& configuration,const std::string& title,std::string& error)
{
    error.clear();
    LLVKSkinFiles skin(configuration);
    const auto files=skin.read("xui","strings.xml",LLVKSkinFiles::Policy::Current,error);
    if (!files) return false;
    std::vector<std::string_view> layers;
    for (const auto& file : *files) layers.push_back(file);
    const auto xml=LLVKXmlLayers::merge(layers,error);
    if (!xml) return false;
    try
    {
        boost::property_tree::ptree document;
        std::istringstream input(*xml);
        boost::property_tree::read_xml(input,document);
        std::map<std::string,std::string> strings;
        for (const auto& [tag,entry] : document.get_child("strings"))
            if (tag=="string")
            {
                const auto name=entry.get<std::string>("<xmlattr>.name","");
                if (name=="StartupInitializingTextureCache" || name=="StartupClearingTextureCache" || name=="ShuttingDown")
                    strings[name]=entry.data();
            }
            if (strings.size()!=3) { error="Native startup/shutdown status strings are missing"; return false; }
        mImpl->strings=std::move(strings);
        mImpl->title=title;
        return true;
    }
    catch (const std::exception& exception)
    { error="Native startup strings could not be loaded: "+std::string(exception.what()); return false; }
}

bool LLVKStartupStatus::show(const std::string& key,std::string& error)
{
    error.clear();
    const auto found=mImpl->strings.find(key);
    if (found==mImpl->strings.end()) { error="Unknown native startup status: "+key; return false; }
    if (!mImpl->window)
    {
        mImpl->window=CreateDialogParamW(GetModuleHandleW(nullptr),L"SPLASHSCREEN",nullptr,Impl::procedure,0);
        if (!mImpl->window) { error="Native startup status resource could not be created"; return false; }
        SetWindowTextW(mImpl->window,ll_convert<std::wstring>(mImpl->title).c_str());
    }
    if (!SetDlgItemTextW(mImpl->window,666,ll_convert<std::wstring>(found->second).c_str()))
    { error="Native startup status text could not be set"; return false; }
    ShowWindow(mImpl->window,SW_SHOWNOACTIVATE);
    if (!RedrawWindow(mImpl->window,nullptr,nullptr,RDW_INVALIDATE|RDW_UPDATENOW|RDW_ALLCHILDREN))
    { error="Native startup status could not be painted"; return false; }
    return true;
}

void LLVKStartupStatus::hide()
{
    if (mImpl->window) { DestroyWindow(mImpl->window); mImpl->window=nullptr; }
}