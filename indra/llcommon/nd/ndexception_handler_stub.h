#ifndef NDEXCEPTIONHANDLERSTUB_H
#define NDEXCEPTIONHANDLERSTUB_H


/**
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Nicky Dasmijn
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

namespace google_breakpad
{
	class MinidumpDescriptor
	{
		std::string mPath;
	public:
		MinidumpDescriptor( std::string const & )
		{ }

		char const* path() const
		{ return mPath.c_str(); }
	};

	typedef bool (*tDumpFunc)(const char*, const char*, void*, bool);
	typedef bool (*tDumpFunc2)( MinidumpDescriptor const&, void*, bool );
	typedef bool (*FilterCallback)(void *context);


	class ExceptionHandler
	{
	public:
		ExceptionHandler( std::string const&, int, tDumpFunc, int, bool, int )
		{ }
		ExceptionHandler( MinidumpDescriptor const&, FilterCallback, tDumpFunc2, void*, bool, int )
		{ }

		void set_dump_path( std::string const& )
		{ }
		void set_dump_path( std::wstring const& )
		{ }
		void set_minidump_descriptor( MinidumpDescriptor const& )
		{ }

		void WriteMinidump()
		{ }
	};
};

#if LL_WINDOWS
typedef void MDRawAssertionInfo;
#endif

#endif
