/* Copyright (c) 2010 Discrete Dreamscape All rights reserved.
 *
 * @file DesktopNotifierLinux.cpp
 * @Implementation of desktop notification system (aka growl).
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Discrete Dreamscape nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DISCRETE DREAMSCAPE AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL DISCRETE DREAMSCAPE OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"
#include "desktopnotifierlinux.h"

// ND; LL's version of glib/gtk is so old, that finding a distrib old enough to  to create a libnotify that links against prebuild glib seemed impossible.
// That's why libnotify is loaded dynamically if present.

#include <dlfcn.h>

#include <glib.h>


typedef enum
{
	NOTIFY_URGENCY_LOW,
	NOTIFY_URGENCY_NORMAL,
	NOTIFY_URGENCY_CRITICAL,
} NotifyUrgency;

struct NotifyNotification;

typedef gboolean (*pND_notify_init) ( const char *app_name );
typedef void (*pND_notify_uninit) ( void );
typedef gboolean (*pND_notify_is_initted) ( void );
typedef gboolean (*pND_notify_get_server_info) ( char **ret_name, char **ret_vendor, char **ret_version, char **ret_spec_version );
typedef gboolean (*pND_notify_notification_update) ( NotifyNotification *notification,const char *summary, const char *body, const char *icon );
typedef gboolean (*pND_notify_notification_show) ( NotifyNotification *notification, GError **error );
typedef void (*pND_notify_notification_set_timeout) ( NotifyNotification *notification, gint timeout );
typedef void (*pND_notify_notification_set_category)  ( NotifyNotification *notification, const char *category );
typedef void (*pND_notify_notification_set_urgency) ( NotifyNotification *notification, NotifyUrgency urgency );

// ND; Never versions of notify_notification_new lack the forth parameters.
// As GCC uses cdecl for x86 and and AMD64 ABI for x64 it is safe to always pass the forth parameter. That's due to the fact that the caller cleans the stack and params are passed right to left. So
// the unused one is always pushed first and qualifies just as dead weight.
typedef NotifyNotification* (*pND_notify_notification_new) (const char *summary, const char *body, const char *icon, void*);


void* tryLoadLibnotify()
{
	char const* aNames[] = {
		"libnotify.so",
		"libnotify.so.7",
		"libnotify.so.6",
		"libnotify.so.5",
		"libnotify.so.4",
		"libnotify.so.3",
		"libnotify.so.2",
		"libnotify.so.1",
		0 };

	void *pLibHandle(0);

	for( int i = 0; !pLibHandle && aNames[i]; ++i )
	{
		pLibHandle = dlopen( aNames[i], RTLD_NOW  );
		if( !pLibHandle )
			LL_INFOS( "DesktopNotifierLinux" ) << dlerror() << LL_ENDL;
		else
			LL_INFOS( "DesktopNotifierLinux" ) << "Loaded " << aNames[i] << LL_ENDL;
	}

	return pLibHandle;
};

template< typename tFunc > tFunc dlSym( void * aLibHandle, char const *aSym )
{
	tFunc pRet = reinterpret_cast< tFunc >( dlsym( aLibHandle, aSym ) );
	if( !pRet )
	{
		char const *pErr = dlerror();
		if( !pErr )
			pErr = "0";

		LL_WARNS( "DesktopNotifierLinux" ) << "Error finding symbol '" << aSym << "' in libnotify.so, dlerror: '" << pErr << LL_ENDL;
	}
	else
	{
		LL_INFOS( "DesktopNotifierLinux" ) << "Resolved symbol '" << aSym << "' in libnotify.so to " << (void*)pRet << LL_ENDL;
	}

	return pRet;
}
#define DLSYM( handle, sym ) dlSym<pND_##sym>( handle, #sym )

struct NDLibnotifyWrapper
{
	void *mLibHandle;

	pND_notify_init mInit;
	pND_notify_uninit mUnInit;
	pND_notify_is_initted mIsInitted;
	pND_notify_get_server_info mGetServerInfo;
	pND_notify_notification_update mNotificationUpdate;
	pND_notify_notification_show mNotificationShow;
	pND_notify_notification_set_timeout mNotificationSetTimeout;
	pND_notify_notification_set_category mNotificationSetCategory;
	pND_notify_notification_set_urgency mNotificationSetUrgency;
	pND_notify_notification_new mNotificationNew;

	NDLibnotifyWrapper()
	{
		zeroMember();
	}

	void zeroMember()
	{
		mLibHandle = 0;

		mInit = 0;
		mUnInit = 0;
		mIsInitted = 0;
		mGetServerInfo = 0;
		mNotificationUpdate = 0;
		mNotificationShow = 0;
		mNotificationSetTimeout = 0;
		mNotificationSetCategory = 0;
		mNotificationSetUrgency = 0;
		mNotificationNew = 0;
	}

	~NDLibnotifyWrapper()
	{
		destroy();
	}

	bool isUseable()
	{
		return mInit &&
			mUnInit &&
			mIsInitted &&
			mGetServerInfo &&
			mNotificationUpdate &&
			mNotificationShow &&
			mNotificationSetTimeout &&
			mNotificationSetCategory &&
			mNotificationSetUrgency &&
			mNotificationNew;
	}

	bool init()
	{
		if( isUseable() )
			return true;

		mLibHandle = tryLoadLibnotify();
		if( !mLibHandle )
		{
			LL_WARNS( "DesktopNotifierLinux" ) << "Cannot load libnotify" << LL_ENDL;
			return false;
		}
		else
		{
			LL_INFOS( "DesktopNotifierLinux" ) << "Loaded libnotify at address " << mLibHandle << LL_ENDL;
		}
		
		mInit = DLSYM( mLibHandle, notify_init );
		mUnInit = 	DLSYM( mLibHandle, notify_uninit );
		mIsInitted = DLSYM( mLibHandle, notify_is_initted );
		mGetServerInfo = DLSYM( mLibHandle, notify_get_server_info );
		mNotificationUpdate = DLSYM( mLibHandle, notify_notification_update );
		mNotificationShow = DLSYM( mLibHandle, notify_notification_show );
		mNotificationSetTimeout = DLSYM( mLibHandle, notify_notification_set_timeout );
		mNotificationSetCategory = DLSYM( mLibHandle, notify_notification_set_category );
		mNotificationSetUrgency = DLSYM( mLibHandle, notify_notification_set_urgency );
		mNotificationNew = DLSYM( mLibHandle, notify_notification_new );

		return isUseable();
	}
	
	void destroy()
	{
		if( !mLibHandle )
			return;

		dlclose( mLibHandle );
		zeroMember();
	}

};

const gint NOTIFICATION_TIMEOUT_MS = 5000;

std::string Find_BMP_Resource( bool a_bSmallIcon )
{
	const std::string ICON_128( "firestorm_icon128.png" );
	const std::string ICON_512( "firestorm_icon.png" );

	std::string strRet( gDirUtilp->getAppRODataDir() );
	strRet += gDirUtilp->getDirDelimiter();
	strRet += "res-sdl";
	strRet += gDirUtilp->getDirDelimiter();

	if( a_bSmallIcon )
		strRet += ICON_128;
	else
		strRet += ICON_512;

	return strRet;
}

DesktopNotifierLinux::DesktopNotifierLinux()
{
	m_pLibNotify = new NDLibnotifyWrapper();
	m_pNotification = 0;

	if ( m_pLibNotify->init() && m_pLibNotify->mInit( "Firestorm Viewer" ) )
	{

		LL_INFOS( "DesktopNotifierLinux" ) << "Linux desktop notifications initialized." << LL_ENDL;
		// Find the name of our notification server. I kinda don't expect it to change after the start of the program.
		char *name(0), *vendor(0), *version(0), *spec(0);
		bool info_success = m_pLibNotify->mGetServerInfo( &name, &vendor, &version, &spec );

		if ( info_success )
		{
			LL_INFOS( "DesktopNotifierLinux" ) << "Server name: " << name << LL_ENDL;
			LL_INFOS( "DesktopNotifierLinux" ) << "Server vendor: " << vendor << LL_ENDL;
			LL_INFOS( "DesktopNotifierLinux" ) << "Server version: " << version << LL_ENDL;
			LL_INFOS( "DesktopNotifierLinux" ) << "Server spec: " << spec << LL_ENDL;
		}

		// When talking to notification-daemon use a 128x128 icon to avoid downscaling, all other daemons seem to be ok to scale?
		bool bSmallIcon(false);
		if ( !info_success || strncmp( "notification-daemon", name, 19 ) )
			bSmallIcon = true;

		g_free( name );
		g_free( vendor );
		g_free( version );
		g_free( spec );

		m_strIcon = Find_BMP_Resource( bSmallIcon );

		LL_INFOS( "DesktopNotifierLinux" ) << "Linux desktop notification icon: " << m_strIcon << LL_ENDL;

		m_pNotification = m_pLibNotify->mNotificationNew( "Firestorm", "Intializing", m_strIcon.c_str(), 0 );
	}
	else
	{
		LL_WARNS( "DesktopNotifierLinux" ) << "Linux desktop notifications FAILED to initialize." << LL_ENDL;
	}
}

DesktopNotifierLinux::~DesktopNotifierLinux()
{
	delete m_pLibNotify;
}

void DesktopNotifierLinux::showNotification( const std::string& notification_title, const std::string& notification_message, const std::string& notification_type )
{
	//Dont Log messages that could contain user name info - FS:TM
	//LL_INFOS( "DesktopNotifierLinux" ) << "New notification title: " << notification_title << LL_ENDL;
	//LL_INFOS( "DesktopNotifierLinux" ) << "New notification message: " << notification_message << LL_ENDL;
	//LL_INFOS( "DesktopNotifierLinux" ) << "New notification type: " << notification_type << LL_ENDL;

	m_pLibNotify->mNotificationUpdate( m_pNotification,(gchar*)notification_title.c_str(), (gchar*)notification_message.c_str(), m_strIcon.c_str() );

	m_pLibNotify->mNotificationSetUrgency( m_pNotification, NOTIFY_URGENCY_LOW );
	m_pLibNotify->mNotificationSetCategory( m_pNotification, ( gchar* )notification_type.c_str() );
	m_pLibNotify->mNotificationSetTimeout( m_pNotification, NOTIFICATION_TIMEOUT_MS ); // NotifyOSD ignores this, sadly.

	GError* error(0);
	if ( m_pLibNotify->mNotificationShow( m_pNotification, &error ) )
	{
		LL_INFOS( "DesktopNotifierLinux" ) << "Linux desktop notification type " << notification_type << "sent." << LL_ENDL;
	}
	else
	{
		LL_WARNS( "DesktopNotifierLinux" ) << "Linux desktop notification FAILED to send. " << error->message << LL_ENDL;
	}
}

bool DesktopNotifierLinux::isUsable()
{
	return m_pLibNotify->isUseable() && m_pLibNotify->mIsInitted() && m_pNotification;
}

void DesktopNotifierLinux::registerApplication(const std::string& application, const std::set<std::string>& notificationTypes)
{
	// Do nothing for now.
}

bool DesktopNotifierLinux::needsThrottle()
{
	return false; // NotifyOSD seems to have no issues with handling spam.. How about notification-daemon?
}
