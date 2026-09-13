#include "llvkerror.h"
#include <iostream>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    void check(bool condition, const char* name)
    {
        if (!condition) throw std::runtime_error(name);
    }

#ifdef _WIN32
    unsigned dialogs = 0;
    bool closeOnly = false;
    bool recursionRejected = false;
    void CALLBACK dismiss(HWND window, UINT, UINT_PTR timer, DWORD)
    {
        KillTimer(window, timer);
        EndDialog(window, IDOK);
    }
    LRESULT CALLBACK acknowledge(int code, WPARAM parameter, LPARAM data)
    {
        if (code == HCBT_ACTIVATE)
        {
            const auto window = reinterpret_cast<HWND>(parameter);
            if (GetDlgItem(window, IDOK))
            {
                ++dialogs;
                closeOnly = !GetDlgItem(window, IDCANCEL) && !GetDlgItem(window, IDRETRY);
                recursionRejected = !llvkPresentErrorFallback({LLVKError::Code::Unexpected});
                SetTimer(window, 1, 10, dismiss);
            }
        }
        return CallNextHookEx(nullptr, code, parameter, data);
    }
#endif
}

int main()
{
    try
    {
        using Code = LLVKError::Code;
        using Operation = LLVKError::Operation;
        LLVKError error{Code::DefaultSettings, Operation::Bootstrap, 7, 3};
        const auto english = error.format();
        check(english.diagnostic == "native-error code=1001 operation=bootstrap generation=7 attempt=3 severity=2 recovery=0", "deterministic diagnostic");
        check(english.body.find("Repair the installation") != std::string::npos, "missing resource advice");
        check(english.action == LLVKError::Action::Close, "no unowned retry or cancel");
        check(error.format([](auto) { return std::string(); }).body == english.body, "missing catalog fallback");
        check(error.format([](auto) -> std::string { throw std::runtime_error("untrusted exception text"); }).body == english.body, "resolver exception fallback");
        check(error.format([](auto) { return std::string("bad\ncontrol"); }).body == english.body, "control rejection");
        check(error.format([](auto) { return std::string("MissingString(key)"); }).body == english.body, "missing marker fallback");
        check(error.format([](auto) { return std::string(2049, 'a'); }).body == english.body, "bounded localization");
        const auto translated = error.format([](auto key)
            { return key == "NativeErrorDefaultSettings" ? std::string("Catalog message") : std::string(); });
        check(translated.body.starts_with("Catalog message"), "message key resolver");
        check(translated.diagnostic == english.diagnostic, "logs independent of resolver");
        error.code = Code::CacheUnavailable;
        check(error.format().body.find("access permissions") != std::string::npos, "cache permission advice");
        error.code = Code::RendererUnavailable;
        check(error.policy().recovery == LLVKError::Recovery::Stop, "renderer must stop");
        error.code = Code::TransportUnavailable;
        check(error.policy().recovery == LLVKError::Recovery::Continue &&
            error.format().body.find("No login request") != std::string::npos, "absent transport is not a network retry");
        for (const auto code : {Code::NetworkUnavailable, Code::TlsRejected, Code::SessionCleanupFailed,
            Code::AuthenticationFailed, Code::ConnectionFailed, Code::SessionTimeout, Code::SessionFailed})
        {
            error.code = code;
            check(error.policy().recovery == LLVKError::Recovery::OwnerRequired, "network recovery belongs to owner");
            check(error.format().action == LLVKError::Action::Close, "network does not invent recovery");
        }
        LLVKErrorGate gate;
        error = {Code::OperationFailed, Operation::Session, 10, 1};
        check(!gate.accept(error), "inactive scope");
        check(gate.begin(Operation::Session, 10), "activate scope");
        check(gate.accept(error) && !gate.accept(error), "duplicate suppression");
        error.attempt = 2;
        check(!gate.accept(error), "attempt does not bypass generation suppression");
        check(gate.begin(Operation::Session, 10) && !gate.accept(error), "begin idempotent");
        error.code = Code::TlsRejected;
        check(gate.accept(error), "distinct error accepted");
        gate.cancel(Operation::Session, 9);
        error.code = Code::NetworkUnavailable;
        check(gate.accept(error), "stale cancellation ignored");
        gate.cancel(Operation::Session, 10);
        check(!gate.accept(error) && !gate.begin(Operation::Session, 10), "cancelled generation cannot reopen");
        check(gate.begin(Operation::Session, 11), "new generation");
        check(!gate.accept(error), "late response rejected");
        error.generation = 11;
        check(gate.accept(error) && !gate.begin(Operation::Session, 10), "generation cannot rewind");
        error.operation = Operation::Browser;
        check(gate.begin(Operation::Browser, 11) && gate.accept(error), "operation isolation");
        error.code = static_cast<Code>(99999);
        check(error.diagnostic().starts_with("native-error code=1000 "), "unknown code normalized");
        check(gate.accept(error), "unknown admitted once");
        error.code = static_cast<Code>(99998);
        check(!gate.accept(error), "unknown codes cannot flood");
        error.operation = static_cast<Operation>(255);
        check(!gate.begin(error.operation, 1) && !gate.accept(error), "invalid operation rejected");
        error = {Code::Unexpected, Operation::Cache, 1, 0};
        check(gate.begin(error.operation, error.generation), "capacity test scope");
        for (std::uint32_t index = 0; index <= LLVKErrorGate::capacity; ++index)
        {
            error.code = static_cast<Code>(1000 + index);
            check(gate.accept(error), "bounded gate insertion");
        }
        check(!gate.accept(error), "newest entry retained");
        error.code = Code::Unexpected;
        check(gate.accept(error), "oldest entry evicted at capacity");
        error.generation = (std::numeric_limits<std::uint64_t>::max)();
        error.attempt = error.generation;
        check(gate.begin(error.operation, error.generation) && gate.accept(error), "full-width identities");
        check(!gate.begin(error.operation, 0), "generation wrap rejected");
        check(error.diagnostic().find("attempt=18446744073709551615") != std::string::npos, "full-width formatting");
        const auto log = error.diagnostic();
        error.format([](auto) { return std::string("Trusted catalog text"); });
        check(error.diagnostic() == log, "localization cannot alter diagnostic facts");
    #ifdef _WIN32
        const auto hook = SetWindowsHookExW(WH_CBT, acknowledge, nullptr, GetCurrentThreadId());
        check(hook != nullptr, "install automatic acknowledgement hook");
        const bool presented = llvkPresentErrorFallback({Code::RendererUnavailable, Operation::Renderer, 1, 1});
        UnhookWindowsHookEx(hook);
        check(presented && dialogs == 1 && closeOnly && recursionRejected, "GPU-free close-only OS fallback and recursion guard");
    #endif
        std::cout << "Native error formatting, policy, duplicate and generation tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Native error test failed: " << exception.what() << '\n';
        return 1;
    }
}