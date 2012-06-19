/** 
 * @file fsslurl.cpp (was llsimurlstring.cpp)
 * @brief Handles "SLURL fragments" like Ahern/123/45 for
 * startup processing, login screen, prefs, etc.
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
 * Based on Second Life Viewer Source Code llslurl.cpp
 * Copyright (C) 2010, Linden Research, Inc.
 * With modifications Copyright (C) 2012, arminweatherwax@lavabit.com
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA 94111 USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llslurl.h"

#include "llpanellogin.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llfiltersd2xmlrpc.h"
#include "curl/curl.h"
#include <boost/algorithm/string.hpp>
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.0d)
#include "rlvhandler.h"
// [/RLVa:KB]
const char* LLSLURL::HOP_SCHEME		 = "hop"; // <AW: hop:// protocol>
const char* LLSLURL::SLURL_HTTP_SCHEME		 = "http";
const char* LLSLURL::SLURL_HTTPS_SCHEME		 = "https";
const char* LLSLURL::SLURL_SECONDLIFE_SCHEME	 = "secondlife";
const char* LLSLURL::SLURL_X_GRID_LOCATION_INFO_SCHEME = "x-grid-location-info";

// For DnD - even though www.slurl.com redirects to slurl.com in a browser, you  can copy and drag
// text with www.slurl.com or a link explicitly pointing at www.slurl.com so testing for this
// version is required also.

const char* LLSLURL::WWW_SLURL_COM				 = "www.slurl.com";
const char* LLSLURL::MAPS_SECONDLIFE_COM		 = "maps.secondlife.com";

// <AW: opensim>
const char* LLSLURL::SLURL_COM           = "slurl.com";
const char* LLSLURL::SLURL_APP_PATH              = "app";
const char* LLSLURL::SLURL_SECONDLIFE_PATH	 = "secondlife";
const char* LLSLURL::SLURL_REGION_PATH           = "region";
const char* LLSLURL::SIM_LOCATION_HOME           = "home";
const char* LLSLURL::SIM_LOCATION_LAST           = "last";
// </AW: opensim>

// resolve a simstring from a slurl
LLSLURL::LLSLURL(const std::string& slurl)
: mHypergrid(false)
{
	// by default we go to agni.
	mType = INVALID;
	LL_INFOS("AppInit") << "SLURL: " << slurl << LL_ENDL;
	LL_DEBUGS("SLURL") << "SLURL: " << slurl << LL_ENDL;
	if(slurl == SIM_LOCATION_HOME)
	{
		mType = HOME_LOCATION;
	}
	else if(slurl.empty() || (slurl == SIM_LOCATION_LAST))
	{

		mType = LAST_LOCATION;
	}
	else
	{
		LLURI slurl_uri;
		// parse the slurl as a uri
		if(slurl.find("://") == std::string::npos)
		{
			// There may be no scheme ('secondlife:' etc.) passed in.  In that case
			// we want to normalize the slurl by putting the appropriate scheme
			// in front of the slurl.  So, we grab the appropriate slurl base
			// from the grid manager which may be http://slurl.com/secondlife/ for maingrid, or
			// https://<hostname>/region/ for Standalone grid (the word region, not the region name)
			// these slurls are typically passed in from the 'starting location' box on the login panel,
			// where the user can type in <regionname>/<x>/<y>/<z>
			std::string fixed_slurl = LLGridManager::getInstance()->getSLURLBase();
			// the slurl that was passed in might have a prepended /, or not.  So,
			// we strip off the prepended '/' so we don't end up with http://slurl.com/secondlife/<region>/<x>/<y>/<z>
			// or some such.
			
			if(slurl[0] == '/')
		    {
				fixed_slurl += slurl.substr(1);
		    }
			else
		    {
				fixed_slurl += slurl;
		    }
			// We then load the slurl into a LLURI form
			slurl_uri = LLURI(fixed_slurl);
		}
		else if (std::string::npos == slurl.find("|!!"))
		{
			// as we did have a scheme, implying a URI style slurl, we
			// simply parse it as a URI
			slurl_uri = LLURI(slurl);
			LL_DEBUGS("SLURL") << "standard slurl" << LL_ENDL;
		}
		else
		{
			mHypergrid = true;

			std::string hop = slurl;
			std::string match = "|!!";
			size_t pos = hop.find(match);
			hop.erase( 0,pos+match.length());

			boost::replace_all(hop, "|", ":");
			boost::replace_all(hop, "!", "/");
			boost::replace_first(hop, "+", "/");
			boost::replace_all(hop, "+", " ");

			hop = "hop://" + hop;
			slurl_uri = LLURI(hop);

			LL_DEBUGS("SLURL") << "hypergrid slurl " << hop <<LL_ENDL;
		}
		
		LLSD path_array = slurl_uri.pathArray();
		
		// determine whether it's a maingrid URI or an Standalone/open style URI
		// by looking at the scheme.  If it's a 'secondlife:' slurl scheme or
		// 'sl:' scheme, we know it's maingrid
		
		// At the end of this if/else block, we'll have determined the grid,
		// and the slurl type (APP or LOCATION)

		// default to current
		std::string default_grid = LLGridManager::getInstance()->getGrid();
		mGrid = default_grid;

		if(slurl_uri.scheme() == LLSLURL::SLURL_SECONDLIFE_SCHEME)
		{
			LL_DEBUGS("SLURL") << "secondlife scheme" << LL_ENDL;
			// parse a maingrid style slurl.  We know the grid is maingrid
			// so grab it.
			// A location slurl for maingrid (with the special schemes) can be in the form
			// secondlife://<regionname>/<x>/<y>/<z>
			// or
			// secondlife://<Grid>/secondlife/<region>/<x>/<y>/<z>
			// where if grid is empty, it specifies Agni
			
			// An app style slurl for maingrid can be
			// secondlife://<Grid>/app/<app parameters>
			// where an empty grid implies Agni
			
			// we'll start by checking the top of the 'path' which will be 
			// either 'app', 'secondlife', or <x>.

			LL_DEBUGS("SLURL") << "slurl_uri.hostNameAndPort(): " << slurl_uri.hostNameAndPort() << LL_ENDL;
			LL_DEBUGS("SLURL") << "path_array[0]: " << path_array[0].asString() << LL_ENDL;

			if ((path_array[0].asString() == LLSLURL::SLURL_SECONDLIFE_PATH) ||
				(path_array[0].asString() == LLSLURL::SLURL_APP_PATH))
		    {
				// it's in the form secondlife://<grid>/(app|secondlife)
				// so parse the grid name to derive the grid ID
				if (!slurl_uri.hostNameAndPort().empty())
				{
					LL_DEBUGS("SLURL") << "secondlife://<grid>/(app|secondlife)" << LL_ENDL;

					mGrid = LLGridManager::getInstance()->getGridByProbing(slurl_uri.hostNameAndPort());
					if (mGrid.empty())
						mGrid = 
						  LLGridManager::getInstance()->getGridByProbing(slurl_uri.hostName());
					if (mGrid.empty())
						mGrid = default_grid;
				}
				else if(path_array[0].asString() == LLSLURL::SLURL_SECONDLIFE_PATH)
				{
					LL_DEBUGS("SLURL") << "secondlife:///secondlife/<region>" << LL_ENDL;
					// If the slurl is in the form secondlife:///secondlife/<region> form, 
					// then we are in fact on maingrid.  
					mGrid = default_grid;
				}
				else if(path_array[0].asString() == LLSLURL::SLURL_APP_PATH)
				{
					LL_DEBUGS("SLURL") << "app style slurls, no grid name specified" << LL_ENDL;
					// for app style slurls, where no grid name is specified, assume the currently
					// selected or logged in grid.
					mGrid = default_grid;
				}

				if(mGrid.empty())
				{
					LL_DEBUGS("SLURL") << "couldn't find the grid so bail" << LL_ENDL;
					// we couldn't find the grid in the grid manager, so bail
					return;
				}
				// set the type as appropriate.
				if(path_array[0].asString() == LLSLURL::SLURL_SECONDLIFE_PATH)
				{
					mType = LOCATION;
				}
				else
				{
					mType = APP;
				}
				path_array.erase(0);
		    }
			else
		    {
				LL_DEBUGS("SLURL") << "secondlife://<region>" << LL_ENDL;
				// it wasn't a /secondlife/<region> or /app/<params>, so it must be secondlife://<region>
				// therefore the hostname will be the region name, and it's a location type
				mType = LOCATION;

				//AW: use current grid for compatibility
				//with viewer 1 slurls.
				mGrid = LLGridManager::getInstance()->getGrid();

				// 'normalize' it so the region name is in fact the head of the path_array
				path_array.insert(0, slurl_uri.hostNameAndPort());
		    }
		}
		else if(   (slurl_uri.scheme() == LLSLURL::SLURL_HTTP_SCHEME)
		 	|| (slurl_uri.scheme() == LLSLURL::SLURL_HTTPS_SCHEME)
		 	|| (slurl_uri.scheme() == LLSLURL::SLURL_X_GRID_LOCATION_INFO_SCHEME)
		 	|| (slurl_uri.scheme() == LLSLURL::HOP_SCHEME	) // <AW: hop:// protocol>
			)
		{
		    // We're dealing with either a Standalone style slurl or slurl.com slurl
		  if ((slurl_uri.hostName() == LLSLURL::SLURL_COM) ||
		      (slurl_uri.hostName() == LLSLURL::WWW_SLURL_COM) || 
		      (slurl_uri.hostName() == LLSLURL::MAPS_SECONDLIFE_COM))
			{
				LL_DEBUGS("SLURL") << "slurl style slurl.com"  << LL_ENDL;
				// slurl.com implies maingrid
				mGrid = MAINGRID;
			}
		    else
			{
				LL_DEBUGS("SLURL") << "slurl style Standalone"  << LL_ENDL;
				// Don't try to match any old http://<host>/ URL as a SLurl.
				// SLE SLurls will have the grid hostname in the URL, so only
				// match http URLs if the hostname matches the grid hostname
				// (or its a slurl.com or maps.secondlife.com URL).
				std::string probe_grid;

				std::string hyper = slurl_uri.hostNameAndPort();
				std::string hyper_trimmed = LLGridManager::getInstance()
								->trimHypergrid(hyper);
				if (hyper != hyper_trimmed)
				{
					mHypergrid = true;
					path_array.insert(0,hyper);
				}

				probe_grid = LLGridManager::getInstance()
						->getGridByProbing(hyper);
				if (probe_grid.empty())
				{
					probe_grid = LLGridManager::getInstance()
						->getGridByProbing(slurl_uri.hostName());
				}

				LL_DEBUGS("SLURL") << "Probing result: "
							<< probe_grid << LL_ENDL;
				LL_DEBUGS("SLURL") << "slurl_uri.hostNameAndPort(): " 
						<< slurl_uri.hostNameAndPort() << LL_ENDL;

				if ((slurl_uri.scheme() == LLSLURL::SLURL_HTTP_SCHEME ||
					 slurl_uri.scheme() == LLSLURL::SLURL_HTTPS_SCHEME) &&
					slurl_uri.hostNameAndPort() != probe_grid)
				{
					LL_DEBUGS("SLURL") << "Don't try to match any old http://<host>/ URL as a SLurl"  << LL_ENDL;

					return;
				}
				// As it's a Standalone grid/open, we will always have a hostname,
				// as Standalone/open style urls are properly formed,
				// unlike the stinky maingrid style
				if (probe_grid.empty())
				{
					mGrid = slurl_uri.hostNameAndPort();
				}
				else
				{
					mGrid = probe_grid;
					mHypergrid = LLGridManager::getInstance()
						->isHyperGrid(probe_grid);
				}
			}

		    if (path_array.size() == 0)
			{
				LL_DEBUGS("SLURL") << "its a broken slurl"  << LL_ENDL;
				// um, we need a path...
				return;
			}
			
			// we need to normalize the urls so
			// the path portion starts with the 'command' that we want to do
			// it can either be region or app.  
		    if ((path_array[0].asString() == LLSLURL::SLURL_REGION_PATH) ||
				(path_array[0].asString() == LLSLURL::SLURL_SECONDLIFE_PATH))
			{
				LL_DEBUGS("SLURL") << "its a location slurl"  << LL_ENDL;
				// strip off 'region' or 'secondlife'
				path_array.erase(0);
				// it's a location
				mType = LOCATION;
			}
			else if (path_array[0].asString() == LLSLURL::SLURL_APP_PATH)
			{
				LL_DEBUGS("SLURL") << "its an app slurl"  << LL_ENDL;
				mType = APP;
				if (mGrid.empty())
					mGrid = default_grid;
				path_array.erase(0);
				// leave app appended.  
			}
// <AW: hop:// protocol>
			else if ( slurl_uri.scheme() == LLSLURL::HOP_SCHEME)
			{
				LL_DEBUGS("SLURL") << "its a location hop"  << LL_ENDL;
				mType = LOCATION;
			}
// </AW: hop:// protocol>
			else
			{
				LL_DEBUGS("SLURL") << "not a valid https/http/x-grid-location-info slurl " 
				<<  slurl << LL_ENDL;
				// not a valid https/http/x-grid-location-info slurl, so it'll likely just be a URL
				return;
			}
		}
		else
		{
		    // invalid scheme, so bail
			LL_DEBUGS("SLURL")<< "invalid scheme" << LL_ENDL;
		    return;
		}
		
		
		if(path_array.size() == 0)
		{
			LL_DEBUGS("SLURL") << "path_array.size() == 0"  << LL_ENDL;
			// we gotta have some stuff after the specifier as to whether it's a region or command
			return;
		}
		
		// now that we know whether it's an app slurl or a location slurl,
		// parse the slurl into the proper data structures.
		if(mType == APP)
		{		
			// grab the app command type and strip it (could be a command to jump somewhere, 
			// or whatever )
			mAppCmd = path_array[0].asString();
			path_array.erase(0);
			
			// Grab the parameters
			mAppPath = path_array;
			// and the query
			mAppQuery = slurl_uri.query();
			mAppQueryMap = slurl_uri.queryMap();
			return;
		}
		else if(mType == LOCATION)
		{
			// at this point, head of the path array should be [ <region>, <x>, <y>, <z> ] where x, y and z 
			// are collectively optional
			// are optional
			mRegion = LLURI::unescape(path_array[0].asString());
			path_array.erase(0);
			
			LL_DEBUGS("SLURL") << "mRegion: "  << mRegion << LL_ENDL;

			// parse the x, y, z
			if(path_array.size() >= 3)
			{
				mPosition = LLVector3(path_array);
// AW: the simulator should care of this
// 				if((F32(mPosition[VX]) < 0.f) || 
// 				(mPosition[VX] > REGION_WIDTH_METERS) ||
// 				(F32(mPosition[VY]) < 0.f) || 
// 				(mPosition[VY] > REGION_WIDTH_METERS) ||
// 				(F32(mPosition[VZ]) < 0.f) || 
// 				(mPosition[VZ] > LLWorld::getInstance()->getRegionMaxHeight()))
// 				{
// 					mType = INVALID;
// 					return;
// 				}
 
			}
			else
			{
				// if x, y and z were not fully passed in, go to the middle of the region.
				// teleport will adjust the actual location to make sure you're on the ground
				// and such
				mPosition = LLVector3(REGION_WIDTH_METERS/2, REGION_WIDTH_METERS/2, 0);
			}
		}
	}
}


// Create a slurl for the middle of the region
LLSLURL::LLSLURL(const std::string& grid, 
				 const std::string& region,
				bool hyper)
: mHypergrid(hyper)
{
	mGrid = grid;
	mRegion = region;
	mType = LOCATION;
	mPosition = LLVector3((F64)REGION_WIDTH_METERS/2, (F64)REGION_WIDTH_METERS/2, 0);
}



// create a slurl given the position.  The position will be modded with the region
// width handling global positions as well
LLSLURL::LLSLURL(const std::string& grid, 
		 const std::string& region, 
		 const LLVector3& position,
				bool hyper)
: mHypergrid(hyper)
{
	mGrid = grid;
	mRegion = region;
	S32 x = llround( (F32)fmod( position[VX], (F32)REGION_WIDTH_METERS ) );
	S32 y = llround( (F32)fmod( position[VY], (F32)REGION_WIDTH_METERS ) );
	S32 z = llround( (F32)position[VZ] );
	mType = LOCATION;
	mPosition = LLVector3(x, y, z);
}


// create a simstring
LLSLURL::LLSLURL(const std::string& region, 
		 const LLVector3& position,
				bool hyper)
: mHypergrid(hyper)
{
  *this = LLSLURL(LLGridManager::getInstance()->getGrid(),
		  region, position);
}

// create a slurl from a global position
LLSLURL::LLSLURL(const std::string& grid, 
		 const std::string& region, 
		 const LLVector3d& global_position,
				bool hyper)
: mHypergrid(hyper)
{
  *this = LLSLURL(grid,
		  region, LLVector3(global_position.mdV[VX],
				    global_position.mdV[VY],
				    global_position.mdV[VZ]));
}

// create a slurl from a global position
LLSLURL::LLSLURL(const std::string& region, 
		 const LLVector3d& global_position,
				bool hyper)
: mHypergrid(hyper)
{
  *this = LLSLURL(LLGridManager::getInstance()->getGrid(),
		  region, global_position);
}

LLSLURL::LLSLURL(const std::string& command, const LLUUID&id, const std::string& verb)
: mHypergrid(false)
{
  mType = APP;
  mAppCmd = command;
  mAppPath = LLSD::emptyArray();
  mAppPath.append(LLSD(id));
  mAppPath.append(LLSD(verb));
}

//<AW: opensim>
LLSLURL::LLSLURL(const LLSD& path_array, bool from_app)
: mHypergrid(false)
{
	if (path_array.isArray() && path_array.size() > 0)
	{
		std::string query;
		LLGridManager::getInstance()->isInOpenSim() ?
			query="hop://" : query="secondlife://";

		for(int i=0; path_array.size()>i; i++)
		{
			query += path_array[i].asString() + "/";
		}

		LLSLURL result(query);
		if(APP == result.getType() && from_app)
		{
			mType = INVALID;
		}
		else
		{
			*this = result;
		}
	}
	else
	{
		mType = INVALID;
	}
}
//</AW: opensim>

std::string LLSLURL::getSLURLString() const
{
	switch(mType)
	{
		case HOME_LOCATION:
			return SIM_LOCATION_HOME;
		case LAST_LOCATION:
			return SIM_LOCATION_LAST;
		case LOCATION:
			{
				// lookup the grid
				S32 x = llround( (F32)mPosition[VX] );
				S32 y = llround( (F32)mPosition[VY] );
				S32 z = llround( (F32)mPosition[VZ] );	
				std::string ret = LLGridManager::getInstance()->getSLURLBase(mGrid);
// 				ret.append(LLURI::escape(mRegion)); 
// 				ret.append(llformat("/%d/%d/%d",x,y,z));
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.0d) | Added: RLVa-1.2.0d
				ret.append(	( ((!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) || (!RlvUtil::isNearbyRegion(mRegion)))
						? (LLURI::escape(mRegion) + llformat("/%d/%d/%d",x,y,z)) : RlvStrings::getString(RLV_STRING_HIDDEN_REGION) ));

// [/RLVa:KB]
 				LL_DEBUGS("SLURL") << "Location: " << ret << LL_ENDL;
				return ret;

			}

		case APP:
		{
			std::ostringstream app_url;
			app_url << LLGridManager::getInstance()->getAppSLURLBase() << "/" << mAppCmd;
			for(LLSD::array_const_iterator i = mAppPath.beginArray();
				i != mAppPath.endArray();
				i++)
			{
				app_url << "/" << i->asString();
			}
			if(mAppQuery.length() > 0)
			{
				app_url << "?" << mAppQuery;
			}
			return app_url.str();
		}	
		default:
			LL_WARNS("AppInit") << "Unexpected SLURL type for SLURL string" << (int)mType << LL_ENDL;			
			return std::string();
	}
}

std::string LLSLURL::getLoginString() const
{
	
	std::stringstream unescaped_start;
	switch(mType)
	{
		case LOCATION:
			unescaped_start << "uri:" 
			<< mRegion << "&" 
			<< llround(mPosition[0]) << "&" 
			<< llround(mPosition[1]) << "&" 
			<< llround(mPosition[2]);
			break;
		case HOME_LOCATION:
			unescaped_start << "home";
			break;
		case LAST_LOCATION:
			unescaped_start << "last";
			break;
		default:
			LL_WARNS("AppInit") << "Unexpected SLURL type for login string" << (int)mType << LL_ENDL;
			break;
	}
	return  xml_escape_string(unescaped_start.str());
}

bool LLSLURL::operator==(const LLSLURL& rhs)
{
	if(rhs.mType != mType) return false;
	switch(mType)
	{
		case LOCATION:
			return ((mGrid == rhs.mGrid) &&
					(mRegion == rhs.mRegion) &&
					(mPosition == rhs.mPosition));
		case APP:
			return getSLURLString() == rhs.getSLURLString();
			
		case HOME_LOCATION:
		case LAST_LOCATION:
			return true;
		default:
			return false;
	}
}

bool LLSLURL::operator !=(const LLSLURL& rhs)
{
	return !(*this == rhs);
}

std::string LLSLURL::getLocationString() const
{
	return llformat("%s/%d/%d/%d",
					mRegion.c_str(),
					(int)llround(mPosition[0]),
					(int)llround(mPosition[1]),
					(int)llround(mPosition[2]));						 
}
std::string LLSLURL::asString() const
{
    std::ostringstream result;
	result << 	" mAppCmd:" << getAppCmd() <<
			" mAppPath:" + getAppPath().asString() <<
			" mAppQueryMap:" + getAppQueryMap().asString() <<
			" mAppQuery: " + getAppQuery() <<
			" mGrid: " + getGrid() <<
			" mRegion: " + getRegion() <<
			" mPosition: " <<
			" mType: " << mType <<
			" mPosition: " << mPosition <<
			" mHypergrid: " << mHypergrid;
    return result.str();
}

// <AW: opensim>
std::string LLSLURL::getTypeHumanReadable(SLURL_TYPE type)
{
	std::string ret;
	switch(type)
	{
		case INVALID:		ret = "INVALID"; break;
		case LOCATION:		ret = "LOCATION"; break;
		case HOME_LOCATION:	ret = "HOME_LOCATION"; break;
		case LAST_LOCATION:	ret = "LAST_LOCATION"; break;
		case APP:		ret = "APP"; break;
		case HELP:		ret = "HELP";
	}

	return ret;
}
// </AW: opensim>
