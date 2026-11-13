/** Regression coverage for LLProcess stdin writes while more data is queued. */
#include "linden_common.h"

// <FS:Beq> Keep asynchronous stdin data valid when subsequent output grows the stream buffer.
#include "llprocess.h"
#include "llstring.h"
#include "fsyspath.h"
#include "../test/lltut.h"
#include "../test/namedtempfile.h"

#include <chrono>
#include <fstream>
#include <thread>

namespace tut
{
    struct process_stdin_data {};
    using process_stdin_group = test_group<process_stdin_data>;
    using process_stdin_object = process_stdin_group::object;
    process_stdin_group process_stdin_tests("LLProcessStdin");

    template<> template<>
    void process_stdin_object::test<1>()
    {
        set_test_name("growing stdin buffer preserves an in-flight write");

        const auto python = LLStringUtil::getenv("PYTHON");
        ensure("Set PYTHON to the interpreter pathname", !python.empty());
        const std::size_t first_size = 1024 * 1024;
        const std::size_t appended_size = 8 * first_size;
        NamedTempFile release_reader("stdin_release", "");
        NamedExtTempFile script("py",
            "import os\n"
            "import sys\n"
            "import time\n"
            "print('ready', flush=True)\n"
            "# Do not drain the OS pipe until the parent has grown its buffer.\n"
            "while os.path.getsize(sys.argv[1]) == 0:\n"
            "    time.sleep(0.001)\n"
            "first, appended = map(int, sys.argv[2:])\n"
            "expected = b'A' * first + b'B' * appended\n"
            "data = sys.stdin.buffer.read(len(expected))\n"
            "if data != expected:\n"
            "    offset = next((i for i, (got, want) in enumerate(zip(data, expected))\n"
            "                   if got != want), min(len(data), len(expected)))\n"
            "    print('payload mismatch at byte %d (received %d of %d bytes)' %\n"
            "          (offset, len(data), len(expected)), flush=True)\n"
            "    sys.exit(1)\n"
            "print('ok', flush=True)\n");

        LLProcess::Params params;
        params.desc = get_test_name();
        params.executable = python;
        // Keep path arguments UTF-8 when the Windows temp directory contains
        // characters outside the native narrow code page.
        // params.args.add(script.getPath().string());
        // params.args.add(release_reader.getPath().string());
        params.args.add(fsyspath(script.getPath()).string());
        params.args.add(fsyspath(release_reader.getPath()).string());
        params.args.add(std::to_string(first_size));
        params.args.add(std::to_string(appended_size));
        params.files.add(LLProcess::FileParam("pipe")); // stdin
        params.files.add(LLProcess::FileParam("pipe")); // stdout
        const auto process = LLProcess::create(params);
        ensure("child process launched", bool(process));
        auto& childin = process->getWritePipe();
        auto& childout = process->getReadPipe(LLProcess::STDOUT);

        const auto wait_until = [&](const auto& done, const char* message)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (!done() && std::chrono::steady_clock::now() < deadline)
            {
                process->pump();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            ensure(message, done());
        };
        wait_until([&] { return childout.contains('\n') || !process->isRunning(); },
                   "child did not become ready");
        ensure_equals("child is waiting before reading stdin", childout.getline(), "ready");

        childin.get_ostream() << std::string(first_size, 'A') << std::flush;
        process->pump();
        // The gated reader cannot drain the pipe. Verify the first write has
        // not completed before appending enough data to relocate its storage.
        ensure_equals("first write must still be pending", childin.size(), first_size);
        childin.get_ostream() << std::string(appended_size, 'B') << std::flush;
        ensure_equals("both payloads remain queued", childin.size(), first_size + appended_size);

        {
            std::ofstream release(release_reader.getPath(), std::ios::binary);
            release << '1';
            release.close();
            ensure("release the child reader", bool(release));
        }
        wait_until([&] { return !process->isRunning() && childout.atEOF(); },
                   "stdin transfer did not finish");
        ensure_equals("pending and appended bytes arrive intact and in order", childout.getline(), "ok");
        ensure_equals("child exited normally", process->getStatus().mState, LLProcess::EXITED);
        ensure_equals("child validated the complete payload", process->getStatus().mData, 0);
        ensure_equals("all queued stdin data was sent", childin.size(), std::size_t(0));
    }
}
// </FS:Beq>
