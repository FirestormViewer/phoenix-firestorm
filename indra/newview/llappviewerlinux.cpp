/**
 * @file llappviewerlinux.cpp
 * @brief The LLAppViewerLinux class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llappviewerlinux.h"

#include "llcommandlineparser.h"

#include "lldiriterator.h"
#include "llurldispatcher.h"        // SLURL from other app instance
#include "llviewernetwork.h"
#include "llviewercontrol.h"
#include "llwindowsdl.h"
#include "llmd5.h"
#include "llfindlocale.h"

#include <exception>

#include <gio/gio.h>
#include <resolv.h>

#if (__GLIBC__*1000 + __GLIBC_MINOR__) >= 2034
#pragma message "Compiling with libresolv stubs"
extern "C"
{
  int __res_nquery(res_state statep,
                   const char *dname, int qclass, int type,
                   unsigned char *answer, int anslen)
  {
    return res_nquery( statep, dname, qclass, type, answer, anslen );
  }

  int __dn_expand(const unsigned char *msg,
                  const unsigned char *eomorig,
                  const unsigned char *comp_dn, char *exp_dn,
                  int length)
  {
    return dn_expand( msg,eomorig,comp_dn,exp_dn,length);
  }
}
#endif

#if LL_SEND_CRASH_REPORTS
#include "breakpad/client/linux/handler/exception_handler.h"
#include "breakpad/common/linux/http_upload.h"
#include "lldir.h"
#include "../llcrashlogger/llcrashlogger.h"
#endif

#include "boost/json.hpp"
#include "fsversionvalues.h"

#define VIEWERAPI_SERVICE "com.secondlife.ViewerAppAPIService"
#define VIEWERAPI_PATH "/com/secondlife/ViewerAppAPI"
#define VIEWERAPI_INTERFACE "com.secondlife.ViewerAppAPI"

static const char * DBUS_SERVER = "<node name=\"/com/secondlife/ViewerAppAPI\">\n"
                                  "  <interface name=\"com.secondlife.ViewerAppAPI\">\n"
                                  "    <annotation name=\"org.freedesktop.DBus.GLib.CSymbol\" value=\"viewer_app_api\"/>\n"
                                  "    <method name=\"GoSLURL\">\n"
                                  "      <annotation name=\"org.freedesktop.DBus.GLib.CSymbol\" value=\"dispatchSLURL\"/>\n"
                                  "      <arg type=\"s\" name=\"slurl\" direction=\"in\" />\n"
                                  "    </method>\n"
                                  "  </interface>\n"
                                  "</node>";

typedef struct
{
    GObject parent;
} ViewerAppAPI;

namespace
{
    int gArgC = 0;
    char **gArgV = NULL;
    void (*gOldTerminateHandler)() = NULL;
}

// Initialize static members
guint32 LLAppViewerLinux::sPowerInhibitCookie = 0;
bool LLAppViewerLinux::sPowerInhibitActive = false;


static void exceptionTerminateHandler()
{
    // reinstall default terminate() handler in case we re-terminate.
    if (gOldTerminateHandler) std::set_terminate(gOldTerminateHandler);
    // treat this like a regular viewer crash, with nice stacktrace etc.
    long *null_ptr;
    null_ptr = 0;
    *null_ptr = 0xDEADBEEF; //Force an exception that will trigger breakpad.
    // we've probably been killed-off before now, but...
    gOldTerminateHandler(); // call old terminate() handler
}

int main( int argc, char **argv )
{
    // Call Tracy first thing to have it allocate memory
    // https://github.com/wolfpld/tracy/issues/196
    LL_PROFILER_FRAME_END;
    LL_PROFILER_SET_THREAD_NAME("App");

    gArgC = argc;
    gArgV = argv;

    LLAppViewer* viewer_app_ptr = new LLAppViewerLinux();

    // install unexpected exception handler
    gOldTerminateHandler = std::set_terminate(exceptionTerminateHandler);

    unsetenv( "LD_PRELOAD" ); // <FS:ND/> Get rid of any preloading, we do not want this to happen during startup of plugins.

    bool ok = viewer_app_ptr->init();
    if(!ok)
    {
        // <FS:PP> FIRE-36191: A secondary helper process can intentionally quit after handing an external SLURL to the already-running viewer instance
        if (LLApp::isQuitting())
        {
            return 0;
        }
        // </FS:PP>
        LL_WARNS() << "Application init failed." << LL_ENDL;
        return -1;
    }

        // Run the application main loop
    while (! viewer_app_ptr->frame())
    {}

    if (!LLApp::isError())
    {
        //
        // We don't want to do cleanup here if the error handler got called -
        // the assumption is that the error handler is responsible for doing
        // app cleanup if there was a problem.
        //
        viewer_app_ptr->cleanup();
    }
    delete viewer_app_ptr;
    viewer_app_ptr = NULL;
    return 0;
}

LLAppViewerLinux::LLAppViewerLinux()
{
}

LLAppViewerLinux::~LLAppViewerLinux()
{
    // Clean up any power management inhibition on exit
    if (sPowerInhibitActive)
    {
        uninhibitPowerManagement();
    }
}

#if LL_SEND_CRASH_REPORTS
std::string gCrashLogger;
std::string gVersion;
std::string gBugsplatDB;
std::string gCrashBehavior;

static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)
{
    if( fork() == 0 )
        execl( gCrashLogger.c_str(), gCrashLogger.c_str(), descriptor.path(), gVersion.c_str(), gBugsplatDB.c_str(),  gCrashBehavior.c_str(), nullptr );
    return succeeded;
}

void setupBreakpad()
{
    std::string build_data_fname(gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "build_data.json"));
    gCrashLogger =  gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "linux-crash-logger.bin");

    llifstream inf(build_data_fname.c_str());
    if(!inf.is_open())
    {
        LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, can't read '" << build_data_fname << "'" << LL_ENDL;
        return;
    }

    boost::system::error_code ec;
    boost::json::value build_data = boost::json::parse(inf, ec);
    if(ec.failed())
    {
        LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, can't parse '" << build_data_fname << "': "
                             << ec.what() << LL_ENDL;
        return;
    }

    if (!build_data.is_object() || !build_data.as_object().contains("BugSplat DB"))
    {
        LL_WARNS("BUGSPLAT") << "Can't initialize BugSplat, no 'BugSplat DB' entry in '" << build_data_fname
                             << "'" << LL_ENDL;
        return;
    }

    gVersion = STRINGIZE(
            LL_VIEWER_VERSION_MAJOR << '.' << LL_VIEWER_VERSION_MINOR << '.' << LL_VIEWER_VERSION_PATCH
                                    << '.' << LL_VIEWER_VERSION_BUILD);

    boost::json::value BugSplat_DB = build_data.at("BugSplat DB");
    gBugsplatDB = boost::json::value_to<std::string>(BugSplat_DB);

    LL_INFOS("BUGSPLAT") << "Initializing with crash logger: " << gCrashLogger << " database: " << gBugsplatDB << " version: " << gVersion << LL_ENDL;

    google_breakpad::MinidumpDescriptor *descriptor = new google_breakpad::MinidumpDescriptor(gDirUtilp->getExpandedFilename(LL_PATH_DUMP, ""));
    google_breakpad::ExceptionHandler *eh = new google_breakpad::ExceptionHandler(*descriptor, NULL, dumpCallback, NULL, true, -1);
}
#endif

bool LLAppViewerLinux::init()
{
    // g_thread_init() must be called before *any* use of glib, *and*
    // before any mutexes are held, *and* some of our third-party
    // libraries likes to use glib functions; in short, do this here
    // really early in app startup!

    bool success = LLAppViewer::init();

#if LL_SEND_CRASH_REPORTS
    S32 nCrashSubmitBehavior = gCrashSettings.getS32("CrashSubmitBehavior");

    // For the first version we just consider always send and create a nice dialog for CRASH_BEHAVIOR_ASK later.
    if (success && nCrashSubmitBehavior != CRASH_BEHAVIOR_NEVER_SEND )
    {
        if( nCrashSubmitBehavior == CRASH_BEHAVIOR_ASK )
            gCrashBehavior = "ask";
        else
            gCrashBehavior = "send";
        setupBreakpad();
    }
#endif

    return success;
}

bool LLAppViewerLinux::restoreErrorTrap()
{
    // *NOTE:Mani there is a case for implementing this on the mac.
    // Linux doesn't need it to my knowledge.
    return true;
}

/////////////////////////////////////////
#if LL_GLIB

typedef struct
{
        GObjectClass parent_class;
} ViewerAppAPIClass;

///

static void viewerappapi_init(ViewerAppAPI *server);
static void viewerappapi_class_init(ViewerAppAPIClass *klass);

G_DEFINE_TYPE(ViewerAppAPI, viewerappapi, G_TYPE_OBJECT);

void viewerappapi_class_init(ViewerAppAPIClass *klass)
{
}

static void dispatchSLURL(gchar const *slurl)
{
    LL_INFOS() << "Was asked to go to slurl: " << slurl << LL_ENDL;

    std::string url = slurl;
    LLMediaCtrl* web = NULL;
    const bool trusted_browser = false;
    LLURLDispatcher::dispatch(url, "", web, trusted_browser);
}

static void DoMethodeCall (GDBusConnection       *connection,
                                const gchar           *sender,
                                const gchar           *object_path,
                                const gchar           *interface_name,
                                const gchar           *method_name,
                                GVariant              *parameters,
                                GDBusMethodInvocation *invocation,
                                gpointer               user_data)
{
    LL_INFOS() << "DBUS message " << method_name << "  from: " << sender << " interface: " << interface_name << LL_ENDL;
    const gchar *slurl;

    g_variant_get (parameters, "(&s)", &slurl);
    dispatchSLURL(slurl);
}

GDBusNodeInfo *gBusNodeInfo = nullptr;
static const GDBusInterfaceVTable interface_vtable =
        {
                DoMethodeCall
        };
static void busAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
    auto id = g_dbus_connection_register_object(connection,
                                                VIEWERAPI_PATH,
                                                gBusNodeInfo->interfaces[0],
                                                &interface_vtable,
                                                NULL,  /* user_data */
                                                             NULL,  /* user_data_free_func */
                                                             NULL); /* GError** */
    g_assert (id > 0);
}

static void nameAcquired(GDBusConnection *connection, const gchar *name, gpointer user_data)
{
}

static void nameLost(GDBusConnection *connection, const gchar *name, gpointer user_data)
{

}
void viewerappapi_init(ViewerAppAPI *server)
{
    gBusNodeInfo = g_dbus_node_info_new_for_xml (DBUS_SERVER, NULL);
    g_assert (gBusNodeInfo != NULL);

    g_bus_own_name(G_BUS_TYPE_SESSION,
                   VIEWERAPI_SERVICE,
                   G_BUS_NAME_OWNER_FLAGS_NONE,
                   busAcquired,
                   nameAcquired,
                   nameLost,
                   NULL,
                   NULL);

}

///

//virtual
bool LLAppViewerLinux::initSLURLHandler()
{
    //ViewerAppAPI *api_server = (ViewerAppAPI*)
    g_object_new(viewerappapi_get_type(), NULL);

    return true;
}

//virtual
bool LLAppViewerLinux::sendURLToOtherInstance(const std::string& url)
{
    auto *pBus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, nullptr);

    if( !pBus )
    {
        LL_WARNS() << "Getting dbus failed." << LL_ENDL;
        return false;
    }

    auto pProxy = g_dbus_proxy_new_sync(pBus, G_DBUS_PROXY_FLAGS_NONE, nullptr,
                                        VIEWERAPI_SERVICE, VIEWERAPI_PATH,
                                        VIEWERAPI_INTERFACE, nullptr, nullptr);

    if( !pProxy )
    {
        LL_WARNS() << "Cannot create new dbus proxy." << LL_ENDL;
        g_object_unref( pBus );
        return false;
    }

    auto *pArgs = g_variant_new( "(s)", url.c_str() );
    if( !pArgs )
    {
        LL_WARNS() << "Cannot create new variant." << LL_ENDL;
        g_object_unref( pBus );
        return false;
    }

    auto pRes  = g_dbus_proxy_call_sync(pProxy,
                                        "GoSLURL",
                                        pArgs,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1, nullptr, nullptr);



    if( pRes )
        g_variant_unref( pRes );
    g_object_unref( pProxy );
    g_object_unref( pBus );
    return true;
}

#else // LL_GLIB
bool LLAppViewerLinux::initSLURLHandler()
{
    return false; // not implemented without dbus
}
bool LLAppViewerLinux::sendURLToOtherInstance(const std::string& url)
{
    return false; // not implemented without dbus
}
#endif // LL_GLIB


void LLAppViewerLinux::setOSHibernationMode(eHibernationMode mode)
{
    if (mode == LL_HIBERNATE_MODE_DEFAULT)
    {
        // Allow OS to sleep/hibernate - remove any inhibition
        if (sPowerInhibitActive)
        {
            uninhibitPowerManagement();
            LL_INFOS("OS") << "Permitted OS hibernation/sleep" << LL_ENDL;
        }
    }
    else if (mode == LL_HIBERNATE_MODE_PREVENT)
    {
        // Prevent system sleep, but allow display to turn off
        // Release any existing inhibition first to allow mode switching
        if (sPowerInhibitActive)
        {
            uninhibitPowerManagement();
        }

        if (inhibitPowerManagement(false))
        {
            LL_INFOS("OS") << "Prevented OS hibernation/sleep, display sleep allowed" << LL_ENDL;
        }
        else
        {
            LL_WARNS("OS") << "Failed to prevent OS hibernation/sleep" << LL_ENDL;
        }
    }
    else if (mode == LL_HIBERNATE_MODE_PREVENT_SCREEN)
    {
        // Prevent both system and display sleep
        // Release any existing inhibition first to allow mode switching
        if (sPowerInhibitActive)
        {
            uninhibitPowerManagement();
        }

        if (inhibitPowerManagement(true))
        {
            LL_INFOS("OS") << "Prevented OS hibernation/sleep and display sleep" << LL_ENDL;
        }
        else
        {
            LL_WARNS("OS") << "Failed to prevent OS hibernation/sleep and display sleep" << LL_ENDL;
        }
    }
}

// TODO: This is AI Generated!!!, needs review and testing.
bool LLAppViewerLinux::inhibitPowerManagement(bool inhibit_display)
{
#if LL_DBUS_ENABLED
    // Try to use D-Bus to inhibit power management via various desktop environment APIs
    // This works with GNOME, KDE, XFCE, and most modern Linux desktop environments

    if (!grab_dbus_syms(DBUSGLIB_DYLIB_DEFAULT_NAME))
    {
        LL_WARNS("OS") << "Failed to load D-Bus symbols for power management" << LL_ENDL;
        return false;
    }

    GError* error = nullptr;
    DBusGConnection* bus = lldbus_g_bus_get(DBUS_BUS_SESSION, &error);

    if (!bus)
    {
        LL_WARNS("OS") << "Failed to connect to D-Bus session bus: "
            << (error ? error->message : "unknown error") << LL_ENDL;
        if (error)
            g_error_free(error);
        return false;
    }

    // Try multiple power management services in order of preference
    // 1. org.freedesktop.PowerManagement (older standard)
    // 2. org.gnome.SessionManager (GNOME)
    // 3. org.kde.Solid.PowerManagement (KDE)

    const char* services[] = {
        "org.freedesktop.PowerManagement",
        "org.gnome.SessionManager",
        "org.kde.Solid.PowerManagement"
    };

    const char* paths[] = {
        "/org/freedesktop/PowerManagement/Inhibit",
        "/org/gnome/SessionManager",
        "/org/kde/Solid/PowerManagement"
    };

    const char* interfaces[] = {
        "org.freedesktop.PowerManagement.Inhibit",
        "org.gnome.SessionManager",
        "org.kde.Solid.PowerManagement"
    };

    const char* methods[] = {
        "Inhibit",
        "Inhibit",
        "inhibit"
    };

    bool success = false;

    for (int i = 0; i < 3 && !success; ++i)
    {
        DBusGProxy* proxy = lldbus_g_proxy_new_for_name(
            bus,
            services[i],
            paths[i],
            interfaces[i]
        );

        if (!proxy)
            continue;

        error = nullptr;
        guint32 cookie = 0;

        if (i == 0) // freedesktop.PowerManagement
        {
            // Inhibit(application_name: s, reason: s) -> cookie: u
            success = lldbus_g_proxy_call(
                proxy,
                methods[i],
                &error,
                G_TYPE_STRING, "Second Life Viewer",
                G_TYPE_STRING, inhibit_display ?
                "Viewer active - preventing system and display sleep" :
                "Viewer active - preventing system sleep",
                G_TYPE_INVALID,
                G_TYPE_UINT, &cookie,
                G_TYPE_INVALID
            );
        }
        else if (i == 1) // GNOME SessionManager
        {
            // Inhibit(app_id: s, toplevel_xid: u, reason: s, flags: u) -> cookie: u
            // flags: 4 = suspend, 8 = idle (display), 12 = both
            guint32 flags = inhibit_display ? 12 : 4;
            success = lldbus_g_proxy_call(
                proxy,
                methods[i],
                &error,
                G_TYPE_STRING, "SecondLifeViewer",
                G_TYPE_UINT, 0,  // toplevel_xid (0 = none)
                G_TYPE_STRING, inhibit_display ?
                "Viewer active - preventing system and display sleep" :
                "Viewer active - preventing system sleep",
                G_TYPE_UINT, flags,
                G_TYPE_INVALID,
                G_TYPE_UINT, &cookie,
                G_TYPE_INVALID
            );
        }
        else if (i == 2) // KDE Solid
        {
            // Different method signature for KDE
            success = lldbus_g_proxy_call(
                proxy,
                methods[i],
                &error,
                G_TYPE_INVALID,
                G_TYPE_INT, &cookie,
                G_TYPE_INVALID
            );
        }

        if (success)
        {
            sPowerInhibitCookie = cookie;
            sPowerInhibitActive = true;
            LL_INFOS("OS") << "Successfully inhibited power management using "
                << services[i] << LL_ENDL;
        }
        else if (error)
        {
            LL_DEBUGS("OS") << "Failed to inhibit via " << services[i]
                << ": " << error->message << LL_ENDL;
            g_error_free(error);
            error = nullptr;
        }

        g_object_unref(proxy);
    }

    return success;

#else // !LL_DBUS_ENABLED
    LL_WARNS("OS") << "Power management control not available - D-Bus support not enabled" << LL_ENDL;
    return false;
#endif
}

void LLAppViewerLinux::uninhibitPowerManagement()
{
#if LL_DBUS_ENABLED
    if (!sPowerInhibitActive || sPowerInhibitCookie == 0)
    {
        return;
    }

    if (!grab_dbus_syms(DBUSGLIB_DYLIB_DEFAULT_NAME))
    {
        LL_WARNS("OS") << "Failed to load D-Bus symbols for power management uninhibit" << LL_ENDL;
        return;
    }

    GError* error = nullptr;
    DBusGConnection* bus = lldbus_g_bus_get(DBUS_BUS_SESSION, &error);

    if (!bus)
    {
        if (error)
            g_error_free(error);
        return;
    }

    // Try to uninhibit using all services that might have been used
    const char* services[] = {
        "org.freedesktop.PowerManagement",
        "org.gnome.SessionManager",
        "org.kde.Solid.PowerManagement"
    };

    const char* paths[] = {
        "/org/freedesktop/PowerManagement/Inhibit",
        "/org/gnome/SessionManager",
        "/org/kde/Solid/PowerManagement"
    };

    const char* interfaces[] = {
        "org.freedesktop.PowerManagement.Inhibit",
        "org.gnome.SessionManager",
        "org.kde.Solid.PowerManagement"
    };

    const char* methods[] = {
        "UnInhibit",
        "Uninhibit",
        "uninhibit"
    };

    bool success = false;

    for (int i = 0; i < 3; ++i)
    {
        DBusGProxy* proxy = lldbus_g_proxy_new_for_name(
            bus,
            services[i],
            paths[i],
            interfaces[i]
        );

        if (!proxy)
            continue;

        error = nullptr;

        if (lldbus_g_proxy_call(
            proxy,
            methods[i],
            &error,
            G_TYPE_UINT, sPowerInhibitCookie,
            G_TYPE_INVALID,
            G_TYPE_INVALID))
        {
            success = true;
            LL_INFOS("OS") << "Successfully uninhibited power management using "
                << services[i] << LL_ENDL;
        }
        else if (error)
        {
            LL_DEBUGS("OS") << "Failed to uninhibit via " << services[i]
                << ": " << error->message << LL_ENDL;
            g_error_free(error);
            error = nullptr;
        }

        g_object_unref(proxy);

        if (success)
            break;
    }

    sPowerInhibitCookie = 0;
    sPowerInhibitActive = false;

#endif // LL_DBUS_ENABLED
}

void LLAppViewerLinux::initCrashReporting(bool reportFreeze)
{
    std::string cmd =gDirUtilp->getExecutableDir();
    cmd += gDirUtilp->getDirDelimiter();
#if LL_LINUX
    cmd += "linux-crash-logger.bin";
#else
# error Unknown platform
#endif

    std::stringstream pid_str;
    pid_str <<  LLApp::getPid();
    std::string logdir = gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "");
    std::string appname = gDirUtilp->getExecutableFilename();
    std::string grid{ LLGridManager::getInstance()->getGridId() };
    std::string title{ LLAppViewer::instance()->getSecondLifeTitle() };
    std::string pidstr{ pid_str.str() };
    // launch the actual crash logger
    const char * cmdargv[] =
        {cmd.c_str(),
         "-user",
         grid.c_str(),
         "-name",
         title.c_str(),
         "-pid",
         pidstr.c_str(),
         "-dumpdir",
         logdir.c_str(),
         "-procname",
         appname.c_str(),
         NULL};
    fflush(NULL);

    pid_t pid = fork();
    if (pid == 0)
    { // child
        execv(cmd.c_str(), (char* const*) cmdargv);     /* Flawfinder: ignore */
        LL_WARNS() << "execv failure when trying to start " << cmd << LL_ENDL;
        _exit(1); // avoid atexit()
    }
    else
    {
        if (pid > 0)
        {
            // DO NOT wait for child proc to die; we want
            // the logger to outlive us while we quit to
            // free up the screen/keyboard/etc.
            ////int childExitStatus;
            ////waitpid(pid, &childExitStatus, 0);
        }
        else
        {
            LL_WARNS() << "fork failure." << LL_ENDL;
        }
    }
    // Sometimes signals don't seem to quit the viewer.  Also, we may
    // have been called explicitly instead of from a signal handler.
    // Make sure we exit so as to not totally confuse the user.
    //_exit(1); // avoid atexit(), else we may re-crash in dtors.
}

bool LLAppViewerLinux::beingDebugged()
{
    static enum {unknown, no, yes} debugged = unknown;

    if (debugged == unknown)
    {
        pid_t ppid = getppid();
        char *name;
        int ret;

        ret = asprintf(&name, "/proc/%d/exe", ppid);
        if (ret != -1)
        {
            char buf[1024];
            ssize_t n;

            n = readlink(name, buf, sizeof(buf) - 1);
            if (n != -1)
            {
                char *base = strrchr(buf, '/');
                buf[n + 1] = '\0';
                if (base == NULL)
                {
                    base = buf;
                } else {
                    base += 1;
                }

                if (strcmp(base, "gdb") == 0)
                {
                    debugged = yes;
                }
            }
            free(name);
        }
    }

    return debugged == yes;
}

void LLAppViewerLinux::initLoggingAndGetLastDuration()
{
    // Remove the last stack trace, if any
    // This file is no longer created, since the move to Google Breakpad
    // The code is left here to clean out any old state in the log dir
    std::string old_stack_file =
        gDirUtilp->getExpandedFilename(LL_PATH_LOGS,"stack_trace.log");
    LLFile::remove(old_stack_file);

    LLAppViewer::initLoggingAndGetLastDuration();
}

bool LLAppViewerLinux::initParseCommandLine(LLCommandLineParser& clp)
{
    if (!clp.parseCommandLine(gArgC, gArgV))
    {
        return false;
    }

    // Find the system language.
    FL_Locale *locale = NULL;
    FL_Success success = FL_FindLocale(&locale, FL_MESSAGES);
    if (success != 0)
    {
        if (success >= 2 && locale->lang) // confident!
        {
            LL_INFOS("AppInit") << "Language " << ll_safe_string(locale->lang) << LL_ENDL;
            LL_INFOS("AppInit") << "Location " << ll_safe_string(locale->country) << LL_ENDL;
            LL_INFOS("AppInit") << "Variant " << ll_safe_string(locale->variant) << LL_ENDL;

            LLControlVariable* c = gSavedSettings.getControl("SystemLanguage");
            if(c)
            {
                c->setValue(std::string(locale->lang), false);
            }
        }
    }
    FL_FreeLocale(&locale);

    return true;
}

std::string LLAppViewerLinux::generateSerialNumber()
{
    char serial_md5[MD5HEX_STR_SIZE];
    serial_md5[0] = 0;
    std::string best;
    std::string uuiddir("/dev/disk/by-uuid/");

    // trawl /dev/disk/by-uuid looking for a good-looking UUID to grab
    std::string this_name;

    LLDirIterator iter(uuiddir, "*");
    while (iter.next(this_name))
    {
        if (this_name.length() > best.length() ||
            (this_name.length() == best.length() &&
             this_name > best))
        {
            // longest (and secondarily alphabetically last) so far
            best = this_name;
        }
    }

    // we don't return the actual serial number, just a hash of it.
    LLMD5 md5( reinterpret_cast<const unsigned char*>(best.c_str()) );
    md5.hex_digest(serial_md5);

    return serial_md5;
}
