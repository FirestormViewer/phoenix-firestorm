/** 
 * @file llviewernetwork.cpp
 * @author James Cook, Richard Nelson
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llviewernetwork.h"
#include "llviewercontrol.h"
#include "llsdserialize.h"
#include "llsecapi.h"

#include "llhttpclient.h"
// #include "llxmlnode.h"
#include "lltrans.h"
#include "llweb.h"
#include "llbufferstream.h"

// <AW opensim>
class GridInfoRequestResponder : public LLHTTPClient::Responder
{
public:
	GridInfoRequestResponder(LLGridManager* owner, GridEntry* grid_data, LLGridManager::AddState state)
	{
		mOwner = owner;
		mData = grid_data;
		mState = state;

		llwarns << "hello " << this << llendl;
		mOwner->incResponderCount();
	}

	~GridInfoRequestResponder()
	{
		llwarns << "goodbye " << this << llendl;
	}

	// the grid info is no LLSD *sigh* ... override the default LLSD parsing behaviour 
	virtual  void completedRaw(U32 status, const std::string& reason,
									const LLChannelDescriptors& channels,
									const LLIOPipe::buffer_ptr_t& buffer)
	{
		mOwner->decResponderCount();
		LL_DEBUGS("GridManager") << mData->grid[GRID_VALUE] << " status: " << status << " reason: " << reason << llendl;
		if(LLGridManager::TRYLEGACY == mState && 200 == status)
		{
			mOwner->addGrid(mData, LLGridManager::SYSTEM);
		}
		else if (200 == status)// OK
		{
			LL_DEBUGS("GridManager") << "Parsing gridinfo xml file from "
				<< mData->grid[GRID_VALUE] << LL_ENDL;

			LLBufferStream istr(channels, buffer.get());
			if(LLXMLNode::parseStream( istr, mData->info_root, NULL))
			{
				mOwner->gridInfoResponderCB(mData);
			}
			else
			{
				LLSD args;
				args["GRID"] = mData->grid[GRID_VALUE];
				//Could not add [GRID] to the grid list.
				args["REASON"] = "Server provided broken grid info xml. Please";
				//[REASON] contact support of [GRID].
				LLNotificationsUtil::add("CantAddGrid", args);

				llwarns << " Could not parse grid info xml from server."
					<< mData->grid[GRID_VALUE] << " skipping." << llendl;
				mOwner->addGrid(mData, LLGridManager::FAIL);
			}
		}
		else if (304 == status && !LLGridManager::TRYLEGACY == mState)// not modified
		{
			mOwner->addGrid(mData, LLGridManager::FINISH);
		}
		else if (499 == status && LLGridManager::LOCAL == mState) //add localhost even if its not up
		{
			mOwner->addGrid(mData,	LLGridManager::FINISH);
			//since we know now that its not up we cold also start it
		}
		else
		{
			error(status, reason);
		}
	}

	virtual void result(const LLSD& content)
	{

	}

	virtual void error(U32 status, const std::string& reason)
	{
		if (504 == status)// gateway timeout ... well ... retry once >_>
		{
			if (LLGridManager::FETCH == mState)
			{
				mOwner->addGrid(mData,	LLGridManager::RETRY);
			}

		}
		else if (LLGridManager::TRYLEGACY == mState) //we did TRYLEGACY and faild
		{
			LLSD args;
			args["GRID"] = mData->grid[GRID_VALUE];
			//Could not add [GRID] to the grid list.
			std::string reason_dialog = "Server didn't provide grid info: ";
			reason_dialog.append(mData->last_http_error);
			reason_dialog.append("\nPlease check if the loginuri is correct and");
			args["REASON"] = reason_dialog;
			//[REASON] contact support of [GRID].
			LLNotificationsUtil::add("CantAddGrid", args);

			llwarns << "No legacy login page. Giving up for " << mData->grid[GRID_VALUE] << llendl;
			mOwner->addGrid(mData, LLGridManager::FAIL);
		}
		else
		{
			// remember the error we got when trying to get grid info where we expect it
			std::ostringstream last_error;
			last_error << status << " " << reason;
			mData->last_http_error = last_error.str();

			mOwner->addGrid(mData, LLGridManager::TRYLEGACY);
		}

	}

private:
	LLGridManager* mOwner;
	GridEntry* mData;
	LLGridManager::AddState mState;
};
// </AW opensim>


const char* DEFAULT_LOGIN_PAGE = "http://phoenixviewer.com/app/loginV3/";

const char* SYSTEM_GRID_SLURL_BASE = "secondlife://%s/secondlife/";
const char* MAIN_GRID_SLURL_BASE = "http://maps.secondlife.com/secondlife/";
const char* SYSTEM_GRID_APP_SLURL_BASE = "secondlife:///app";

const char* DEFAULT_HOP_BASE = "hop://%s/"; // <AW: hop:// protocol>
const char* DEFAULT_SLURL_BASE = "https://%s/region/";
const char* DEFAULT_APP_SLURL_BASE = "x-grid-location-info://%s/app";

// <AW opensim>
LLGridManager::LLGridManager()
:	mIsInSLMain(false),
	mIsInSLBeta(false),
	mIsInOpenSim(false),
	mReadyToLogin(false),
	mCommandLineDone(false),
	mResponderCount(0),
	mGridEntries(0)
{
	mGrid.clear() ;
	mGridList = LLSD();
}


void LLGridManager::initGrids()
{
	std::string grid_fallback_file  = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,  "grids.fallback.xml");

	std::string grid_user_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,  "grids.user.xml");

	std::string grid_remote_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,  "grids.remote.xml");


	mGridFile = grid_user_file;

	initSystemGrids();
	initGridList(grid_fallback_file, FINISH);
	initGridList(grid_remote_file, FINISH);
	initGridList(grid_user_file, FINISH);

	if(!mCommandLineDone)
	{
		initCmdLineGrids();
	}
}



void LLGridManager::initSystemGrids()
{

	addSystemGrid(LLTrans::getString("LoadingData"), "", "", "", "", DEFAULT_LOGIN_PAGE);
// 	addSystemGrid("None", "", "", "", DEFAULT_LOGIN_PAGE);
//we get this now from the grid list
/*
	addSystemGrid(	AGNI,
			MAINGRID,
			"agni",
			"https://login.agni.lindenlab.com/cgi-bin/login.cgi",
			"https://secondlife.com/helpers/",
			 DEFAULT_LOGIN_PAGE);
	addSystemGrid(	ADITI,
			"util.aditi.lindenlab.com",
			"aditi",
			"https://login.aditi.lindenlab.com/cgi-bin/login.cgi",
			"http://aditi-secondlife.webdev.lindenlab.com/helpers/",
			DEFAULT_LOGIN_PAGE);
// LLGridManager::addSystemGrid - helper for adding a system grid.
void LLGridManager::addSystemGrid(const std::string& label,
					  const std::string& name,
					  const std::string& nick,
					  const std::string& login,
					  const std::string& helper,
					  const std::string& login_page )
*/
}

void LLGridManager::initGridList(std::string grid_file, AddState state)
{
	LLSD other_grids;
	llifstream llsd_xml;
	if (grid_file.empty())
	{
		return;
	}

	if(!gDirUtilp->fileExists(grid_file))
	{
		return;
	}

	llsd_xml.open( grid_file.c_str(), std::ios::in | std::ios::binary );

	// parse through the gridfile
	if( llsd_xml.is_open()) 
	{
		LLSDSerialize::fromXMLDocument( other_grids, llsd_xml );
		if(other_grids.isMap())
		{
			for(LLSD::map_iterator grid_itr = other_grids.beginMap(); 
				grid_itr != other_grids.endMap();
				++grid_itr)
			{
				LLSD::String key_name = grid_itr->first;
				GridEntry* grid_entry = new GridEntry;
				grid_entry->set_current = false;
				grid_entry->grid = grid_itr->second;

				LL_DEBUGS("GridManager") << "reading: " << key_name << LL_ENDL;

				try
				{
					addGrid(grid_entry, state);
					LL_DEBUGS("GridManager") << "Added grid: " << key_name << LL_ENDL;
				}
				catch (...)
				{
				}

			}
			llsd_xml.close();
		}
	}

}

void LLGridManager::initCmdLineGrids()
{
	mCommandLineDone = true;

	// load a grid from the command line.
	// if the actual grid name is specified from the command line,
	// set it as the 'selected' grid.
	LLSD cmd_line_login_uri = gSavedSettings.getLLSD("CmdLineLoginURI");
	if (cmd_line_login_uri.isString() && !cmd_line_login_uri.asString().empty())
	{	
		mGrid = cmd_line_login_uri.asString();
		gSavedSettings.setLLSD("CmdLineLoginURI", LLSD::emptyArray());	//in case setGridChoice tries to addGrid 
									//and  addGrid recurses here.
		setGridChoice(mGrid);
		return;
	}

	std::string cmd_line_grid = gSavedSettings.getString("CmdLineGridChoice");
	if(!cmd_line_grid.empty())
	{

		// try to find the grid assuming the command line parameter is
		// the case-insensitive 'label' of the grid.  ie 'Agni'

		mGrid = getGridByGridNick(cmd_line_grid);
		gSavedSettings.setString("CmdLineGridChoice",std::string());

		if(mGrid.empty())
		{
			mGrid = getGridByLabel(cmd_line_grid);

		}
		if(mGrid.empty())
		{
			// if we couldn't find it, assume the
			// requested grid is the actual grid 'name' or index,
			// which would be the dns name of the grid (for non
			// linden hosted grids)
			// If the grid isn't there, that's ok, as it will be
			// automatically added later.
			mGrid = cmd_line_grid;
		}
		
	}

	else
	{
		// if a grid was not passed in via the command line, grab it from the CurrentGrid setting.
		// if there's no current grid, that's ok as it'll be either set by the value passed
		// in via the login uri if that's specified, or will default to maingrid
		mGrid = gSavedSettings.getString("CurrentGrid");
	}
	
	if(mGrid.empty())
	{
		// no grid was specified so default to maingrid
		LL_DEBUGS("GridManager") << "Setting grid to MAINGRID as no grid has been specified " << LL_ENDL;
		mGrid = MAINGRID;
	}
	
	// generate a 'grid list' entry for any command line parameter overrides
	// or setting overides that we'll add to the grid list or override
	// any grid list entries with.

	
	if(mGridList.has(mGrid))
	{
// 		grid_entry->grid = mGridList[mGrid];
		LL_DEBUGS("GridManager") << "Setting commandline grid " << mGrid << LL_ENDL;
		setGridChoice(mGrid);
	}
	else
	{
		LL_DEBUGS("GridManager") << "Trying to fetch commandline grid " << mGrid << LL_ENDL;
		GridEntry* grid_entry = new GridEntry;
		grid_entry->set_current = true;
		grid_entry->grid = LLSD::emptyMap();	
		grid_entry->grid[GRID_VALUE] = mGrid;

		// add the grid with the additional values, or update the
		// existing grid if it exists with the given values
		addGrid(grid_entry, FETCH);
	}
}
// </AW opensim>


LLGridManager::~LLGridManager()
{

}

// <AW opensim>
void LLGridManager::getGridData(const std::string &grid, LLSD& grid_info)
{
	
	grid_info = mGridList[grid]; 


	
	// override any grid data with the command line info.

	// Rather not. Overriding means what you get is different from what you see
	// - the actual loginuri is foo.com  but the combo shows bar.org -
	// that is at least annoying and a security risk.
	/*	
	LLSD cmd_line_login_uri = gSavedSettings.getLLSD("CmdLineLoginURI");
	if (cmd_line_login_uri.isString())
	{
		grid_info[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
		grid_info[GRID_LOGIN_URI_VALUE].append(cmd_line_login_uri);
	}
	
	// override the helper uri if it was passed in
	std::string cmd_line_helper_uri = gSavedSettings.getString("CmdLineHelperURI");
	if(!cmd_line_helper_uri.empty())
	{
		grid_info[GRID_HELPER_URI_VALUE] = cmd_line_helper_uri;	
	}
	
	// override the login page if it was passed in
	std::string cmd_line_login_page = gSavedSettings.getString("LoginPage");
	if(!cmd_line_login_page.empty())
	{
		grid_info[GRID_LOGIN_PAGE_VALUE] = cmd_line_login_page;
	}
	*/	
}
// <AW opensim>

// <AW opensim>
void LLGridManager::gridInfoResponderCB(GridEntry* grid_entry)
{
	for (LLXMLNode* node = grid_entry->info_root->getFirstChild(); node != NULL; node = node->getNextSibling())
	{
		std::string check;
		check = "login";
		if (node->hasName(check))
		{
			grid_entry->grid[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
			grid_entry->grid[GRID_LOGIN_URI_VALUE].append(node->getTextContents());
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[GRID_LOGIN_URI_VALUE] << LL_ENDL;
			continue;
		}
		check = "gridname";
		if (node->hasName(check))
		{
			grid_entry->grid[GRID_LABEL_VALUE] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[GRID_LABEL_VALUE] << LL_ENDL;
			continue;
		}
		check = "gridnick";
		if (node->hasName(check))
		{
			grid_entry->grid[check] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[check] << LL_ENDL;
			continue;
		}
		check = "welcome";
		if (node->hasName(check))
		{
			grid_entry->grid[GRID_LOGIN_PAGE_VALUE] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[GRID_LOGIN_PAGE_VALUE] << LL_ENDL;
			continue;
		}
		check = GRID_REGISTER_NEW_ACCOUNT;
		if (node->hasName(check))
		{
			grid_entry->grid[check] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[check] << LL_ENDL;
			continue;
		}
		check = GRID_FORGOT_PASSWORD;
		if (node->hasName(check))
		{
			grid_entry->grid[check] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[check] << LL_ENDL;
			continue;
		}
		check = "help";
		if (node->hasName(check))
		{
			grid_entry->grid[check] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[check] << LL_ENDL;
			continue;
		}
		check = "about";
		if (node->hasName(check))
		{
			grid_entry->grid[check] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[check] << LL_ENDL;
			continue;
		}
		check = "helperuri";
		if (node->hasName(check))
		{
			grid_entry->grid[GRID_HELPER_URI_VALUE] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[GRID_HELPER_URI_VALUE] << LL_ENDL;
			//don't continue, also check the next
		}
		check = "economy";
		if (node->hasName(check))
		{	//sic! economy and helperuri is 2 names for the same
			grid_entry->grid[GRID_HELPER_URI_VALUE] = node->getTextContents();
			LL_DEBUGS("GridManager") << "[\""<<check<<"\"]: " << grid_entry->grid[GRID_HELPER_URI_VALUE] << LL_ENDL;
		}
	}


	std::string grid = grid_entry->grid[GRID_VALUE].asString();
	std::string slurl_base(llformat(DEFAULT_HOP_BASE, grid.c_str())); // <AW: hop:// protocol>
	grid_entry->grid[GRID_SLURL_BASE]= slurl_base;

	LLDate now = LLDate::now();
	grid_entry->grid["LastModified"] = now;

	addGrid(grid_entry, FINISH);
}

//
// LLGridManager::addGrid - add a grid to the grid list, populating the needed values
// if they're not populated yet.
//
void LLGridManager::addGrid(GridEntry* grid_entry,  AddState state)
{
	if(!grid_entry)
	{
		llwarns << "addGrid called with NULL grid_entry. Please send a bug report." << llendl;
		state = FAIL;
	}
	if(!grid_entry->grid.has(GRID_VALUE))
	{
		state = FAIL;
	}
	else if(grid_entry->grid[GRID_VALUE].asString().empty())
	{
		state = FAIL;
	}
	else if(!grid_entry->grid.isMap())
	{
		state = FAIL;
	}

	if ((FETCH == state) ||(FETCHTEMP == state) || (SYSTEM == state))
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);
		// grid should be in the form of a dns address
		// but also support localhost:9000 or localhost:9000/login
		if ( !grid.empty() && grid.find_first_not_of("abcdefghijklmnopqrstuvwxyz1234567890-_.:/@% ") != std::string::npos)
		{
			printf("grid name: %s", grid.c_str());
			if (grid_entry)
			{
				state = FAIL;
				delete grid_entry;
				grid_entry = NULL;
			}
			throw LLInvalidGridName(grid);
		}

		size_t find_last_slash = grid.find_last_of("/");
		if ( (grid.length()-1) == find_last_slash )
		{
			grid.erase(find_last_slash);
			grid_entry->grid[GRID_VALUE]  = grid;
		}

		if (FETCHTEMP == state)
		{
			grid_entry->grid["FLAG_TEMPORARY"] = "TRUE";
			state = FETCH;
		}

	}

	if ((FETCH == state) || (RETRY == state))
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);


		std::string match = "://";
		size_t find_scheme = grid.find(match);
		if ( std::string::npos != find_scheme)
		{
			// We only support http so just remove anything the user might have chosen
			grid.erase(0,find_scheme+match.length());
			grid_entry->grid[GRID_VALUE]  = grid;
		}

		std::string uri = "http://"+grid;

		if (std::string::npos != uri.find("lindenlab.com"))
		{
			state = SYSTEM;
		}
		else
		{
			if ( std::string::npos == uri.find(".")
				|| std::string::npos != uri.find("127.0.0.1")
				|| std::string::npos != uri.find("localhost") )
			{
				state = LOCAL;
			}
			grid_entry->grid[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
			grid_entry->grid[GRID_LOGIN_URI_VALUE].append(uri);

			size_t find_last_slash = uri.find_last_of("/");
			if ( (uri.length()-1) != find_last_slash )
			{
				uri.append("/");
			}
			uri.append("get_grid_info");
	
			LL_DEBUGS("GridManager") << "get_grid_info uri: " << uri << LL_ENDL;
	
			time_t last_modified = 0;
			if(grid_entry->grid.has("LastModified"))
			{
				LLDate saved_value = grid_entry->grid["LastModified"];
				last_modified = (time_t)saved_value.secondsSinceEpoch();
			}
	
			LLHTTPClient::getIfModified(uri, new GridInfoRequestResponder(this, grid_entry, state), last_modified);
			return;
		}
	}

	if(TRYLEGACY == state)
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);
		std::string uri = "https://" + grid + "/cgi-bin/login.cgi";
		llwarns << "No gridinfo found. Trying if legacy login page exists: " << uri << llendl;
		LLHTTPClient::get(uri, new GridInfoRequestResponder(this, grid_entry, state));
		return;
	}

	if(FAIL != state)
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);

		// populate the other values if they don't exist
		if (!grid_entry->grid.has(GRID_LABEL_VALUE)) 
		{
			grid_entry->grid[GRID_LABEL_VALUE] = grid;
			llwarns << "No \"gridname\" found in grid info, setting to " << grid_entry->grid[GRID_LABEL_VALUE].asString() << llendl;
		}


		if (!grid_entry->grid.has(GRID_NICK_VALUE))
		{
			grid_entry->grid[GRID_NICK_VALUE] = grid;
			llwarns << "No \"gridnick\" found in grid info, setting to " << grid_entry->grid[GRID_NICK_VALUE].asString() << llendl;
		}
	}

	if (SYSTEM == state)
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);

		// if the grid data doesn't include any of the URIs, then 
		// generate them from the grid

		if (!grid_entry->grid.has(GRID_LOGIN_URI_VALUE))
		{
			grid_entry->grid[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
			grid_entry->grid[GRID_LOGIN_URI_VALUE].append(std::string("https://") + grid + "/cgi-bin/login.cgi");
			llwarns << "Adding Legacy Login Service at:" << grid_entry->grid[GRID_LOGIN_URI_VALUE].asString() << llendl;
		}

		// Populate to the default values
		if (!grid_entry->grid.has(GRID_LOGIN_PAGE_VALUE)) 
		{
			grid_entry->grid[GRID_LOGIN_PAGE_VALUE] = std::string("http://") + grid + "/app/login/";
			llwarns << "Adding Legacy Login Screen at:" << grid_entry->grid[GRID_LOGIN_PAGE_VALUE].asString() << llendl;
		}		
		if (!grid_entry->grid.has(GRID_HELPER_URI_VALUE)) 
		{
			llwarns << "Adding Legacy Economy at:" << grid_entry->grid[GRID_HELPER_URI_VALUE].asString() << llendl;
			grid_entry->grid[GRID_HELPER_URI_VALUE] = std::string("https://") + grid + "/helpers/";
		}
	}

	if(FAIL != state)
	{
		std::string grid = utf8str_tolower(grid_entry->grid[GRID_VALUE]);

		if (!grid_entry->grid.has(GRID_LOGIN_IDENTIFIER_TYPES))
		{
			// non system grids and grids that haven't already been configured with values
			// get both types of credentials.
			grid_entry->grid[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
			grid_entry->grid[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);
			grid_entry->grid[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_ACCOUNT);
		}
	
		bool is_current = grid_entry->set_current;
		grid_entry->set_current = false;
	
		if(!grid.empty())//
		{
			if (!mGridList.has(grid)) //new grid
			{
				//finally add the grid \o/
				mGridList[grid] = grid_entry->grid;
				++mGridEntries;

				LL_DEBUGS("GridManager") << "Adding new entry: " << grid << LL_ENDL;
			}
			else
			{
				LLSD existing_grid = mGridList[grid];
				if (!existing_grid.has("LastModified"))
				{
					//lack of "LastModified" means existing_grid is from fallback list,
					// assume its anyway older and override with the new entry

					mGridList[grid] = grid_entry->grid;
					//number of mGridEntries doesn't change
					LL_DEBUGS("GridManager") << "Using custom entry: " << grid << LL_ENDL;
				}
				else if (grid_entry->grid.has("LastModified"))
				{
// (time_t)saved_value.secondsSinceEpoch();
					LLDate testing_newer = grid_entry->grid["LastModified"];
					LLDate existing = existing_grid["LastModified"];

					LL_DEBUGS("GridManager") << "testing_newer " << testing_newer
								<< " existing " << existing << LL_ENDL;

					if(testing_newer.secondsSinceEpoch() > existing.secondsSinceEpoch())
					{
						//existing_grid is older, override.
	
						mGridList[grid] = grid_entry->grid;
						//number of mGridEntries doesn't change
						LL_DEBUGS("GridManager") << "Updating entry: " << grid << LL_ENDL;
					}
				}
				else
				{
					LL_DEBUGS("GridManager") << "Same or newer entry already present: " << grid << LL_ENDL;
				}

			}
	
			if(is_current)
			{
				mGrid = grid;
	
				LL_DEBUGS("GridManager") << "Selected grid is " << mGrid << LL_ENDL;		
				setGridChoice(mGrid);
			}
	
		}
	}

// This is only of use if we want to fetch infos of entire gridlists at startup
/*
	if(grid_entry && FINISH == state || FAIL == state)
	{

		if((FINISH == state && !mCommandLineDone && 0 == mResponderCount)
			||(FAIL == state && grid_entry->set_current) )
		{
			LL_DEBUGS("GridManager") << "init CmdLineGrids"  << LL_ENDL;

			initCmdLineGrids();
		}
	}
*/

	if (grid_entry)
	{
		delete grid_entry;
		grid_entry = NULL;
	}
}

//
// LLGridManager::addSystemGrid - helper for adding a system grid.
void LLGridManager::addSystemGrid(const std::string& label,
					  const std::string& name,
					  const std::string& nick,
					  const std::string& login,
					  const std::string& helper,
					  const std::string& login_page )
{
	GridEntry* grid_entry = new GridEntry;
	grid_entry->set_current = false;
	grid_entry->grid = LLSD::emptyMap();
	grid_entry->grid[GRID_VALUE] = name;
	grid_entry->grid[GRID_LABEL_VALUE] = label;
	grid_entry->grid[GRID_NICK_VALUE] = nick;
	grid_entry->grid[GRID_HELPER_URI_VALUE] = helper;
	grid_entry->grid[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
	grid_entry->grid[GRID_LOGIN_URI_VALUE].append(login);
	grid_entry->grid[GRID_LOGIN_PAGE_VALUE] = login_page;
	grid_entry->grid[GRID_IS_SYSTEM_GRID_VALUE] = TRUE;
	grid_entry->grid[GRID_LOGIN_IDENTIFIER_TYPES] = LLSD::emptyArray();
	grid_entry->grid[GRID_LOGIN_IDENTIFIER_TYPES].append(CRED_IDENTIFIER_TYPE_AGENT);
	
	grid_entry->grid[GRID_APP_SLURL_BASE] = SYSTEM_GRID_APP_SLURL_BASE;

	
	// only add the system grids beyond agni to the visible list
	// if we're building a debug version.
	if (name == std::string(MAINGRID))
	{
		grid_entry->grid[GRID_SLURL_BASE] = MAIN_GRID_SLURL_BASE;
		grid_entry->grid[GRID_IS_FAVORITE_VALUE] = TRUE;
	}
	else
	{
		grid_entry->grid[GRID_SLURL_BASE] = llformat(SYSTEM_GRID_SLURL_BASE, label.c_str());
	}
	addGrid(grid_entry, SYSTEM);
}



// return a list of grid name -> grid label mappings for UI purposes
std::map<std::string, std::string> LLGridManager::getKnownGrids(bool favorite_only)
{
	std::map<std::string, std::string> result;
	for(LLSD::map_iterator grid_iter = mGridList.beginMap();
		grid_iter != mGridList.endMap();
		grid_iter++) 
	{
		if(!favorite_only || grid_iter->second.has(GRID_IS_FAVORITE_VALUE))
		{
			result[grid_iter->first] = grid_iter->second[GRID_LABEL_VALUE].asString();
		}
	}

	return result;
}


void LLGridManager::setGridChoice(const std::string& grid)
{
	if(grid.empty()) return;

	// Set the grid choice based on a string.
	// The string can be:
	// - a grid label from the gGridInfo table 
	// - a hostname
	// - an ip address

	// loop through.  We could do just a hash lookup but we also want to match
	// on label

	mReadyToLogin = false;
	std::string grid_name = grid;
	if(mGridList.has(grid_name))
	{
		LL_DEBUGS("GridManager")<< "got grid from grid list: " << grid << LL_ENDL;
	}
	else
	{
		// case insensitive
		grid_name = getGridByLabel(grid);
		if (!grid_name.empty())
			LL_DEBUGS("GridManager")<< "got grid by label: " << grid << LL_ENDL;
	}



	if(grid_name.empty())
	{
		// case insensitive
		grid_name = getGridByGridNick(grid);
		if (!grid_name.empty())
			LL_DEBUGS("GridManager")<< "got grid by gridnick: " << grid << LL_ENDL;
	}
	
	if(grid_name.empty())
	{
		LL_DEBUGS("GridManager")<< "trying to fetch grid: " << grid << LL_ENDL;
		// the grid was not in the list of grids.
		GridEntry* grid_entry = new GridEntry;
		grid_entry->grid = LLSD::emptyMap();
		grid_entry->grid[GRID_VALUE] = grid;
		grid_entry->set_current = true;
		addGrid(grid_entry, FETCH);
	}
	else
	{
		LL_DEBUGS("GridManager")<< "setting grid choice: " << grid << LL_ENDL;
		mGrid = grid;
		gSavedSettings.setString("CurrentGrid", grid); 
		updateIsInProductionGrid();
		mReadyToLogin = true;
	}
}

std::string LLGridManager::getGridByProbing( const std::string &probe_for, bool case_sensitive)
{
	std::string ret;
	ret = getGridByHostName(probe_for, case_sensitive);
	if (ret.empty())
	{
		getGridByGridNick(probe_for, case_sensitive);
	}
	if (ret.empty())
	{
		getGridByLabel(probe_for, case_sensitive);
	}

	return ret;
}

std::string LLGridManager::getGridByLabel( const std::string &grid_label, bool case_sensitive)
{
	return grid_label.empty() ? std::string() : getGridByAttribute(GRID_LABEL_VALUE, grid_label, case_sensitive);
}

std::string LLGridManager::getGridByGridNick( const std::string &grid_nick, bool case_sensitive)
{
	return grid_nick.empty() ? std::string() : getGridByAttribute(GRID_NICK_VALUE, grid_nick, case_sensitive);
}
std::string LLGridManager::getGridByHostName( const std::string &host_name, bool case_sensitive)
{
	return host_name.empty() ? std::string() : getGridByAttribute(GRID_VALUE, host_name, case_sensitive);
}

std::string LLGridManager::getGridByAttribute( const std::string &attribute, const std::string &attribute_value, bool case_sensitive)
{
	if(attribute.empty()||attribute_value.empty())
		return std::string();

	for(LLSD::map_iterator grid_iter = mGridList.beginMap();
		grid_iter != mGridList.endMap();
		grid_iter++) 
	{
		if (grid_iter->second.has(attribute))
		{
			if (0 == (case_sensitive?LLStringUtil::compareStrings(attribute_value, grid_iter->second[attribute].asString()):
				LLStringUtil::compareInsensitive(attribute_value, grid_iter->second[attribute].asString())))
			{
				return grid_iter->first;
			}
		}
	}
	return std::string();
}
// </AW opensim>


// <AW opensim>
void LLGridManager::getLoginURIs(std::vector<std::string>& uris)
{
	uris.clear();
	LLSD cmd_line_login_uri = gSavedSettings.getLLSD("CmdLineLoginURI");
//	if (cmd_line_login_uri.isString())
// 	{
// 		uris.push_back(cmd_line_login_uri);
// 		return;
// 	}
	for (LLSD::array_iterator llsd_uri = mGridList[mGrid][GRID_LOGIN_URI_VALUE].beginArray();
		 llsd_uri != mGridList[mGrid][GRID_LOGIN_URI_VALUE].endArray();
		 llsd_uri++)
	{
		std::string current = llsd_uri->asString();
		if(!current.empty())
		{
			uris.push_back(current);
		}
	}
}
// </AW opensim>

std::string LLGridManager::getHelperURI() 
{
	std::string cmd_line_helper_uri = gSavedSettings.getString("CmdLineHelperURI");
	if(!cmd_line_helper_uri.empty())
	{
		return cmd_line_helper_uri;	
	}
	return mGridList[mGrid][GRID_HELPER_URI_VALUE];
}

std::string LLGridManager::getLoginPage() 
{
	// override the login page if it was passed in
	std::string cmd_line_login_page = gSavedSettings.getString("LoginPage");
	if(!cmd_line_login_page.empty())
	{
		return cmd_line_login_page;
	}	
	
	return mGridList[mGrid][GRID_LOGIN_PAGE_VALUE];
}

// <AW opensim>
void LLGridManager::updateIsInProductionGrid()
{
	mIsInSLMain = false;
	mIsInSLBeta = false;
	mIsInOpenSim = false;

	// *NOTE:Mani This used to compare GRID_INFO_AGNI to gGridChoice,
	// but it seems that loginURI trumps that.
	std::vector<std::string> uris;
	getLoginURIs(uris);
	if(uris.empty())
	{
		LL_DEBUGS("GridManager") << "uri is empty, no grid type set." << LL_ENDL;
		return;
	}

	LLStringUtil::toLower(uris[0]);

	// LL looks if "agni" is contained in the string for SL main grid detection.
	// cool for http://agni.nastyfraud.com/steal.php#allyourmoney
	if( uris[0].find("https://login.agni.lindenlab.com") ==  0 )
	{
		LL_DEBUGS("GridManager")<< "uri: "<< uris[0]  << "set IsInSLMain" << LL_ENDL;
		mIsInSLMain = true;
		return;
	}
	else if( uris[0].find("lindenlab.com") !=  std::string::npos )//here is no real money
	{
		LL_DEBUGS("GridManager")<< "uri: "<< uris[0]  << "set IsInSLBeta" << LL_ENDL;
		mIsInSLBeta = true;
		return;
	}

	LL_DEBUGS("GridManager")<< "uri: "<< uris[0]  << "set IsInOpenSim" << LL_ENDL;
	mIsInOpenSim = true;
}
// </AW opensim>

bool LLGridManager::isInSLMain()
{
	return mIsInSLMain;
}

bool LLGridManager::isInSLBeta()
{
	return mIsInSLBeta;
}

bool LLGridManager::isInOpenSim()
{
	return mIsInOpenSim;
}

void LLGridManager::saveGridList()
{
	// filter out just those which are not hardcoded anyway
	LLSD output_grid_list = LLSD::emptyMap();
	for(LLSD::map_iterator grid_iter = mGridList.beginMap();
		grid_iter != mGridList.endMap();
		grid_iter++)
	{
		if(!grid_iter->second.has("FLAG_TEMPORARY"))
		{
 			output_grid_list[grid_iter->first] = grid_iter->second;
		}
	}
	llofstream llsd_xml;
	llsd_xml.open( mGridFile.c_str(), std::ios::out | std::ios::binary);	
	LLSDSerialize::toPrettyXML(output_grid_list, llsd_xml);
	llsd_xml.close();
}

// get location slurl base for the given region 
// within the selected grid (LL comment was misleading)
std::string LLGridManager::getSLURLBase(const std::string& grid)
{
	std::string grid_base;
	std::string ret;
	if(mGridList.has(grid) && mGridList[grid].has(GRID_SLURL_BASE))
	{
		ret = mGridList[grid][GRID_SLURL_BASE].asString();
		LL_DEBUGS("GridManager") << "GRID_SLURL_BASE: " << ret << LL_ENDL;// <AW opensim>
	}
//<AW opensim>
	else
	{
		LL_DEBUGS("GridManager") << "Trying to fetch info for:" << grid << LL_ENDL;
		GridEntry* grid_entry = new GridEntry;
		grid_entry->set_current = false;
		grid_entry->grid = LLSD::emptyMap();	
		grid_entry->grid[GRID_VALUE] = grid;

		// add the grid with the additional values, or update the
		// existing grid if it exists with the given values
		addGrid(grid_entry, FETCHTEMP);
// </AW opensim>
		ret = llformat(DEFAULT_HOP_BASE, grid.c_str());// <AW: hop:// protocol>
		LL_DEBUGS("GridManager") << "DEFAULT_HOP_BASE: " << ret  << LL_ENDL;// <AW opensim>
	}

	return  ret;
}

// get app slurl base for the given region 
// within the selected grid (LL comment was misleading)
std::string LLGridManager::getAppSLURLBase(const std::string& grid)
{
	std::string grid_base;
	std::string ret;

	if(mGridList.has(grid) && mGridList[grid].has(GRID_APP_SLURL_BASE))
	{
		ret = mGridList[grid][GRID_APP_SLURL_BASE].asString();
	}
	else
	{
		std::string app_base;
		if(mGridList.has(grid) && mGridList[grid].has(GRID_SLURL_BASE))
		{
			std::string grid_slurl_base = mGridList[grid][GRID_SLURL_BASE].asString();
			if( 0 == grid_slurl_base.find("hop://"))
			{
				app_base = DEFAULT_HOP_BASE;
				app_base.append("app");
			}
			else 
			{
				app_base = DEFAULT_APP_SLURL_BASE;
			}
		}
		else 
		{
			app_base = DEFAULT_APP_SLURL_BASE;
		}

		ret =  llformat(app_base.c_str(), grid.c_str());
	}

	LL_DEBUGS("GridManager") << "App slurl base: " << ret << LL_ENDL;
	return ret;
}
