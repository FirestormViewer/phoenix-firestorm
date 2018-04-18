/** 
 * @file llviewermenufile.h
 * @brief "File" menu in the main menu bar.
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

#ifndef LLVIEWERMENUFILE_H
#define LLVIEWERMENUFILE_H

#include "llfoldertype.h"
#include "llassetstorage.h"
#include "llinventorytype.h"
#include "llfilepicker.h"
#include "llthread.h"
#include <queue>

#include "llviewerassetupload.h"
#include <boost/signals2.hpp> // <FS:CR Threaded Filepickers />

class LLTransactionID;


void init_menu_file();


LLUUID upload_new_resource(
    const std::string& src_filename,
    std::string name,
    std::string desc,
    S32 compression_info,
    LLFolderType::EType destination_folder_type,
    LLInventoryType::EType inv_type,
    U32 next_owner_perms,
    U32 group_perms,
    U32 everyone_perms,
    const std::string& display_name,
    LLAssetStorage::LLStoreAssetCallback callback,
    S32 expected_upload_cost,
    void *userdata);

void upload_new_resource(
    LLResourceUploadInfo::ptr_t &uploadInfo,
    LLAssetStorage::LLStoreAssetCallback callback = NULL,
    void *userdata = NULL);


void assign_defaults_and_show_upload_message(
	LLAssetType::EType asset_type,
	LLInventoryType::EType& inventory_type,
	std::string& name,
	const std::string& display_name,
	std::string& description);

class LLFilePickerThread : public LLThread
{ //multi-threaded file picker (runs system specific file picker in background and calls "notify" from main thread)
public:

	static std::queue<LLFilePickerThread*> sDeadQ;
	static LLMutex* sMutex;

	static void initClass();
	static void cleanupClass();
	static void clearDead();

	std::vector<std::string> mResponses;
	std::string mProposedName;

	LLFilePicker::ELoadFilter mLoadFilter;
	LLFilePicker::ESaveFilter mSaveFilter;
	bool mIsSaveDialog;
	bool mIsGetMultiple;

	LLFilePickerThread(LLFilePicker::ELoadFilter filter, bool get_multiple = false)
		: LLThread("file picker"), mLoadFilter(filter), mIsSaveDialog(false), mIsGetMultiple(get_multiple)
	{
	}

	LLFilePickerThread(LLFilePicker::ESaveFilter filter, const std::string &proposed_name)
		: LLThread("file picker"), mSaveFilter(filter), mIsSaveDialog(true), mProposedName(proposed_name)
	{
	}

	void getFile();

	virtual void run();

	virtual void notify(const std::vector<std::string>& filenames) = 0;
};

// <FS:CR Threaded Filepickers>
class LLGenericLoadFilePicker : public LLFilePickerThread
{
public:
	LLGenericLoadFilePicker(LLFilePicker::ELoadFilter filter, boost::function<void (const std::string&)> notify_slot)
		: LLFilePickerThread(filter)
	{
		mSignal.connect(notify_slot);
	}

	virtual void notify(const std::vector<std::string>& filenames)
	{
		if (!filenames.empty())
		{
			mSignal(filenames[0]);
		}
		else
		{
			mSignal(std::string());
		}
	}

	static void open(LLFilePicker::ELoadFilter filter, boost::function<void (const std::string&)> notify_slot)
	{
		(new LLGenericLoadFilePicker(filter, notify_slot))->getFile();
	}

protected:
	boost::signals2::signal<void (const std::string&)> mSignal;
};

class LLGenericSaveFilePicker : public LLFilePickerThread
{
public:
	LLGenericSaveFilePicker(LLFilePicker::ESaveFilter filter, const std::string& default_name, boost::function<void (const std::string&)> notify_slot)
		: LLFilePickerThread(filter, default_name)
	{
		mSignal.connect(notify_slot);
	}

	virtual void notify(const std::vector<std::string>& filenames)
	{
		if (!filenames.empty())
		{
			mSignal(filenames[0]);
		}
		else
		{
			mSignal(std::string());
		}
	}

	static void open(LLFilePicker::ESaveFilter filter, const std::string& default_name, boost::function<void (const std::string&)> notify_slot)
	{
		(new LLGenericSaveFilePicker(filter, default_name, notify_slot))->getFile();
	}

protected:
	boost::signals2::signal<void (const std::string&)> mSignal;
};


class LLGenericLoadMultipleFilePicker : public LLFilePickerThread
{
public:
	LLGenericLoadMultipleFilePicker(LLFilePicker::ELoadFilter filter, boost::function<void (std::list<std::string> filenames)> notify_slot)
		: LLFilePickerThread(filter, true)
	{
		mSignal.connect(notify_slot);
	}

	virtual void notify(const std::vector<std::string>& filenames)
	{
		std::list<std::string> files;
		if (!filenames.empty())
		{
			std::copy(filenames.begin(), filenames.end(), files.begin());
		}
		mSignal(files);
	}

	static void open(LLFilePicker::ELoadFilter filter, boost::function<void (std::list<std::string> filenames)> notify_slot)
	{
		(new LLGenericLoadMultipleFilePicker(filter, notify_slot))->getFile();
	}

protected:
	boost::signals2::signal<void (std::list<std::string> filenames)> mSignal;
};
// <FS:CR Threaded Filepickers>

#endif
