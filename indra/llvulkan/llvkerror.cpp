#include "llvkerror.h"
#include <algorithm>

LLVKError::Policy LLVKError::policy() const noexcept
{
    using enum Code;
    switch (code)
    {
    case DefaultSettings: return {"NativeErrorDefaultSettings", "Required default settings could not be loaded. Repair the installation before starting again.", Severity::Fatal, Recovery::Stop};
    case SettingsMode: return {"NativeErrorSettingsMode", "The selected settings mode could not be loaded. Check the installed settings files before starting again.", Severity::Fatal, Recovery::Stop};
    case UnsupportedArguments: return {"NativeErrorUnsupportedArguments", "One or more command-line options are not supported by native startup. Remove unsupported options before starting again.", Severity::Fatal, Recovery::Stop};
    case SettingsRead: return {"NativeErrorSettingsRead", "Required settings could not be read or applied. Check the settings files and access permissions before starting again.", Severity::Fatal, Recovery::Stop};
    case SettingsWrite: return {"NativeErrorSettingsWrite", "Settings could not be saved. Check available disk space and write permissions before starting again.", Severity::Fatal, Recovery::Stop};
    case CacheUnavailable: return {"NativeErrorCacheUnavailable", "The cache could not be initialized. Check the cache location, access permissions and available disk space before starting again.", Severity::Fatal, Recovery::Stop};
    case StartupResources: return {"NativeErrorStartupResources", "Required startup resources are unavailable. Repair the installation before starting again.", Severity::Fatal, Recovery::Stop};
    case WindowUnavailable: return {"NativeErrorWindowUnavailable", "The native window is unavailable or could not complete an operation. Check display availability and window settings before starting again.", Severity::Fatal, Recovery::Stop};
    case RendererUnavailable: return {"NativeErrorRendererUnavailable", "The native Vulkan renderer is unavailable or has stopped. Check Vulkan driver support before starting again. No renderer switch or retry has been performed.", Severity::Fatal, Recovery::Stop};
    case BrowserUnavailable: return {"NativeErrorBrowserUnavailable", "The embedded browser could not start or load its page. Check the installation and network configuration before starting again.", Severity::Fatal, Recovery::Stop};
    case ShutdownFailed: return {"NativeErrorShutdownFailed", "Native shutdown could not complete normally. Check settings and cache access before starting again.", Severity::Fatal, Recovery::Stop};
    case OperationFailed: return {"NativeErrorOperationFailed", "The requested operation could not be completed. Dismiss this message to return to the viewer. No retry has been performed.", Severity::Error, Recovery::Continue};
    case OptionalSettings: return {"NativeErrorOptionalSettings", "Optional settings could not be loaded. The viewer will continue with the available defaults.", Severity::Warning, Recovery::Continue};
    case OutOfMemory: return {"NativeErrorOutOfMemory", "The viewer has run out of memory and cannot continue safely. Close other applications before restarting the viewer.", Severity::Fatal, Recovery::Stop};
    case MissingFiles: return {"NativeErrorMissingFiles", "Required viewer files are missing or inaccessible. Repair the installation and check file permissions before restarting.", Severity::Fatal, Recovery::Stop};
    case AudioFailed: return {"NativeErrorAudioFailed", "The audio service failed. Check the audio device and settings before restarting.", Severity::Fatal, Recovery::Stop};
    case VoiceFailed: return {"NativeErrorVoiceFailed", "The voice device service failed. Check the voice processing settings and selected devices before restarting.", Severity::Fatal, Recovery::Stop};
    case TranslationFailed: return {"NativeErrorTranslationFailed", "The translation verification service failed. Check the network and proxy settings before restarting.", Severity::Fatal, Recovery::Stop};
    case PreviewFailed: return {"NativeErrorPreviewFailed", "The texture preview service failed. Check cache access and the selected image before restarting.", Severity::Fatal, Recovery::Stop};
    case NetworkUnavailable: return {"NativeErrorNetworkUnavailable", "The network request could not be completed. Check the connection. Recovery requires the operation owner; no retry has been performed.", Severity::Error, Recovery::OwnerRequired};
    case TlsRejected: return {"NativeErrorTlsRejected", "The secure connection could not be verified. Check the system clock and network configuration. Certificate verification has not been bypassed.", Severity::Error, Recovery::OwnerRequired};
    case TransportUnavailable: return {"NativeErrorTransportUnavailable", "Native authentication is not available in this build. No login request has been sent.", Severity::Error, Recovery::Continue};
    case SessionCleanupFailed: return {"NativeErrorSessionCleanupFailed", "Session cleanup could not complete. Resources remain owned. Retry cleanup to finish the operation.", Severity::Error, Recovery::OwnerRequired};
    case AuthenticationFailed: return {"NativeErrorAuthenticationFailed", "Authentication failed. No connected session has been established.", Severity::Error, Recovery::OwnerRequired};
    case ConnectionFailed: return {"NativeErrorConnectionFailed", "The region connection failed. Recovery requires the session owner.", Severity::Error, Recovery::OwnerRequired};
    case SessionTimeout: return {"NativeErrorSessionTimeout", "The session request timed out. Recovery requires the session owner.", Severity::Error, Recovery::OwnerRequired};
    case SessionFailed: return {"NativeErrorSessionFailed", "The session operation could not be completed. Recovery requires the session owner.", Severity::Error, Recovery::OwnerRequired};
    default: return {"NativeErrorUnexpected", "An unexpected native viewer error occurred. Close the viewer and contact support if this happens again.", Severity::Fatal, Recovery::Stop};
    }
}

namespace
{
    std::string_view operationName(LLVKError::Operation operation) noexcept
    {
        using enum LLVKError::Operation;
        switch (operation)
        {
        case Bootstrap: return "bootstrap";
        case Settings: return "settings";
        case Cache: return "cache";
        case Window: return "window";
        case Renderer: return "renderer";
        case Browser: return "browser";
        case Shutdown: return "shutdown";
        case Session: return "session";
        default: return "unknown";
        }
    }

    std::string resolve(const LLVKError::Resolver& resolver, std::string_view key, std::string_view english)
    {
        if (resolver)
        {
            try
            {
                auto value = resolver(key);
                if (!value.empty() && value.size() <= 2048 && value.find("MissingString(") == std::string::npos &&
                    std::none_of(value.begin(), value.end(), [](unsigned char character)
                        { return character < 32 || character == 127; })) return value;
            }
            catch (...) {}
        }
        return std::string(english);
    }
}

std::string LLVKError::diagnostic() const
{
    const auto description = policy();
    const auto stableCode = description.key == "NativeErrorUnexpected" ? Code::Unexpected : code;
    return "native-error code=" + std::to_string(static_cast<std::uint32_t>(stableCode)) +
        " operation=" + std::string(operationName(operation)) + " generation=" + std::to_string(generation) +
        " attempt=" + std::to_string(attempt) + " severity=" + std::to_string(static_cast<unsigned>(description.severity)) +
        " recovery=" + std::to_string(static_cast<unsigned>(description.recovery));
}

LLVKError::Message LLVKError::format(const Resolver& resolver) const
{
    const auto description = policy();
    Message result;
    result.title = resolve(resolver, "NativeErrorTitle", "Vulkanstorm native error");
    result.diagnostic = diagnostic();
    result.body = resolve(resolver, description.key, description.english);
    if (description.recovery == Recovery::Stop)
        result.body += "\n\n" + resolve(resolver, "NativeErrorCloseViewer", "Acknowledge this message to close the viewer.");
    result.body += "\n\n" + result.diagnostic;
    return result;
}

bool LLVKErrorGate::begin(LLVKError::Operation operation, std::uint64_t generation) noexcept
{
    const auto index = static_cast<std::size_t>(operation);
    if (index >= mScopes.size()) return false;
    auto& scope = mScopes[index];
    if (scope.initialized && generation <= scope.generation)
        return generation == scope.generation && scope.active;
    scope = {};
    scope.initialized = true;
    scope.active = true;
    scope.generation = generation;
    return true;
}

void LLVKErrorGate::cancel(LLVKError::Operation operation, std::uint64_t generation) noexcept
{
    const auto index = static_cast<std::size_t>(operation);
    if (index < mScopes.size() && mScopes[index].generation == generation) mScopes[index].active = false;
}

bool LLVKErrorGate::accept(const LLVKError& error) noexcept
{
    const auto index = static_cast<std::size_t>(error.operation);
    if (index >= mScopes.size()) return false;
    auto& scope = mScopes[index];
    if (!scope.active || error.generation != scope.generation) return false;
    const auto code = error.policy().key == "NativeErrorUnexpected" ? LLVKError::Code::Unexpected : error.code;
    for (std::size_t entry = 0; entry < scope.count; ++entry)
        if (scope.codes[entry] == code) return false;
    scope.codes[scope.next] = code;
    scope.next = (scope.next + 1) % capacity;
    if (scope.count < capacity) ++scope.count;
    return true;
}