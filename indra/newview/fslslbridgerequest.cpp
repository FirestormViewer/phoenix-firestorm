/** 
 * @fslslbridgerequest.cpp
 * @FSLSLBridgerequest implementation 
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

#include "llviewerprecompiledheaders.h"

#include "fslslbridgerequest.h"

#include "fscommon.h"
#include "fslslbridge.h"
#include <string>
#include <boost/tokenizer.hpp> // for radar 
#include "llpanelpeople.h"
#include "llavatarlist.h"
#include "llavatarlistitem.h"

#ifdef LL_STANDALONE
#include <expat.h>
#else
#include "expat/expat.h"
#endif

//
//-TT Client LSL Bridge File
//

FSLSLBridgeRequestManager::FSLSLBridgeRequestManager()
{
}

FSLSLBridgeRequestManager::~FSLSLBridgeRequestManager()
{
}

void FSLSLBridgeRequestManager::initSingleton()
{
}
void FSLSLBridgeRequestManager::processBridgeCall(const LLSD& content)
{
	std::string strContent = content.asString();
	llinfos << "Got info: " << strContent << llendl;
}

FSLSLBridgeRequestResponder::FSLSLBridgeRequestResponder() 
{ 
	//FSLSLBridgeRequestManager::instance().initSingleton(); 
}

//If we get back a normal response, handle it here
void FSLSLBridgeRequestResponder::result(const LLSD& content)
{
	//FSLSLBridgeRequestManager::instance().processBridgeCall(content);
	std::string strContent = content.asString();
	llinfos << "Got info: " << strContent << llendl;

	//do not use - infinite loop, only here for testing.
	//FSLSLBridge::instance().viewerToLSL("Response_to_response|" + strContent);
}

//If we get back an error (not found, etc...), handle it here
void FSLSLBridgeRequestResponder::error(U32 status, const std::string& reason)
{
	llwarns << "FSLSLBridgeRequest::error("
	<< status << ": " << reason << ")" << llendl;
}

// AO: The below handler is used to parse return data from the bridge, requesting bulk ZOffset updates.
FSLSLBridgeRequestRadarPosResponder::FSLSLBridgeRequestRadarPosResponder()
{
}
void FSLSLBridgeRequestRadarPosResponder::result(const LLSD& content)
{
	LLPanelPeople* panel_people = getPeoplePanel();
	if (panel_people)
	{
		LLAvatarList* nearbyList = panel_people->getNearbyList();
		
		std::string strContent = content.asString();
		//llinfos << "Got info: " << strContent << llendl;	
		// AO: parse content into pairs of [agent UUID,agent zHeight] , update our peoplepanel radar for each one
		
		LLUUID targetAv;
		F32 targetZ;
		
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(", "); 
		tokenizer tokens(strContent,sep);
		for (tokenizer::iterator tok_iter=tokens.begin(); tok_iter != tokens.end();++tok_iter)
		{
			targetAv = LLUUID(*(tok_iter++));
			targetZ = (F32)::atof((*tok_iter).c_str());
			
			LLAvatarListItem* avListItem = nearbyList->getAvatarListItem(targetAv);
			if (avListItem != NULL)
			{
				avListItem->setZOffset((F32)(targetZ));
				//llinfos << targetAv << " ::: " << targetZ << llendl;
			}
		}
	}
}


