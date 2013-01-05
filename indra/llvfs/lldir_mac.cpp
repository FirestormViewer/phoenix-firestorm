/** 
 * @file lldir_mac.cpp
 * @brief Implementation of directory utilities for Mac OS X
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#if LL_DARWIN

#include "linden_common.h"

#include "lldir_mac.h"
#include "llerror.h"
#include "llrand.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <boost/filesystem.hpp>
#include "llvfs_objc.h"

// --------------------------------------------------------------------------------

static bool CreateDirectory(const std::string &parent, 
                            const std::string &child,
                            std::string *fullname)
{
    
    boost::filesystem::path p(parent);
    p /= child;
    
    if (fullname)
        *fullname = std::string(p.string());
    
    if (! boost::filesystem::create_directory(p))
    {
        return (boost::filesystem::is_directory(p));
    }
    return true;
}

// --------------------------------------------------------------------------------

LLDir_Mac::LLDir_Mac()
{
	mDirDelimiter = "/";
	mCurrentDirIndex = -1;
	mCurrentDirCount = -1;
	
	//CFBundleRef		mainBundleRef = NULL;
	//CFURLRef		executableURLRef = NULL;
	//CFStringRef		stringRef = NULL;
	//OSStatus		error = noErr;
	//FSRef			fileRef;
	//CFStringRef		secondLifeString = CFSTR("Firestorm");
	
	//mainBundleRef = CFBundleGetMainBundle();
		
	//executableURLRef = CFBundleCopyExecutableURL(mainBundleRef);
	
	//if (executableURLRef != NULL)

    const std::string     secondLifeString = "Firestorm";
    
    std::string *executablepathstr = getSystemExecutableFolder();

    //NOTE:  LLINFOS/LLERRS will not output to log here.  The streams are not initialized.
    
	if (executablepathstr)
	{
		// mExecutablePathAndName
		mExecutablePathAndName = *executablepathstr;
        
        boost::filesystem::path executablepath(*executablepathstr);
        
# ifndef BOOST_SYSTEM_NO_DEPRECATED
#endif
        mExecutableFilename = executablepath.filename().string();
        mExecutableDir = executablepath.parent_path().string();
		
		// mAppRODataDir
        std::string *resourcepath = getSystemResourceFolder();
        mAppRODataDir = *resourcepath;
		
		// *NOTE: When running in a dev tree, use the copy of
		// skins in indra/newview/ rather than in the application bundle.  This
		// mirrors Windows dev environment behavior and allows direct checkin
		// of edited skins/xui files. JC
		
		// MBW -- This keeps the mac application from finding other things.
		// If this is really for skins, it should JUST apply to skins.
        
		U32 build_dir_pos = mExecutableDir.rfind("/build-darwin-");
		if (build_dir_pos != std::string::npos)
		{
			// ...we're in a dev checkout
			mSkinBaseDir = mExecutableDir.substr(0, build_dir_pos)
				+ "/indra/newview/skins";
			llinfos << "Running in dev checkout with mSkinBaseDir "
				<< mSkinBaseDir << llendl;
		}
		else
		{
			// ...normal installation running
			mSkinBaseDir = mAppRODataDir + mDirDelimiter + "skins";
		}
		
		// mOSUserDir
        std::string *appdir = getSystemApplicationSupportFolder();
        std::string rootdir;

        //Create root directory
        if (CreateDirectory(*appdir, secondLifeString, &rootdir))
        {
            
            // Save the full path to the folder
            mOSUserDir = rootdir;
            
            // Create our sub-dirs
            CreateDirectory(rootdir, std::string("data"), NULL);
            CreateDirectory(rootdir, std::string("logs"), NULL);
            CreateDirectory(rootdir, std::string("user_settings"), NULL);
            CreateDirectory(rootdir, std::string("browser_profile"), NULL);
        }
    
		//mOSCacheDir
        std::string *cachedir =  getSystemCacheFolder();

        if (cachedir)
		
		{
#if 0 // <FS:TS> Merge testing
			FSRef	newFileRef;
			
			// Create the directory
			error = CFCreateDirectory(&fileRef, secondLifeString, &newFileRef);
			if (error == noErr)
			{
				// Save the full path to the folder
				FSRefToLLString(&newFileRef, mOSUserDir);
				
				// Create our sub-dirs
				(void) CFCreateDirectory(&newFileRef, CFSTR("data"), NULL);
				//(void) CFCreateDirectory(&newFileRef, CFSTR("cache"), NULL);
				(void) CFCreateDirectory(&newFileRef, CFSTR("logs"), NULL);
				(void) CFCreateDirectory(&newFileRef, CFSTR("user_settings"), NULL);
				(void) CFCreateDirectory(&newFileRef, CFSTR("browser_profile"), NULL);
			}
		}
		
		//mOSCacheDir
		FSRef cacheDirRef;
		error = FSFindFolder(kUserDomain, kCachedDataFolderType, true, &cacheDirRef);
		if (error == noErr)
		{
			FSRefToLLString(&cacheDirRef, mOSCacheDir);
			(void)CFCreateDirectory(&cacheDirRef, CFSTR("Firestorm"),NULL);
#endif
            mOSCacheDir = *cachedir;
            //SPATTERS TODO:  This changes from ~/Library/Cache/Secondlife to ~/Library/Cache/com.app.secondlife/Secondlife.  Last dir level could go away.
            CreateDirectory(mOSCacheDir, secondLifeString, NULL);
		}
		
		// mOSUserAppDir
		mOSUserAppDir = mOSUserDir;
		
		// mTempDir
        //Aura 120920 boost::filesystem::temp_directory_path() not yet implemented on mac. :(
        std::string *tmpdir = getSystemTempFolder();
        if (tmpdir)
        {
            
            CreateDirectory(*tmpdir, secondLifeString, &mTempDir);
            if (tmpdir) delete tmpdir;
        }
		
		mWorkingDir = getCurPath();

		mLLPluginDir = mAppRODataDir + mDirDelimiter + "llplugin";
	}
}

LLDir_Mac::~LLDir_Mac()
{
}

// Implementation


void LLDir_Mac::initAppDirs(const std::string &app_name,
							const std::string& app_read_only_data_dir)
{
	// Allow override so test apps can read newview directory
	if (!app_read_only_data_dir.empty())
	{
		mAppRODataDir = app_read_only_data_dir;
		mSkinBaseDir = mAppRODataDir + mDirDelimiter + "skins";
	}
	mCAFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "CA.pem");
}

// get the next file in the directory
// AO: Used by LGG Selection Beams
BOOL LLDir_Mac::getNextFileInDir(const std::string &dirname, const std::string &mask, std::string &fname)
{
        glob_t g;
        BOOL result = FALSE;
        fname = "";

        if(!(dirname == mCurrentDir))
        {
                // different dir specified, close old search
                mCurrentDirIndex = -1;
                mCurrentDirCount = -1;
                mCurrentDir = dirname;
        }

        std::string tmp_str;
        tmp_str = dirname;
        tmp_str += mask;

        if(glob(tmp_str.c_str(), GLOB_NOSORT, NULL, &g) == 0)
        {
                if(g.gl_pathc > 0)
                {
                        if(g.gl_pathc != mCurrentDirCount)
                        {
                                // Number of matches has changed since the last search, meaning a file has been added or deleted.
                                // Reset the index.
                                mCurrentDirIndex = -1;
                                mCurrentDirCount = g.gl_pathc;
                        }

                        mCurrentDirIndex++;

                        if(mCurrentDirIndex < g.gl_pathc)
                        {
//                              llinfos << "getNextFileInDir: returning number " << mCurrentDirIndex << ", path is " << g.gl_pathv[mCurrentDirIndex] << llendl;

                                // The API wants just the filename, not the full path.
                                //fname = g.gl_pathv[mCurrentDirIndex];

                                char *s = strrchr(g.gl_pathv[mCurrentDirIndex], '/');

                                if(s == NULL)
                                        s = g.gl_pathv[mCurrentDirIndex];
                                else if(s[0] == '/')
                                        s++;

                                fname = s;

                                result = TRUE;
                        }
                }

                globfree(&g);
        }

        return(result);
}

std::string LLDir_Mac::getCurPath()
{
	return boost::filesystem::path( boost::filesystem::current_path() ).string();
}



bool LLDir_Mac::fileExists(const std::string &filename) const
{
    return boost::filesystem::exists(filename);
}


/*virtual*/ std::string LLDir_Mac::getLLPluginLauncher()
{
	return gDirUtilp->getAppRODataDir() + gDirUtilp->getDirDelimiter() +
		"SLPlugin.app/Contents/MacOS/SLPlugin";
}

/*virtual*/ std::string LLDir_Mac::getLLPluginFilename(std::string base_name)
{
	return gDirUtilp->getLLPluginDir() + gDirUtilp->getDirDelimiter() +
		base_name + ".dylib";
}


#endif // LL_DARWIN
