/**
 * @file fslauncherentrylinuxtransport.cpp
 * @brief Asynchronous Unity LauncherEntry badge transport for Linux.
 */

#include "llviewerprecompiledheaders.h"

#include "fslauncherentrylinuxtransport.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <utility>

#if LL_GLIB
#include <gio/gio.h>
#endif

namespace
{
constexpr char DEFAULT_DESKTOP_ID[] = "firestorm-viewer.desktop";
constexpr char LAUNCHER_ENTRY_PATH[] = "/com/canonical/Unity/LauncherEntry";
constexpr char LAUNCHER_ENTRY_INTERFACE[] = "com.canonical.Unity.LauncherEntry";
constexpr char LAUNCHER_ENTRY_MEMBER[] = "Update";

bool isDesktopIdCharacter(char character)
{
    const unsigned char value = static_cast<unsigned char>(character);
    return std::isalnum(value) || character == '.' || character == '-' || character == '_';
}
}

struct FSLauncherEntryLinuxTransport::Impl : public std::enable_shared_from_this<Impl>
{
    explicit Impl(std::string desktop_id)
        : mDesktopId(std::move(desktop_id)),
          mUri("application://" + mDesktopId)
    {
    }

#if LL_GLIB
    void start()
    {
        mCancellable = g_cancellable_new();
        auto* callback_state = new std::shared_ptr<Impl>(shared_from_this());
        g_bus_get(G_BUS_TYPE_SESSION, mCancellable, &Impl::onBusReady, callback_state);
    }
#endif

    ~Impl()
    {
        shutdown();
    }

    void setCount(S32 count)
    {
        mDesiredCount = std::max<S32>(0, count);
#if LL_GLIB
        emitUpdate(false);
#endif
    }

    void shutdown()
    {
        if (!mActive)
        {
            return;
        }

#if LL_GLIB
        // Best effort only: enqueue a hidden state while the shared
        // connection is still usable.  This must never wait for a receiver.
        if (mConnection)
        {
            mDesiredCount = 0;
            emitUpdate(true);
        }
#endif
        mActive = false;
#if LL_GLIB
        if (mCancellable)
        {
            g_cancellable_cancel(mCancellable);
            g_clear_object(&mCancellable);
        }

        if (mConnection && mNameOwnerSubscription)
        {
            g_dbus_connection_signal_unsubscribe(mConnection, mNameOwnerSubscription);
            mNameOwnerSubscription = 0;
        }

        // This is the shared g_bus_get() connection.  Releasing this object's
        // reference must not close it for ViewerAppAPI or another user.
        g_clear_object(&mConnection);
#endif
    }

#if LL_GLIB
    static std::shared_ptr<Impl> takeCallbackState(gpointer user_data)
    {
        auto* state = static_cast<std::shared_ptr<Impl>*>(user_data);
        std::shared_ptr<Impl> result = *state;
        delete state;
        return result;
    }

    static void onBusReady(GObject*, GAsyncResult* result, gpointer user_data)
    {
        std::shared_ptr<Impl> state = takeCallbackState(user_data);
        GError* error = nullptr;
        GDBusConnection* connection = g_bus_get_finish(result, &error);

        if (!state->mActive)
        {
            g_clear_error(&error);
            g_clear_object(&connection);
            return;
        }

        if (!connection)
        {
            // A missing session bus is a normal headless/non-KDE case.  Do
            // not retry or make startup depend on a bus consumer.
            g_clear_error(&error);
            return;
        }

        state->mConnection = connection;
        state->mNameOwnerSubscription = g_dbus_connection_signal_subscribe(
            state->mConnection,
            "org.freedesktop.DBus",
            "org.freedesktop.DBus",
            "NameOwnerChanged",
            "/org/freedesktop/DBus",
            "com.canonical.Unity",
            G_DBUS_SIGNAL_FLAGS_NONE,
            &Impl::onNameOwnerChanged,
            new std::shared_ptr<Impl>(state),
            [](gpointer data) {
                delete static_cast<std::shared_ptr<Impl>*>(data);
            });

        // Establish the full state even if no LauncherEntry consumer is
        // currently listening.  A consumer that appears later is replayed by
        // the NameOwnerChanged subscription above.
        state->emitUpdate(true);
    }

    static void onNameOwnerChanged(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                   const gchar*, GVariant* parameters, gpointer user_data)
    {
        auto* callback_state = static_cast<std::shared_ptr<Impl>*>(user_data);
        const std::shared_ptr<Impl>& state = *callback_state;
        if (!state->mActive || !parameters)
        {
            return;
        }

        const gchar* name = nullptr;
        const gchar* old_owner = nullptr;
        const gchar* new_owner = nullptr;
        g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
        if (name && std::strcmp(name, "com.canonical.Unity") == 0 && new_owner && *new_owner)
        {
            state->emitUpdate(true);
        }
    }

    void emitUpdate(bool force)
    {
        if (!mActive || !mConnection || mEmissionFailed)
        {
            return;
        }

        if (!force && mHasPublished && mPublishedCount == mDesiredCount)
        {
            return;
        }

        GVariantBuilder properties;
        g_variant_builder_init(&properties, G_VARIANT_TYPE_VARDICT);
        g_variant_builder_add(&properties, "{sv}", "count", g_variant_new_int32(mDesiredCount));
        g_variant_builder_add(&properties, "{sv}", "count-visible", g_variant_new_boolean(mDesiredCount > 0));

        GVariant* parameters = g_variant_new("(s@a{sv})",
                                              mUri.c_str(),
                                              g_variant_builder_end(&properties));
        GError* error = nullptr;
        const gboolean queued = g_dbus_connection_emit_signal(
            mConnection,
            nullptr,
            LAUNCHER_ENTRY_PATH,
            LAUNCHER_ENTRY_INTERFACE,
            LAUNCHER_ENTRY_MEMBER,
            parameters,
            &error);
        // g_dbus_connection_emit_signal() consumes the floating parameter
        // variant, including its a{sv} child.
        if (!queued)
        {
            // Keep this failure bounded and make future updates harmless.  A
            // closed bus is not a reason to block the viewer or reconnect
            // synchronously; a new process will establish a new publisher.
            if (!mEmissionFailed)
            {
                mEmissionFailed = true;
                if (error)
                {
                    LL_WARNS("LauncherEntry") << "Unable to publish unread badge: "
                                               << error->message << LL_ENDL;
                }
            }
            g_clear_error(&error);
            return;
        }

        mHasPublished = true;
        mPublishedCount = mDesiredCount;
    }
#endif // LL_GLIB

    std::string mDesktopId;
    std::string mUri;
    S32 mDesiredCount{0};
    S32 mPublishedCount{0};
    bool mHasPublished{false};
    bool mActive{true};
    bool mEmissionFailed{false};
#if LL_GLIB
    GDBusConnection* mConnection{nullptr};
    GCancellable* mCancellable{nullptr};
    guint mNameOwnerSubscription{0};
#endif
};

FSLauncherEntryLinuxTransport::FSLauncherEntryLinuxTransport(std::string desktop_id)
    : mImpl(std::make_shared<Impl>(std::move(desktop_id)))
{
    // Impl starts asynchronous bus acquisition only after shared ownership is
    // established, because its callback state retains the Impl safely.
#if LL_GLIB
    mImpl->start();
#endif
}

FSLauncherEntryLinuxTransport::~FSLauncherEntryLinuxTransport()
{
    if (mImpl)
    {
        mImpl->shutdown();
    }
}

void FSLauncherEntryLinuxTransport::setCount(S32 count)
{
    if (mImpl)
    {
        mImpl->setCount(count);
    }
}

bool FSLauncherEntryLinuxTransport::isValidDesktopId(std::string_view desktop_id)
{
    if (desktop_id.size() < 9 || desktop_id.back() != 'p' ||
        desktop_id.substr(desktop_id.size() - 8) != ".desktop" ||
        !std::isalnum(static_cast<unsigned char>(desktop_id.front())))
    {
        return false;
    }

    return std::all_of(desktop_id.begin(), desktop_id.end(), isDesktopIdCharacter);
}

std::string FSLauncherEntryLinuxTransport::resolveDesktopId(std::string_view configured_desktop_id,
                                                            const char* runtime_override)
{
    if (runtime_override && *runtime_override && isValidDesktopId(runtime_override))
    {
        return runtime_override;
    }

    if (isValidDesktopId(configured_desktop_id))
    {
        return std::string(configured_desktop_id);
    }

    return DEFAULT_DESKTOP_ID;
}
