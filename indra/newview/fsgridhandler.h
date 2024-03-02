/** 
 * @file fsgridhandler.h
 * @author James Cook
 * @brief Networking constants and globals for viewer.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Based on Second Life Viewer Source Code llviewernetwork.h
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

#ifndef FS_GRIDHANDLER_H
#define FS_GRIDHANDLER_H

#include "../llxml/llxmlnode.h"
#include "llsingleton.h"

#include <boost/function.hpp>
#include <boost/signals2.hpp>
                                                                                                       
extern const char* DEFAULT_LOGIN_PAGE;
//Kokua: for llviewernetwork_test
const S32 KNOWN_GRIDS_SIZE	= 3;

const std::string GRID_VALUE					= "name";
const std::string GRID_LABEL_VALUE				= "gridname";
const std::string GRID_ID_VALUE					= "grid_login_id";
const std::string GRID_LOGIN_URI_VALUE			= "loginuri";
const std::string GRID_UPDATE_SERVICE_URL		= "update_query_url_base";
const std::string GRID_HELPER_URI_VALUE			= "helperuri";
const std::string GRID_LOGIN_PAGE_VALUE			= "loginpage";
const std::string GRID_IS_SYSTEM_GRID_VALUE		= "system_grid";
const std::string GRID_IS_FAVORITE_VALUE		= "favorite";
const std::string GRID_LOGIN_IDENTIFIER_TYPES	= "login_identifier_types";

const std::string GRID_NICK_VALUE			= "gridnick";
const std::string GRID_REGISTER_NEW_ACCOUNT = "register";
const std::string GRID_FORGOT_PASSWORD		= "password";
const std::string GRID_HELP					= "help";
const std::string GRID_ABOUT				= "about";
const std::string GRID_SEARCH				= "search";
const std::string GRID_WEB_PROFILE_VALUE	= "web_profile_url";
const std::string GRID_SENDGRIDINFO			= "SendGridInfoToViewerOnLogin";
const std::string GRID_DIRECTORY_FEE		= "DirectoryFee";
const std::string GRID_PLATFORM				= "platform";
const std::string GRID_MESSAGE				= "message";

// defines slurl formats associated with various grids.
// we need to continue to support existing forms, as slurls
// are shared between viewers that may not understand newer
// forms.
const std::string GRID_SLURL_BASE		= "slurl_base";
const std::string GRID_APP_SLURL_BASE	= "app_slurl_base";

// Inworldz special
#define INWORLDZ_URI "inworldz.com:8002"

class GridInfoRequestResponder;

struct GridEntry
{
	LLSD grid;
	LLXMLNodePtr info_root;
	bool set_current;
	std::string last_http_error;
};

class LLInvalidGridName
{
public:
	LLInvalidGridName(std::string grid) : mGrid(grid)
	{
	}
	std::string name() { return mGrid; }
protected:
	std::string mGrid;
};


/**
 * @brief A class to manage the grids available to the viewer
 * including persistance.  This class also maintains the currently
 * selected grid.
 * 
 **/
class LLGridManager : public LLSingleton<LLGridManager>
{
	// when the grid manager is instantiated, the default grids are automatically
	// loaded, and the grids favorites list is loaded from the xml file.
	LLSINGLETON(LLGridManager);
	~LLGridManager();

public:
	typedef enum 
	{
		FETCH,
		FETCHTEMP,
		SYSTEM,
		MANUAL,
		RETRY,
		LOCAL,
		FINISH,
		TRYLEGACY,
		FAIL,
		REMOVE
	} AddState;

	void initGrids();
	void initSystemGrids();
	void initGridList(std::string grid_file, AddState state);
	void initCmdLineGrids();
	void resetGrids();
	// grid list management
	bool isReadyToLogin() const {return mReadyToLogin;}

	// add a grid to the list of grids
	void addGrid(const std::string& loginuri);
	void removeGrid(const std::string& grid);
	void reFetchGrid() { reFetchGrid(mGrid, true); }
	void reFetchGrid(const std::string& grid, bool set_current = false);

	/// Retrieve a map of grid-name -> label
	std::map<std::string, std::string> getKnownGrids();
	
	// this was getGridInfo - renamed to avoid ambiguity with the OpenSim grid_info
	void getGridData(const std::string& grid, LLSD &grid_info);
	void getGridData(LLSD &grid_info) { getGridData(mGrid, grid_info); }

	// current grid management

	// select a given grid as the current grid.  If the grid
	// is not a known grid, then it's assumed to be a dns name for the
	// grid, and the various URIs will be automatically generated.
	void setGridChoice(const std::string& grid);

	/// Return the name of a grid, given either its name or its id
	std::string getGrid( const std::string &grid );
	
	/// Get the id (short form selector) for a given grid; example: "agni"
	std::string getGridId(const std::string& grid);

	/// Get the id (short form selector) for the selected grid; example: "agni"
	std::string getGridId() { return getGridId(mGrid); }

	/// Deprecated for compatibility with LL. Use getGridId() instead.
	//std::string getGridNick() { return mGridList[mGrid][GRID_NICK_VALUE]; }

	/// Get the user-friendly long form descriptor for a given grid; example: "Second Life"
	std::string getGridLabel(const std::string& grid);
	
	/// Get the user-friendly long form descriptor for the selected grid; example: "Second Life"
	std::string getGridLabel() { return getGridLabel(mGrid); }
	
	/// Get the uri of  the selected grid; example: "login.agni.lindenlab.com"
	std::string getGrid() const { return mGrid; }

	// get the first (and very probably only) login URI of a specified grid
	std::string getLoginURI(const std::string& grid);

	// get the Login URIs of the current grid
	void getLoginURIs(std::vector<std::string>& uris);

	std::string getHelperURI();
	std::string getLoginPage();
	std::string getGridLoginID() { return mGridList[mGrid][GRID_ID_VALUE]; }	
	std::string getLoginPage(const std::string& grid) { return mGridList[grid][GRID_LOGIN_PAGE_VALUE]; }
	void        getLoginIdentifierTypes(LLSD& idTypes) { idTypes = mGridList[mGrid][GRID_LOGIN_IDENTIFIER_TYPES]; }

	std::string trimHypergrid(const std::string& trim);

	// get location slurl base for the given region within the selected grid
	std::string getSLURLBase(const std::string& grid);
	std::string getSLURLBase() { return getSLURLBase(mGrid); }

	// get app slurl base for the given region within the selected grid
	std::string getAppSLURLBase(const std::string& grid);
	std::string getAppSLURLBase() { return getAppSLURLBase(mGrid); }

	/// Return the url of the resident profile web site for the given grid
	std::string getWebProfileURL(const std::string& grid);

	/// Return the url of the resident profile web site for the selected grid
	std::string getWebProfileURL() { return getWebProfileURL(mGrid); }

	void setWebProfileUrl(const std::string& url) { mGridList[mGrid][GRID_WEB_PROFILE_VALUE] = url; }

	bool hasGrid(const std::string& grid){ return mGridList.has(grid); }
	bool isTemporary(){ return mGridList[mGrid].has("FLAG_TEMPORARY"); }
	bool isTemporary(const std::string& grid){ return mGridList[grid].has("FLAG_TEMPORARY"); }

	// tell if we got this from a Hypergrid SLURL
	bool isHyperGrid(const std::string& grid) { return mGridList[grid].has("HG"); }

	// tell if we know how to acess this grid via Hypergrid
	std::string getGatekeeper() { return getGatekeeper(mGrid); }
	std::string getGatekeeper(const std::string& grid) { return mGridList[grid].has("gatekeeper") ? mGridList[grid]["gatekeeper"].asString() : std::string(); }
	
	std::string getGridByLabel( const std::string &grid_label, bool case_sensitive = false);

	std::string getGridByProbing( const std::string &probe_for, bool case_sensitive = false);
	std::string getGridByGridNick( const std::string &grid_nick, bool case_sensitive = false);
	std::string getGridByHostName( const std::string &host_name, bool case_sensitive = false);
	std::string getGridByAttribute(const std::string &attribute, const std::string &attribute_value, bool case_sensitive );

	bool isSystemGrid(const std::string& grid) 
	{ 
		return mGridList.has(grid) &&
		      mGridList[grid].has(GRID_IS_SYSTEM_GRID_VALUE) && 
	           mGridList[grid][GRID_IS_SYSTEM_GRID_VALUE].asBoolean(); 
	}
	bool isSystemGrid() { return isSystemGrid(mGrid); }

	typedef boost::function<void(bool success)> grid_list_changed_callback_t;
	typedef boost::signals2::signal<void(bool success)> grid_list_changed_signal_t;

	boost::signals2::connection addGridListChangedCallback(grid_list_changed_callback_t cb);
	grid_list_changed_signal_t	mGridListChangedSignal;
	bool isInSecondLife();
	bool isInSLMain();
	bool isInSLBeta();
	bool isInOpenSim();
	bool isInAuroraSim();
	void saveGridList();
	void clearFavorites();
	void addGrid(GridEntry* grid_info, AddState state);
	
	void setClassifiedFee(const S32 classified_fee) { sClassifiedFee = classified_fee; }
	S32 getClassifiedFee() { return sClassifiedFee; }
	void setDirectoryFee(const S32 directory_fee) { sDirectoryFee = directory_fee; }
	S32 getDirectoryFee() { return sDirectoryFee; }

private:
	friend class GridInfoRequestResponder;

	friend void gridDownloadComplete( LLSD const &aData, LLGridManager* mOwner, GridEntry* mData, LLGridManager::AddState mState );
	friend void gridDownloadError( LLSD const &aData, LLGridManager* mOwner, GridEntry* mData, LLGridManager::AddState mState );

	void incResponderCount(){++mResponderCount;}
	void decResponderCount(){--mResponderCount;}
	void gridInfoResponderCB(GridEntry* grid_data);

	void setGridData(const LLSD &grid_info) { mGridList[mGrid]=grid_info; }
	S32 sClassifiedFee;
	S32 sDirectoryFee;

protected:

	void updateIsInProductionGrid();

	// helper function for adding the predefined grids
	void addSystemGrid(const std::string& label, 
					   const std::string& name,
					   const std::string& nick,
					   const std::string& login, 
					   const std::string& helper,
					   const std::string& login_page);
	
	
	std::string mGrid;
	std::string mGridFile;
	std::string mStartupGrid;
	LLSD mGridList;
	LLSD mConnectedGrid;
	int mResponderCount;
	bool mReadyToLogin;
	bool mCommandLineDone;

	enum e_grid_platform
	{
		GP_NOTSET,
		GP_SLMAIN,
		GP_SLBETA,
		GP_OPENSIM,
		GP_AURORA
	} EGridPlatform;
};

#endif // FS_GRIDHANDLER_H
