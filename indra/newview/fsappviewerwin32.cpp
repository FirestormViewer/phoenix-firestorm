// <FS:ND> Thread to purge old texture cache in the background.
// The cache dir will be search for directories named *.old_texturecache, then each of this directories
// will be deleted.
// The thread will be started each time the viewer starts, just in case there is directories so huge,
// the user quit the viewer before the old cache was fully cleared.

void deleteFilesInDirectory( std::wstring aDir )
{
	if( aDir == L"." || aDir == L".." || aDir.empty() )
		return;

	if( aDir[ aDir.size() -1 ] != '\\' || aDir[ aDir.size() -1 ] != '/' )
		aDir += L"\\";

	WIN32_FIND_DATA oFindData;
	HANDLE hFindHandle = ::FindFirstFile( (aDir + L"*.*").c_str(), &oFindData );

	if( INVALID_HANDLE_VALUE == hFindHandle )
		return;

	do
	{
		if( ! (oFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
		{
			std::wstring strFile = aDir + oFindData.cFileName;
			if( oFindData.dwFileAttributes & FILE_ATTRIBUTE_READONLY )
				::SetFileAttributes( strFile.c_str(), FILE_ATTRIBUTE_NORMAL );

			::DeleteFile( ( aDir + oFindData.cFileName ).c_str() );
		}
	} while( ::FindNextFile( hFindHandle, &oFindData ) );

	::FindClose( hFindHandle );
}

void deleteCacheDirectory( std::wstring aDir )
{
	if( aDir == L"." || aDir == L".." || aDir.empty() )
		return;

	if( aDir[ aDir.size() -1 ] != '\\' || aDir[ aDir.size() -1 ] != '/' )
		aDir += L"\\";

	wchar_t aCacheDirs[] = L"0123456789abcdef";

	for( int i = 0; i < _countof( aCacheDirs ); ++i )
	{
		deleteFilesInDirectory( aDir + aCacheDirs[i] );
		::RemoveDirectory( (aDir + aCacheDirs[i]).c_str() );
	}

	deleteFilesInDirectory( aDir );
	::RemoveDirectory( aDir.c_str() );
}

DWORD WINAPI purgeThread( LPVOID lpParameter )
{
	wchar_t *pDir = reinterpret_cast< wchar_t* >( lpParameter );
	if( !pDir )
		return 0;

	std::wstring strPath = pDir;
	free( pDir );

	if( strPath.empty() )
		return 0;

	if( strPath[ strPath.size() -1 ] != '\\' || strPath[ strPath.size() -1 ] != '/' )
		strPath += L"\\";

	WIN32_FIND_DATA oFindData;
	HANDLE hFindHandle = ::FindFirstFile( ( strPath + L"*.old_texturecache" ).c_str(), &oFindData );

	std::vector< std::wstring > vctDirs;

	if( INVALID_HANDLE_VALUE == hFindHandle )
		return 0;

	do
	{
		if( oFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			vctDirs.push_back( strPath + oFindData.cFileName );
	} while( ::FindNextFile( hFindHandle, &oFindData ) );

	::FindClose( hFindHandle );

	for( auto dir : vctDirs )
		deleteCacheDirectory( dir );

	return 0;
}

void LLAppViewerWin32::startCachePurge()
{
	if( isSecondInstance() )
		return;

    std::wstring strCacheDir = wstringize( gDirUtilp->getExpandedFilename( LL_PATH_CACHE, "" ) );

	HANDLE hThread = CreateThread( nullptr, 0, purgeThread, _wcsdup( strCacheDir.c_str() ), 0, nullptr );

	if( !hThread )
	{
        LL_WARNS("CachePurge") << "CreateThread failed: "  << GetLastError() << LL_ENDL;
    }
	else
		SetThreadPriority( hThread, THREAD_MODE_BACKGROUND_BEGIN );
}

// </FS:ND>
