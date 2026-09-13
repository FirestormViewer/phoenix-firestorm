#ifndef LLVKERROR_H
#define LLVKERROR_H

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

struct LLVKError
{
    enum class Code : std::uint32_t
    {
        Unexpected = 1000,
        DefaultSettings = 1001,
        SettingsMode = 1002,
        UnsupportedArguments = 1003,
        SettingsRead = 1004,
        SettingsWrite = 1005,
        CacheUnavailable = 1006,
        StartupResources = 1007,
        WindowUnavailable = 1008,
        RendererUnavailable = 1009,
        BrowserUnavailable = 1010,
        ShutdownFailed = 1011,
        OperationFailed = 1012,
        OptionalSettings = 1013,
        OutOfMemory = 1014,
        MissingFiles = 1015,
        AudioFailed = 1016,
        VoiceFailed = 1017,
        TranslationFailed = 1018,
        PreviewFailed = 1019,
        NetworkUnavailable = 2000,
        TlsRejected = 2001,
        TransportUnavailable = 2002,
        SessionCleanupFailed = 2003,
        AuthenticationFailed = 2004,
        ConnectionFailed = 2005,
        SessionTimeout = 2006,
        SessionFailed = 2007
    };
    enum class Operation : std::uint8_t
    { Bootstrap = 0, Settings = 1, Cache = 2, Window = 3, Renderer = 4, Browser = 5, Shutdown = 6, Session = 7 };
    enum class Severity : std::uint8_t { Warning = 0, Error = 1, Fatal = 2 };
    enum class Recovery : std::uint8_t { Stop = 0, Continue = 1, OwnerRequired = 2 };
    enum class Action : std::uint8_t { Close = 0 };

    Code code = Code::Unexpected;
    Operation operation = Operation::Bootstrap;
    std::uint64_t generation = 0;
    std::uint64_t attempt = 0;

    struct Policy
    {
        std::string_view key;
        std::string_view english;
        Severity severity;
        Recovery recovery;
    };
    using Resolver = std::function<std::string(std::string_view)>;
    struct Message
    {
        std::string title;
        std::string body;
        std::string diagnostic;
        Action action = Action::Close;
    };

    Policy policy() const noexcept;
    std::string diagnostic() const;
    Message format(const Resolver& resolver = {}) const;
};

class LLVKErrorGate
{
public:
    static constexpr std::size_t capacity = 8;
    bool begin(LLVKError::Operation operation, std::uint64_t generation) noexcept;
    void cancel(LLVKError::Operation operation, std::uint64_t generation) noexcept;
    bool accept(const LLVKError& error) noexcept;
private:
    struct Scope
    {
        bool initialized = false;
        bool active = false;
        std::uint64_t generation = 0;
        std::array<LLVKError::Code, capacity> codes{};
        std::size_t count = 0;
        std::size_t next = 0;
    };
    std::array<Scope, 8> mScopes{};
};

#ifdef _WIN32
bool llvkPresentErrorFallback(const LLVKError& error, void* owner = nullptr,
    const LLVKError::Resolver& resolver = {}) noexcept;
#endif

#endif