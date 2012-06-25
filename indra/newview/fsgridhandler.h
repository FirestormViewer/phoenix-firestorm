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

#include "llxmlnode.h"

#include <boost/function.hpp>
#include <boost/signals2.hpp>
                                                                                                       
extern const char* DEFAULT_LOGIN_PAGE;
//Kokua: for llviewernetwork_test
#define KNOWN_GRIDS_SIZE 3
#define AGNI "Second Life"
#define ADITI "Second Life Beta"

#define GRID_VALUE "name"
#define GRID_ID_VALUE "grid_login_id"
// <AW opensim> 
#define GRID_LABEL_VALUE "gridname"
#define GRID_NICK_VALUE "gridnick"
#define GRID_LOGIN_URI_VALUE "loginuri"
#define GRID_HELPER_URI_VALUE "helperuri"
#define GRID_LOGIN_PAGE_VALUE "loginpage"
#define GRID_IS_SYSTEM_GRID_VALUE "system_grid"
#define GRID_IS_FAVORITE_VALUE "favorite"
#define GRID_REGISTER_NEW_ACCOUNT "register"
#define GRID_FORGOT_PASSWORD "password"
// </AW opensim>
#define GRID_IS_SYSTEM_GRID_VALUE "system_grid"
#define GRID_IS_FAVORITE_VALUE "favorite"
#define MAINGRID "util.agni.lindenlab.com"
#define GRID_LOGIN_IDENTIFIER_TYPES "login_identifier_types"
// defines slurl formats associated with various grids.
// we need to continue to support existing forms, as slurls
// are shared between viewers that may not understand newer
// forms.
#define GRID_SLURL_BASE "slurl_base"
#define GRID_APP_SLURL_BASE "app_slurl_base"

class GridInfoRequestResponder;

// <AW opensim>
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
protected:
	std::string mGrid;
};
// </AW opensim>

/**
 * @brief A class to manage the grids available to the viewer
 * including persistance.  This class also maintains the currently
 * selected grid.
 * 
 **/
class LLGridManager : public LLSingleton<LLGridManager>
{
public:
	// <AW opensim>
	typedef enum 
	{
		FETCH,
		FETCHTEMP,
		SYSTEM,
		RETRY,
		LOCAL,
		FINISH,
		TRYLEGACY,
		FAIL,
		REMOVE
	} AddState;
public:
	
	// when the grid manager is instantiated, the default grids are automatically
	// loaded, and the grids favorites list is loaded from the xml file.
	LLGridManager();
	~LLGridManager();
	
// <AW opensim>
	void initGrids();
	void initSystemGrids();
	void initGridList(std::string grid_file, AddState state);
	void initCmdLineGrids();
	void resetGrids();
	// grid list management
 	bool isReadyToLogin(){return mReadyToLogin;}

	// add a grid to the list of grids
// <FS:AW  grid management>
	void addGrid(const std::string& loginuri);
	void removeGrid(const std::string& grid);
	void reFetchGrid() { reFetchGrid(mGrid, true); }
	void reFetchGrid(const std::string& grid, bool set_current = false);
// </FS:AW  grid management>
	// retrieve a map of grid-name <-> label
	// by default only return the user visible grids
	std::map<std::string, std::string> getKnownGrids(bool favorites_only=FALSE);
	
	// this was getGridInfo - renamed to avoid ambiguity with the OpenSim grid_info
	void getGridData(const std::string& grid, LLSD &grid_info);
	void getGridData(LLSD &grid_info) { getGridData(mGrid, grid_info); }

// </AW opensim>	

	// current grid management

	// select a given grid as the current grid.  If the grid
	// is not a known grid, then it's assumed to be a dns name for the
	// grid, and the various URIs will be automatically generated.
	void setGridChoice(const std::string& grid);
	
	
	//get the grid label e.g. "Second Life"
	std::string getGridLabel() { return mGridList[mGrid][GRID_LABEL_VALUE]; }
	//get the grid nick e.g. "agni"
	std::string getGridNick() { return mGridList[mGrid][GRID_NICK_VALUE]; }
	//get the grid e.g. "login.agni.lindenlab.com"
	std::string getGrid() const { return mGrid; }
// <FS:AW  grid management>
	// get the first (and very probably only) login URI of a specified grid
	std::string getLoginURI(const std::string& grid);
// </FS:AW  grid management>
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

	bool hasGrid(const std::string& grid){ return mGridList.has(grid); }
	bool isTemporary(){ return mGridList[mGrid].has("FLAG_TEMPORARY"); }
	bool isTemporary(const std::string& grid){ return mGridList[grid].has("FLAG_TEMPORARY"); }

	// tell if we got this from a Hypergrid SLURL
	bool isHyperGrid(const std::string& grid) { return mGridList[grid].has("HG"); }

	// tell if we know how to acess this grid via Hypergrid
	std::string getGatekeeper(const std::string& grid) { return mGridList[grid].has("gatekeeper") ? mGridList[grid]["gatekeeper"].asString() : std::string(); }
	
	std::string getGridByLabel( const std::string &grid_label, bool case_sensitive = false);

// <AW opensim>
	std::string getGridByProbing( const std::string &probe_for, bool case_sensitive = false);
	std::string getGridByGridNick( const std::string &grid_nick, bool case_sensitive = false);
	std::string getGridByHostName( const std::string &host_name, bool case_sensitive = false);
	std::string getGridByAttribute(const std::string &attribute, const std::string &attribute_value, bool case_sensitive );
// </AW opensim>
	bool isSystemGrid(const std::string& grid) 
	{ 
		return mGridList.has(grid) &&
		      mGridList[grid].has(GRID_IS_SYSTEM_GRID_VALUE) && 
	           mGridList[grid][GRID_IS_SYSTEM_GRID_VALUE].asBoolean(); 
	}
	bool isSystemGrid() { return isSystemGrid(mGrid); }
	// Mark this grid as a favorite that should be persisited on 'save'
	// this is currently used to persist a grid after a successful login
// <AW: opensim>
	// Not used anymore, keeping commented as reminder for merge conflicts
	//void setFavorite() { mGridList[mGrid][GRID_IS_FAVORITE_VALUE] = TRUE; }
// </AW opensim>
// <FS:AW  grid management>
	typedef boost::function<void(bool success)> grid_list_changed_callback_t;
	typedef boost::signals2::signal<void(bool success)> grid_list_changed_signal_t;

	boost::signals2::connection addGridListChangedCallback(grid_list_changed_callback_t cb);
	grid_list_changed_signal_t	mGridListChangedSignal;
// <FS:AW  grid management>
// <AW opensim>
	bool isInSLMain();
	bool isInSLBeta();
	bool isInOpenSim();
	void saveGridList();
// </AW opensim>
	void clearFavorites();

private:
	friend class GridInfoRequestResponder;
	void addGrid(GridEntry* grid_info, AddState state);
	void incResponderCount(){++mResponderCount;}
	void decResponderCount(){--mResponderCount;}
	void gridInfoResponderCB(GridEntry* grid_data);

	void setGridData(const LLSD &grid_info) { mGridList[mGrid]=grid_info; }

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
	LLSD mGridList;
// <AW opensim>
	LLSD mConnectedGrid;
	bool mIsInSLMain;
	bool mIsInSLBeta;
	bool mIsInOpenSim;
	int mResponderCount;
	bool mReadyToLogin;
	bool mCommandLineDone;
// </AW opensim>
};

#endif // FS_GRIDHANDLER_H
