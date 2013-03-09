/** 
 * @fslslbridgerequest.h 
 * @FSLSLBridgerequest header 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_LSLBRIDGEREQUEST_H
#define FS_LSLBRIDGEREQUEST_H

#include "llviewerprecompiledheaders.h"
#include "llhttpclient.h"

//
//-TT Client LSL Bridge File
//

class FSLSLBridgeRequestManager //: public LLSingleton<FSLSLBridgeRequestManager>
{
public:
	FSLSLBridgeRequestManager();
	~FSLSLBridgeRequestManager();

	void processBridgeCall(const LLSD& content);
	/* virtual */ void initSingleton();
private:
	LLSD mBridgeCalls;

	//friend class LLSingleton<FSLSLBridgeRequestManager>;	

};


class FSLSLBridgeRequestResponder : public LLHTTPClient::Responder
{
public:
	FSLSLBridgeRequestResponder(); 
	//If we get back a normal response, handle it here
	virtual void result(const LLSD& content);
	//If we get back an error (not found, etc...), handle it here
	virtual void error(U32 status, const std::string& reason);
};


//
// Responder used by radar for position lookups
//

class FSLSLBridgeRequestRadarPosResponder : public FSLSLBridgeRequestResponder
{
public:
	FSLSLBridgeRequestRadarPosResponder(); 
	//If we get back a normal response, handle it here
	void result(const LLSD& content);
};



#endif // FS_LSLBRIDGEREQUEST_H
