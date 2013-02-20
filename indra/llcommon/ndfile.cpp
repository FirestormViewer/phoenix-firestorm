/**
 * $LicenseInfo:firstyear=2013&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2013, Nicky Dasmijn
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

#include "ndfile.h"
#include "llerror.h"
#include "llfile.h"

#ifdef LL_WINDOWS
#include <io.h>
#else
#include <sys/file.h>
#endif

namespace nd
{
	namespace apr
	{
		ndFile::ndFile()
		: mFile(NULL)
		{
		}
		
		ndFile::ndFile(const std::string& filename, apr_int32_t flags, ndVolatileAPRPool* pool)
		: mFile(NULL)
		{
			open(filename, flags, pool);
		}
		
		ndFile::~ndFile()
		{
			close() ;
		}
		
		apr_status_t ndFile::close() 
		{
			FILE *pFile(mFile);
			mFile = 0;
			return close( pFile, 0 );
		}
		
		apr_status_t ndFile::open(const std::string& filename, apr_int32_t flags, ndVolatileAPRPool* pool, S32* sizep)
		{
			return nd::aprhelper::ndOpenFile( filename, flags, mFile, sizep );
		}
		
		apr_status_t ndFile::open(const std::string& filename, apr_int32_t flags, BOOL use_global_pool)
		{
			return nd::aprhelper::ndOpenFile( filename, flags, mFile );
		}
		
		S32 ndFile::read(void *buf, S32 nbytes)
		{
			if( !mFile )
			{
				llwarns << "File is not open, cannot read" << llendl;
				return 0;
			}
		
			S32 read =  fread(buf, 1, nbytes, mFile );
			if( nbytes != read )
			{
				llwarns << "Error when reading, wanted " << nbytes << " read " << read << llendl;
			}

			return read;
		}
		
		S32 ndFile::write(const void *buf, S32 nbytes)
		{
			if( !mFile )
			{
				llwarns << "File is not open, cannot write" << llendl;
				return 0;
			}
		
			S32 written =  fwrite( buf, 1, nbytes, mFile );	
			if( nbytes != written )
			{
				llwarns << "Error when writing, wanted " << nbytes << " wrote " << written  << llendl;
			}
			return written;
		}
		
		S32 ndFile::seek(apr_seek_where_t where, S32 offset)
		{
			return ndFile::seek(mFile, where, offset) ;
		}
		
		apr_status_t ndFile::close(FILE* file_handle, ndVolatileAPRPool* pool) 
		{
			if( 0 == LLFile::close( file_handle ) )
				return APR_SUCCESS;
		
			return APR_OS_START_SYSERR + errno;
		}
		
		FILE* ndFile::open(const std::string& filename, ndVolatileAPRPool* pool, apr_int32_t flags)
		{
			FILE *pFile(0);
			if( APR_SUCCESS == nd::aprhelper::ndOpenFile( filename, flags, pFile ) && pFile )
				return pFile;
		
			return 0;
		}
		
		S32 ndFile::seek(FILE* file_handle, apr_seek_where_t where, S32 offset)
		{
			if( !file_handle )
				return -1;
		
			int seekStatus(0);
			if( offset >= 0 )
				seekStatus = fseek( file_handle, offset, nd::aprhelper::ndConvertSeekFlags( where ) );
			else
				seekStatus = fseek( file_handle, 0, SEEK_END );
		
			if( 0 != seekStatus )
			{
				int err = errno;
				llwarns << "Seek failed with errno " << err << llendl;
				return -1;
			}

			S32 offsetNew = ftell( file_handle );
			if( offset != 0 && offset != offsetNew )
			{
				llwarns << "Seek failed, wanted offset " << offset << " got " << offsetNew << llendl;
			}
			return offsetNew;

		}
		
		S32 ndFile::readEx(const std::string& filename, void *buf, S32 offset, S32 nbytes, ndVolatileAPRPool* pool)
		{
			FILE* file_handle = open(filename, pool, APR_READ|APR_BINARY); 
			if (!file_handle)
				return 0;
		
			llassert(offset >= 0);
		
			if (offset > 0)
				offset = ndFile::seek(file_handle, APR_SET, offset);
			
			apr_size_t bytes_read;
			if (offset < 0)
				bytes_read = 0;
			else
				bytes_read = fread(buf, 1, nbytes, file_handle );
			
			close(file_handle, pool);

			if( nbytes != bytes_read )
			{
				llwarns << "Error when reading, wanted " << nbytes << " read " << bytes_read << " offset " << offset << llendl;
			}
			return (S32)bytes_read;
		}
		
		S32 ndFile::writeEx(const std::string& filename, void *buf, S32 offset, S32 nbytes, ndVolatileAPRPool* pool)
		{
			apr_int32_t flags = APR_CREATE|APR_WRITE|APR_BINARY;
			if (offset < 0)
			{
				flags |= APR_APPEND;
				offset = 0;
			}
			
			FILE* file_handle = open(filename, pool, flags);
		
			if (!file_handle)
				return 0;
		
			if (offset > 0)
				offset = ndFile::seek(file_handle, APR_SET, offset);
			
			apr_size_t bytes_written;
			if (offset < 0)
				bytes_written = 0;
			else
				bytes_written = fwrite(buf, 1, nbytes, file_handle );
		
			ndFile::close(file_handle, pool);

			if( nbytes != bytes_written )
				llwarns << "Error when writing, wanted " << nbytes << " wrote " << bytes_written << " offset " << offset << llendl;

			return (S32)bytes_written;
		}
		
		bool ndFile::remove(const std::string& filename, ndVolatileAPRPool* pool)
		{
			return 0 == LLFile::remove( filename );
		}
		
		bool ndFile::rename(const std::string& filename, const std::string& newname, ndVolatileAPRPool* pool)
		{
			return 0 == LLFile::rename( filename, newname );
		}
		
		bool ndFile::isExist(const std::string& filename, ndVolatileAPRPool* pool, apr_int32_t flags)
		{
			FILE *pFile = LLFile::fopen( filename, nd::aprhelper::ndConvertOpenFlags( flags, filename ) );
			if( pFile )
				LLFile::close( pFile );
		
			return 0 != pFile;
		}
		
		S32 ndFile::size(const std::string& aFilename, ndVolatileAPRPool* pool)
		{
			llstat oStat;
			int nRes = LLFile::stat( aFilename, &oStat );
			if( 0 == nRes )
				return oStat.st_size;

			return 0;
		}
		
		bool ndFile::makeDir(const std::string& dirname, ndVolatileAPRPool* pool)
		{
			return 0 != LLFile::mkdir( dirname );
		}
		
		bool ndFile::removeDir(const std::string& dirname, ndVolatileAPRPool* pool)
		{
			return 0 == LLFile::rmdir( dirname );
		}
	}
}
namespace nd
{
	namespace aprhelper
	{
		std::string ndConvertFilename( std::string const &aFilename )
		{
#ifdef LL_WINDOWS
			// For safety reason (don't change any behaviour) do nothing different if filename is already ASCII
			std::string::const_iterator itr = std::find_if( aFilename.begin(), aFilename.end(), [&]( char const & aVal ){ return aVal < 0; } ); 
			if( aFilename.end() == itr )
				return aFilename;
			
			wchar_t aShort[ MAX_PATH ] = {0};
			DWORD nRes = ::GetShortPathNameW( utf8str_to_utf16str( aFilename ).c_str(), aShort, _countof( aShort ) );
			if( nRes == 0 || nRes >= _countof( aShort ) )
				return aFilename;
			
			return utf16str_to_utf8str( aShort );
#else
			return aFilename;
#endif
		}

		char const *openR = "r";
		char const *openRB = "rb";
		char const *openRP = "r+";
		char const *openRBP = "rb+";
		char const *openW = "w";
		char const *openWB = "wb";
		char const *openWP = "w+";
		char const *openWBP = "wb+";
		char const *openA = "a";
		char const *openAB = "ab";

		char const* ndConvertOpenFlags( apr_int32_t aFlags, std::string const &aFilename )
		{
			bool isBinary = (aFlags & APR_BINARY);
			bool doCreate = (aFlags & APR_CREATE);
			bool doTruncate = (aFlags & APR_TRUNCATE);
			bool doesExist = LLFile::isfile( aFilename );

			if( aFlags & APR_READ && aFlags & APR_WRITE )
			{
				if( doTruncate || !doesExist )
				{
					if( isBinary )
						return openWBP;
					else
						return openWP;
				}
				else
				{
					if( isBinary )
						return openRBP;
					else
						return openRP;
				}
			}
			
			if( aFlags & APR_READ )
			{
				if( isBinary )
					return openRB;
				else
					return openR;
			}
			
			if( aFlags & APR_WRITE )
			{
				if( ( doesExist && !doTruncate ) || !doCreate )
				{
					if( isBinary )
						return openRBP;
					else
						return openRP;
				}
				else
				{
					if( isBinary )
						return openWB;
					else
						return openW;
				}
			}
			
			if( aFlags & APR_APPEND )
			{
				if( isBinary )
					return openAB;
				else
					return openA;
			}
			
			return openR;
		}
		
		apr_status_t ndOpenFile( const std::string& aFilename, apr_int32_t aOpenflags, FILE *&aFileout, S32* aSizeout)
		{
			aFileout = 0;
			if( aSizeout )
				*aSizeout = 0;
			
			apr_status_t s = APR_SUCCESS;
			FILE *pFile = LLFile::fopen( aFilename, ndConvertOpenFlags( aOpenflags, aFilename ) );
			
			if ( !pFile )
			{
				s = APR_OS_START_SYSERR + errno;
			}
			else if (aSizeout)
			{
				llstat oStat;
				int nRes = LLFile::stat( aFilename, &oStat );
				if ( 0 == nRes )
					*aSizeout = oStat.st_size;
				else
				{
					int err = errno;
					llwarns << "stat for file " << aFilename << " failed with errno " << err << llendl;
				}
			}
			
			aFileout = pFile;
			return s;
		}
	}
}

int apr_file_close( FILE *aFile )
{
	if( 0 == fclose( aFile ) )
		return APR_SUCCESS;

	return APR_OS_START_SYSERR+errno;
}

int apr_file_printf( FILE *aFile, char const *aFmt, ... )
{
	va_list vaLst;
	va_start( vaLst, aFmt );
	int nPrinted = vfprintf( aFile, aFmt, vaLst );
	va_end( vaLst );

	if( nPrinted >= 0 )
		return APR_SUCCESS;

	return APR_OS_START_SYSERR+errno;
}

int apr_file_eof( FILE *aFile )
{
	if( 0 == feof(aFile) )
		return APR_SUCCESS;
	else
		return APR_EOF;
}

int apr_file_gets( char *aBuffer, U32 aMax, FILE *aFile )
{
	if( fgets( aBuffer, aMax, aFile ) )
		return APR_SUCCESS;

	return APR_OS_START_SYSERR + ferror( aFile );
}

int apr_file_lock( FILE *aFile, int aLock )
{
#ifndef LL_WINDOWS
	int fd = fileno( aFile );
	if( -1 == fd )
		return APR_OS_START_SYSERR + errno;

	int lockType = LOCK_SH;
	if( aLock & APR_FLOCK_EXCLUSIVE )
		lockType = LOCK_EX;
	if( aLock & APR_FLOCK_NONBLOCK )
		lockType |= LOCK_NB;
	
	int nRes;
	do
	{
		nRes = flock( fd, lockType );
	}
	while( nRes && errno == EINTR );

	if( 0 == nRes )
		return APR_SUCCESS;

	return APR_OS_START_SYSERR + errno;
#else
	int fd = _fileno( aFile );
	if( -1 == fd )
		return APR_OS_START_SYSERR + errno;

	HANDLE fHandle = reinterpret_cast<HANDLE>( _get_osfhandle( fd ) );
	if( INVALID_HANDLE_VALUE == fHandle )
		return APR_OS_START_SYSERR + errno;

	DWORD lockType = 0;
	
	if( aLock & APR_FLOCK_NONBLOCK )
		lockType |= LOCKFILE_FAIL_IMMEDIATELY;
	if( aLock & APR_FLOCK_EXCLUSIVE )
		lockType |= LOCKFILE_EXCLUSIVE_LOCK;

	OVERLAPPED oOverlapped;
	memset( &oOverlapped, 0, sizeof( OVERLAPPED ) );
	if( ::LockFileEx( fHandle, lockType, 0, 0, UINT_MAX, &oOverlapped ) )
		return APR_SUCCESS;

	return APR_OS_START_SYSERR + ::GetLastError();
#endif
}

int apr_file_read( FILE *aFile, void *aBuffer, U32 *aLen )
{
	U32 nRead = fread( aBuffer, 1, *aLen, aFile );
	if( 0 == nRead )
		return APR_OS_START_SYSERR + ferror( aFile );

	*aLen = nRead;
	return APR_SUCCESS;
}
