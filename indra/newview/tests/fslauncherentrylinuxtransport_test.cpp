/**
 * @file fslauncherentrylinuxtransport_test.cpp
 * @brief Isolated LauncherEntry session-bus transport tests.
 */

#include "../llviewerprecompiledheaders.h"
#include "../fslauncherentrylinuxtransport.h"
#include "../test/lltut.h"

#include <gio/gio.h>

#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace tut
{
struct launcher_entry_signal
{
    std::string uri;
    S32 count{0};
    bool visible{false};
    bool variants_valid{false};
};

struct fslauncherentrylinuxtransport_data
{
    GDBusConnection* connection{nullptr};
    guint subscription{0};
    std::vector<launcher_entry_signal> signals;

    fslauncherentrylinuxtransport_data() = default;

    void connectBus()
    {
        if (connection)
        {
            return;
        }

        GError* error = nullptr;
        connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (error)
        {
            g_clear_error(&error);
        }
        if (connection)
        {
            subscription = g_dbus_connection_signal_subscribe(
                connection,
                nullptr,
                "com.canonical.Unity.LauncherEntry",
                "Update",
                "/com/canonical/Unity/LauncherEntry",
                nullptr,
                G_DBUS_SIGNAL_FLAGS_NONE,
                &onSignal,
                this,
                nullptr);
        }
    }

    ~fslauncherentrylinuxtransport_data()
    {
        if (connection && subscription)
        {
            g_dbus_connection_signal_unsubscribe(connection, subscription);
        }
        g_clear_object(&connection);
    }

    static void onSignal(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                         const gchar*, GVariant* parameters, gpointer user_data)
    {
        auto* self = static_cast<fslauncherentrylinuxtransport_data*>(user_data);
        const gchar* uri = nullptr;
        GVariant* properties = nullptr;
        g_variant_get(parameters, "(&s@a{sv})", &uri, &properties);

        launcher_entry_signal signal;
        signal.uri = uri ? uri : "";
        gint32 count = 0;
        gboolean visible = FALSE;
        const gboolean count_valid = g_variant_lookup(properties, "count", "i", &count);
        const gboolean visible_valid = g_variant_lookup(properties, "count-visible", "b", &visible);
        signal.count = static_cast<S32>(count);
        signal.visible = visible != FALSE;
        signal.variants_valid = count_valid && visible_valid;
        self->signals.push_back(std::move(signal));
        g_variant_unref(properties);
    }

    void requireBus()
    {
        connectBus();
        if (!connection)
        {
            skip("session bus unavailable; run this test under dbus-run-session");
        }
    }

    bool waitForSignals(size_t count, unsigned timeout_ms = 1000)
    {
        const guint64 end = g_get_monotonic_time() + timeout_ms * 1000;
        while (signals.size() < count && g_get_monotonic_time() < end)
        {
            while (g_main_context_pending(nullptr))
            {
                g_main_context_iteration(nullptr, false);
            }
            g_usleep(1000);
        }
        return signals.size() >= count;
    }

    void drainEvents(unsigned duration_ms = 100)
    {
        const guint64 end = g_get_monotonic_time() + duration_ms * 1000;
        while (g_get_monotonic_time() < end)
        {
            while (g_main_context_pending(nullptr))
            {
                g_main_context_iteration(nullptr, false);
            }
            g_usleep(1000);
        }
    }
};

typedef test_group<fslauncherentrylinuxtransport_data> fslauncherentrylinuxtransport_group;
typedef fslauncherentrylinuxtransport_group::object object;
fslauncherentrylinuxtransport_group fslauncherentrylinuxtransportgrp("fslauncherentrylinuxtransport");

template<> template<>
void object::test<1>()
{
    set_test_name("desktop identity validation and precedence");
    ensure(FSLauncherEntryLinuxTransport::isValidDesktopId("firestorm-viewer.desktop"));
    ensure(FSLauncherEntryLinuxTransport::isValidDesktopId("downstream_firestorm-1.desktop"));
    ensure(!FSLauncherEntryLinuxTransport::isValidDesktopId("firestorm"));
    ensure(!FSLauncherEntryLinuxTransport::isValidDesktopId("../firestorm.desktop"));
    ensure_equals(FSLauncherEntryLinuxTransport::resolveDesktopId(
                      "configured.desktop", "runtime.desktop"),
                  "runtime.desktop");
    ensure_equals(FSLauncherEntryLinuxTransport::resolveDesktopId(
                      "configured.desktop", "not valid"),
                  "configured.desktop");
    ensure_equals(FSLauncherEntryLinuxTransport::resolveDesktopId(
                      "not valid", nullptr),
                  "firestorm-viewer.desktop");
}

template<> template<>
void object::test<2>()
{
    set_test_name("missing session bus and cancellation are harmless");
    const gchar* previous = g_getenv("DBUS_SESSION_BUS_ADDRESS");
    const std::string saved = previous ? previous : "";
    const bool had_previous = previous != nullptr;
    g_setenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/no/such/firestorm-badge-bus", TRUE);
    {
        FSLauncherEntryLinuxTransport transport("firestorm-viewer.desktop");
        transport.setCount(2);
        for (int i = 0; i < 100; ++i)
        {
            while (g_main_context_pending(nullptr))
            {
                g_main_context_iteration(nullptr, false);
            }
            g_usleep(1000);
        }
    }
    if (had_previous)
    {
        g_setenv("DBUS_SESSION_BUS_ADDRESS", saved.c_str(), TRUE);
    }
    else
    {
        g_unsetenv("DBUS_SESSION_BUS_ADDRESS");
    }
}

template<> template<>
void object::test<3>()
{
    set_test_name("initial signal variants and count transitions");
    requireBus();
    signals.clear();

    FSLauncherEntryLinuxTransport transport("firestorm-viewer.desktop");
    ensure(waitForSignals(1));
    ensure_equals(signals.front().uri, "application://firestorm-viewer.desktop");
    ensure_equals(signals.front().count, 0);
    ensure(!signals.front().visible);
    ensure(signals.front().variants_valid);

    signals.clear();
    transport.setCount(3);
    ensure(waitForSignals(1));
    ensure_equals(signals.back().count, 3);
    ensure(signals.back().visible);
}

template<> template<>
void object::test<4>()
{
    set_test_name("duplicate suppression and preference-like hide/show transitions");
    requireBus();
    signals.clear();

    FSLauncherEntryLinuxTransport transport("firestorm-viewer.desktop");
    ensure(waitForSignals(1));
    drainEvents();
    signals.clear();

    transport.setCount(4);
    ensure(waitForSignals(1));
    const size_t after_first = signals.size();
    transport.setCount(4);
    for (int i = 0; i < 100; ++i)
    {
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, false);
        }
        g_usleep(1000);
    }
    ensure_equals(signals.size(), after_first);

    transport.setCount(-5);
    ensure(waitForSignals(after_first + 1));
    ensure_equals(signals.back().count, 0);
    ensure(!signals.back().visible);

    transport.setCount(4);
    ensure(waitForSignals(after_first + 2));
    ensure_equals(signals.back().count, 4);
    ensure(signals.back().visible);
}

template<> template<>
void object::test<5>()
{
    set_test_name("consumer owner appearance replays current state");
    requireBus();
    signals.clear();

    FSLauncherEntryLinuxTransport transport("firestorm-viewer.desktop");
    ensure(waitForSignals(1));
    drainEvents();
    signals.clear();
    transport.setCount(7);
    ensure(waitForSignals(1));
    signals.clear();

    const guint owner_id = g_bus_own_name(
        G_BUS_TYPE_SESSION,
        "com.canonical.Unity",
        G_BUS_NAME_OWNER_FLAGS_NONE,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    if (!owner_id)
    {
        skip("unable to reserve com.canonical.Unity for replay test");
    }

    if (!waitForSignals(1))
    {
        g_bus_unown_name(owner_id);
        skip("com.canonical.Unity is already owned by another process");
    }
    ensure_equals(signals.back().count, 7);
    ensure(signals.back().visible);
    g_bus_unown_name(owner_id);
}

template<> template<>
void object::test<6>()
{
    set_test_name("shutdown publishes hidden state before teardown");
    requireBus();
    signals.clear();

    auto transport = std::make_unique<FSLauncherEntryLinuxTransport>("firestorm-viewer.desktop");
    ensure(waitForSignals(1));
    drainEvents();
    signals.clear();
    transport->setCount(9);
    ensure(waitForSignals(1));
    ensure_equals(signals.back().count, 9);
    signals.clear();
    transport.reset();
    ensure(waitForSignals(1));
    ensure_equals(signals.back().count, 0);
    ensure(!signals.back().visible);
    ensure(signals.back().variants_valid);
}

template<> template<>
void object::test<7>()
{
    set_test_name("queued acquisition callback is safe after teardown");
    requireBus();
    signals.clear();

    {
        FSLauncherEntryLinuxTransport transport("firestorm-viewer.desktop");
        transport.setCount(2);
    }

    // Iterate the default context after destruction.  The production
    // callback retains invalidation-safe state and must not touch the wrapper.
    for (int i = 0; i < 100; ++i)
    {
        while (g_main_context_pending(nullptr))
        {
            g_main_context_iteration(nullptr, false);
        }
        g_usleep(1000);
    }
}
} // namespace tut
