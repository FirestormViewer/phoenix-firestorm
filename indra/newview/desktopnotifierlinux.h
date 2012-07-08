/* Copyright (c) 2010 Discrete Dreamscape All rights reserved.
 *
 * @file DesktopNotifierLinux.h
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

#ifndef DESKTOPNOTIFIERLINUX_H
#define DESKTOPNOTIFIERLINUX_H

#include <string>
#include "growlnotifier.h"

class DesktopNotifierLinux : public GrowlNotifier
{
	LOG_CLASS( DesktopNotifierLinux );

	struct NDLibnotifyWrapper *m_pLibNotify;
	struct NotifyNotification *m_pNotification;
	std::string m_strIcon;

	DesktopNotifierLinux( DesktopNotifierLinux const & );
	DesktopNotifierLinux& operator=(DesktopNotifierLinux const& );

public:
	DesktopNotifierLinux();
	~DesktopNotifierLinux();

	void showNotification( const std::string& notification_title, const std::string& notification_message, const std::string& notificationTypes );
	bool isUsable();
	void registerApplication(const std::string& application, const std::set<std::string>& notificationTypes);
	bool needsThrottle();
};

#endif // DESKTOPNOTIFIERLINUX_H
