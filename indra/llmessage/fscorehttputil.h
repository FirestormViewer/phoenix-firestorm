
namespace FSCoreHttpUtil
{
    typedef boost::function<void(const LLSD &)> completionCallback_t;

	void callbackHttpPostRaw(const std::string &url, std::string postData, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							 LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t() );
    void callbackHttpGetRaw(const std::string &url, completionCallback_t success = NULL, completionCallback_t failure = NULL,
							LLCore::HttpHeaders::ptr_t aHeader = LLCore::HttpHeaders::ptr_t() );

	void trivialGetCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::HttpHeaders::ptr_t aHeader, completionCallback_t success, completionCallback_t failure);
    void trivialPostCoroRaw(std::string url, LLCore::HttpRequest::policy_t policyId, LLCore::BufferArray::ptr_t postData, LLCore::HttpHeaders::ptr_t aHeader,
							completionCallback_t success, completionCallback_t failure);
}

