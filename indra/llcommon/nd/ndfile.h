#ifndef NDAPRFILEREPLACEMENT_H
#define NDAPRFILEREPLACEMENT_H

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

#include <boost/noncopyable.hpp>
#include <stdio.h>

#if LL_LINUX || LL_SOLARIS
#include <sys/param.h>  // Need PATH_MAX in APR headers...
#endif

#include <boost/noncopyable.hpp>

#include "apr_thread_proc.h"
#include "apr_thread_mutex.h"
#include "apr_getopt.h"
#include "apr_signal.h"
#include "apr_atomic.h"


#include "llpreprocessor.h"
#include "llstring.h"

namespace nd
{
	namespace apr
	{
		class LL_COMMON_API ndFile : boost::noncopyable
		{
		private:
			FILE *mFile;
		
		public:
			typedef FILE tFiletype;
			typedef void ndVolatileAPRPool;

			ndFile() ;
			ndFile(const std::string& filename, apr_int32_t flags, ndVolatileAPRPool* pool = NULL);
			~ndFile() ;
			
			apr_status_t open(const std::string& filename, apr_int32_t flags, ndVolatileAPRPool* pool = NULL, S32* sizep = NULL);
			apr_status_t open(const std::string& filename, apr_int32_t flags, BOOL use_global_pool); //use gAPRPoolp.
			apr_status_t close() ;
		
			// Returns actual offset, -1 if seek fails
			S32 seek(apr_seek_where_t where, S32 offset);
		
			apr_status_t eof() { return feof(mFile)==0?APR_SUCCESS:APR_EOF;}
		
			// Returns bytes read/written, 0 if read/write fails:
			S32 read(void* buf, S32 nbytes);
			S32 write(const void* buf, S32 nbytes);
		
			tFiletype* getFileHandle() {return mFile;}	
		
		private:
			static FILE* open(const std::string& filename, ndVolatileAPRPool* pool, apr_int32_t flags);
			static apr_status_t close(FILE* file, ndVolatileAPRPool* pool) ;
			static S32 seek(FILE* file, apr_seek_where_t where, S32 offset);
		
		public:
			// returns false if failure:
			static bool remove(const std::string& filename, ndVolatileAPRPool* pool = NULL);
			static bool rename(const std::string& filename, const std::string& newname, ndVolatileAPRPool* pool = NULL);
			static bool isExist(const std::string& filename, ndVolatileAPRPool* pool = NULL, apr_int32_t flags = APR_READ);
			static S32 size(const std::string& filename, ndVolatileAPRPool* pool = NULL);
			static bool makeDir(const std::string& dirname, ndVolatileAPRPool* pool = NULL);
			static bool removeDir(const std::string& dirname, ndVolatileAPRPool* pool = NULL);
		
			// Returns bytes read/written, 0 if read/write fails:
			static S32 readEx(const std::string& filename, void *buf, S32 offset, S32 nbytes, ndVolatileAPRPool* pool = NULL);	
			static S32 writeEx(const std::string& filename, void *buf, S32 offset, S32 nbytes, ndVolatileAPRPool* pool = NULL); // offset<0 means append
		//*******************************************************************************************************************************
		};
	}
}

int LL_COMMON_API apr_file_close( FILE* );
int LL_COMMON_API apr_file_printf( FILE*, char const*, ... );
int LL_COMMON_API apr_file_eof( FILE* );
int LL_COMMON_API apr_file_gets( char*, U32, FILE* );
int LL_COMMON_API apr_file_lock( FILE*, int );
int LL_COMMON_API apr_file_read( FILE*, void*, U32* );

namespace nd
{
	namespace aprhelper
	{
		std::string LL_COMMON_API ndConvertFilename( std::string const &aFilename );
		char const* LL_COMMON_API ndConvertOpenFlags( apr_int32_t, std::string const& );

		inline bool ndIsCreateFile( apr_int32_t aFlags) { return APR_CREATE == ( aFlags & (APR_CREATE|APR_TRUNCATE) ); }
		inline S32 ndConvertSeekFlags( apr_seek_where_t aWhere ) { return aWhere; }
		inline apr_status_t ndOpenFile( const std::string& aFilename, apr_int32_t aOpenflags, FILE *&aFileout, S32* aSizeout = 0);
	}
}

#endif
