/**
 * @file filesystem.h
 * @brief Simulate local file system operations.
 * @Note The initial implementation does actually use standard C++
 *       file operations but eventually, there will be another
 *       layer that caches and manages file meta data too.
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

#include "linden_common.h"

#include "lldir.h"
#include "llfilesystem.h"
#include "llfasttimer.h"
#include "lldiskcache.h"

const S32 LLFileSystem::READ        = 0x00000001;
const S32 LLFileSystem::WRITE       = 0x00000002;
const S32 LLFileSystem::READ_WRITE  = 0x00000003;  // LLFileSystem::READ & LLFileSystem::WRITE
const S32 LLFileSystem::APPEND      = 0x00000006;  // 0x00000004 & LLFileSystem::WRITE

static LLTrace::BlockTimerStatHandle FTM_VFILE_WAIT("VFile Wait");

LLFileSystem::LLFileSystem(const LLUUID& file_id, const LLAssetType::EType file_type, S32 mode)
{
    mFileType = file_type;
    mFileID = file_id;
    mPosition = 0;
    mBytesRead = 0;
    mMode = mode;
}

LLFileSystem::~LLFileSystem()
{
}

// static
bool LLFileSystem::getExists(const LLUUID& file_id, const LLAssetType::EType file_type)
{
    std::string id_str;
    file_id.toString(id_str);
    const std::string extra_info = "";
    const std::string filename = LLDiskCache::getInstance()->metaDataToFilepath(id_str, file_type, extra_info);

    // <FS:Ansariel> IO-streams replacement
    //llifstream file(filename, std::ios::binary);
    //if (file.is_open())
    //{
    //    file.seekg(0, std::ios::end);
    //    return file.tellg() > 0;
    //}
    llstat file_stat;
    if (LLFile::stat(filename, &file_stat) == 0)
    {
        return S_ISREG(file_stat.st_mode) && file_stat.st_size > 0;
    }
    // </FS:Ansariel>

    return false;
}

// static
// <FS:Ansariel> Fix log spam
//bool LLFileSystem::removeFile(const LLUUID& file_id, const LLAssetType::EType file_type)
bool LLFileSystem::removeFile(const LLUUID& file_id, const LLAssetType::EType file_type, int suppress_error /*= 0*/)
// </FS:Ansariel>
{
    std::string id_str;
    file_id.toString(id_str);
    const std::string extra_info = "";
    const std::string filename =  LLDiskCache::getInstance()->metaDataToFilepath(id_str, file_type, extra_info);

    // <FS:Ansariel> Fix log spam
    //LLFile::remove(filename.c_str());
    LLFile::remove(filename.c_str(), suppress_error);
    // </FS:Ansariel>

    return true;
}

// static
bool LLFileSystem::renameFile(const LLUUID& old_file_id, const LLAssetType::EType old_file_type,
                              const LLUUID& new_file_id, const LLAssetType::EType new_file_type)
{
    std::string old_id_str;
    old_file_id.toString(old_id_str);
    const std::string extra_info = "";
    const std::string old_filename =  LLDiskCache::getInstance()->metaDataToFilepath(old_id_str, old_file_type, extra_info);

    std::string new_id_str;
    new_file_id.toString(new_id_str);
    const std::string new_filename =  LLDiskCache::getInstance()->metaDataToFilepath(new_id_str, new_file_type, extra_info);

    // Rename needs the new file to not exist.
    // <FS:Ansariel> Fix log spam
    //LLFileSystem::removeFile(new_file_id, new_file_type);
    LLFileSystem::removeFile(new_file_id, new_file_type, ENOENT);
    // </FS:Ansariel>

    if (LLFile::rename(old_filename, new_filename) != 0)
    {
        // We would like to return FALSE here indicating the operation
        // failed but the original code does not and doing so seems to
        // break a lot of things so we go with the flow...
        //return FALSE;
        LL_WARNS() << "Failed to rename " << old_file_id << " to " << new_id_str << " reason: "  << strerror(errno) << LL_ENDL;
    }

    return TRUE;
}

// static
S32 LLFileSystem::getFileSize(const LLUUID& file_id, const LLAssetType::EType file_type)
{
    std::string id_str;
    file_id.toString(id_str);
    const std::string extra_info = "";
    const std::string filename =  LLDiskCache::getInstance()->metaDataToFilepath(id_str, file_type, extra_info);

    S32 file_size = 0;
    // <FS:Ansariel> IO-streams replacement
    //llifstream file(filename, std::ios::binary);
    //if (file.is_open())
    //{
    //    file.seekg(0, std::ios::end);
    //    file_size = file.tellg();
    //}
    llstat file_stat;
    if (LLFile::stat(filename, &file_stat) == 0)
    {
        file_size = file_stat.st_size;
    }
    // </FS:Ansariel>

    return file_size;
}

BOOL LLFileSystem::read(U8* buffer, S32 bytes)
{
    BOOL success = TRUE;

    std::string id;
    mFileID.toString(id);
    const std::string extra_info = "";
    const std::string filename =  LLDiskCache::getInstance()->metaDataToFilepath(id, mFileType, extra_info);

    // <FS:Ansariel> IO-streams replacement
    //llifstream file(filename, std::ios::binary);
    //if (file.is_open())
    //{
    //    file.seekg(mPosition, std::ios::beg);

    //    file.read((char*)buffer, bytes);

    //    if (file)
    //    {
    //        mBytesRead = bytes;
    //    }
    //    else
    //    {
    //        mBytesRead = file.gcount();
    //    }

    //    file.close();

    //    mPosition += mBytesRead;
    //    if (!mBytesRead)
    //    {
    //        success = FALSE;
    //    }
    //}
    LLFILE* file = LLFile::fopen(filename, "rb");
    if (file)
    {
        fseek(file, mPosition, SEEK_SET);
        mBytesRead = fread(buffer, 1, bytes, file);
        fclose(file);

        mPosition += mBytesRead;
        if (!mBytesRead)
        {
            success = FALSE;
        }
    }
    // </FS:Ansariel>

    // update the last access time for the file - this is required
    // even though we are reading and not writing because this is the
    // way the cache works - it relies on a valid "last accessed time" for
    // each file so it knows how to remove the oldest, unused files
    LLDiskCache::getInstance()->updateFileAccessTime(filename);

    return success;
}

S32 LLFileSystem::getLastBytesRead()
{
    return mBytesRead;
}

BOOL LLFileSystem::eof()
{
    return mPosition >= getSize();
}

BOOL LLFileSystem::write(const U8* buffer, S32 bytes)
{
    std::string id_str;
    mFileID.toString(id_str);
    const std::string extra_info = "";
    const std::string filename =  LLDiskCache::getInstance()->metaDataToFilepath(id_str, mFileType, extra_info);

    BOOL success = FALSE;

    // <FS:Ansariel> IO-streams replacement
    //if (mMode == APPEND)
    //{
    //    llofstream ofs(filename, std::ios::app | std::ios::binary);
    //    if (ofs)
    //    {
    //        ofs.write((const char*)buffer, bytes);

    //        mPosition = ofs.tellp(); // <FS:Ansariel> Fix asset caching

    //        success = TRUE;
    //    }
    //}
    //// <FS:Ansariel> Fix asset caching
    //else if (mMode == READ_WRITE)
    //{
    //    // Don't truncate if file already exists
    //    llofstream ofs(filename, std::ios::in | std::ios::binary);
    //    if (ofs)
    //    {
    //        ofs.seekp(mPosition, std::ios::beg);
    //        ofs.write((const char*)buffer, bytes);
    //        mPosition += bytes;
    //        success = TRUE;
    //    }
    //    else
    //    {
    //        // File doesn't exist - open in write mode
    //        ofs.open(filename, std::ios::binary);
    //        if (ofs.is_open())
    //        {
    //            ofs.write((const char*)buffer, bytes);
    //            mPosition += bytes;
    //            success = TRUE;
    //        }
    //    }
    //}
    //// </FS:Ansariel>
    //else
    //{
    //    llofstream ofs(filename, std::ios::binary);
    //    if (ofs)
    //    {
    //        ofs.write((const char*)buffer, bytes);

    //        mPosition += bytes;

    //        success = TRUE;
    //    }
    //}
    if (mMode == APPEND)
    {
        LLFILE* ofs = LLFile::fopen(filename, "a+b");
        if (ofs)
        {
            fwrite(buffer, 1, bytes, ofs);
            mPosition = ftell(ofs);
            fclose(ofs);
            success = TRUE;
        }
    }
    else if (mMode == READ_WRITE)
    {
        LLFILE* ofs = LLFile::fopen(filename, "r+b");
        if (ofs)
        {
            fseek(ofs, mPosition, SEEK_SET);
            fwrite(buffer, 1, bytes, ofs);
            mPosition = ftell(ofs);
            fclose(ofs);
            success = TRUE;
        }
        else
        {
            ofs = LLFile::fopen(filename, "wb");
            if (ofs)
            {
                fwrite(buffer, 1, bytes, ofs);
                mPosition = ftell(ofs);
                fclose(ofs);
                success = TRUE;
            }
        }
    }
    else
    {
        LLFILE* ofs = LLFile::fopen(filename, "wb");
        if (ofs)
        {
            fwrite(buffer, 1, bytes, ofs);
            mPosition = ftell(ofs);
            fclose(ofs);
            success = TRUE;
        }
    }
    // </FS:Ansariel>

    return success;
}

BOOL LLFileSystem::seek(S32 offset, S32 origin)
{
    if (-1 == origin)
    {
        origin = mPosition;
    }

    S32 new_pos = origin + offset;

    S32 size = getSize();

    if (new_pos > size)
    {
        LL_WARNS() << "Attempt to seek past end of file" << LL_ENDL;

        mPosition = size;
        return FALSE;
    }
    else if (new_pos < 0)
    {
        LL_WARNS() << "Attempt to seek past beginning of file" << LL_ENDL;

        mPosition = 0;
        return FALSE;
    }

    mPosition = new_pos;
    return TRUE;
}

S32 LLFileSystem::tell() const
{
    return mPosition;
}

S32 LLFileSystem::getSize()
{
    return LLFileSystem::getFileSize(mFileID, mFileType);
}

S32 LLFileSystem::getMaxSize()
{
    // offer up a huge size since we don't care what the max is
    return INT_MAX;
}

BOOL LLFileSystem::rename(const LLUUID& new_id, const LLAssetType::EType new_type)
{
    LLFileSystem::renameFile(mFileID, mFileType, new_id, new_type);

    mFileID = new_id;
    mFileType = new_type;

    return TRUE;
}

BOOL LLFileSystem::remove()
{
    LLFileSystem::removeFile(mFileID, mFileType);

    return TRUE;
}
