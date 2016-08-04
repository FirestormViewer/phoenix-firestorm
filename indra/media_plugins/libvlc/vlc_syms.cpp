
#define VLC_SYM( FUNC, RETURN, ...) RETURN (*FUNC)(__VA_ARGS__) = NULL;
#include "vlc_syms.h"
#undef VLC_SYM

struct Symloader
{
    char const *mName;
    apr_dso_handle_sym_t *mPPFunc;
};

#define VLC_SYM( FUNC, RETURN, ...) { #FUNC , (apr_dso_handle_sym_t*)&FUNC}, 
Symloader sSyms[] = {
#include "vlc_syms.h"
{  0, 0 } };
#undef LL_GST_SYM

#ifdef LL_WINDOWS

#define VLC_REG_KEY "Software\\VideoLAN\\VLC"

bool openRegKey( HKEY &aKey )
{
    if( ERROR_SUCCESS == ::RegOpenKeyExA( HKEY_LOCAL_MACHINE, VLC_REG_KEY, 0, KEY_QUERY_VALUE, &aKey ) )
        return true;

    return false;
}

std::string getVLCDir()
{
    std::string ret;
    HKEY hKey;

    if( openRegKey( hKey ) )
    {
        DWORD dwLen(0);
        ::RegQueryValueExA( hKey, "InstallDir", nullptr, nullptr, nullptr, &dwLen );

        if( dwLen > 0 )
        {
            std::vector< char > vctBuffer;
            vctBuffer.resize( dwLen );
            ::RegQueryValueExA( hKey, "InstallDir", nullptr, nullptr, reinterpret_cast< LPBYTE>(&vctBuffer[ 0 ]), &dwLen );
            ret = &vctBuffer[0];

            if( ret[ dwLen-1 ] != '\\' )
                ret += "\\";

            SetDllDirectoryA( ret.c_str() );
        }
        ::RegCloseKey( hKey );
    }
    return ret;
}
#else
std::string getVLCDir() { return ""; }
#endif

static bool sSymsGrabbed = false;
static apr_pool_t *sSymGSTDSOMemoryPool = NULL;
static std::vector< apr_dso_handle_t* > sLoadedLibraries;

bool resolveVLCSymbols( std::vector< std::string > const &aDSONames )
{
    if( sSymsGrabbed )
        return true;

    ll_init_apr();
    //attempt to load the shared libraries
    apr_pool_create(&sSymGSTDSOMemoryPool, NULL);

    apr_dso_handle_t *pDSO(NULL);

    for( std::vector< std::string >::const_iterator itr = aDSONames.begin(); itr != aDSONames.end(); ++itr )
    {
        apr_dso_handle_t *pDSO( NULL );
        std::string strDSO = getVLCDir() + *itr;
        if( APR_SUCCESS == apr_dso_load( &pDSO, strDSO.c_str(), sSymGSTDSOMemoryPool ) )
        {
            sLoadedLibraries.push_back( pDSO );
        }
        else
        {
#if LL_WINDOWS
            OutputDebugStringA( "Cannot load " );
            OutputDebugStringA( strDSO.c_str() );
            OutputDebugStringA( "\n" );
#endif
        }

        for( int i = 0; sSyms[ i ].mName; ++i )
        {
            if( !*sSyms[ i ].mPPFunc )
            {
                apr_dso_sym( sSyms[ i ].mPPFunc, pDSO, sSyms[ i ].mName );
            }
        }
    }

    for( int i = 0; sSyms[ i ].mName; ++i )
    {
        if( !*sSyms[ i ].mPPFunc )
        {
            apr_dso_sym( sSyms[ i ].mPPFunc, pDSO, sSyms[ i ].mName );
        }
    }

    std::stringstream strm;
    bool sym_error = false;
    for( int i = 0; sSyms[ i ].mName; ++i )
    {
        if(  ! *sSyms[ i ].mPPFunc )
        {
            sym_error = true;
            strm << sSyms[ i ].mName << std::endl;
        }
    }

#if LL_WINDOWS
    OutputDebugStringA( strm.str().c_str() );
#endif

    sSymsGrabbed = !sym_error;
    return sSymsGrabbed;
}


bool resolveVLCSymbols( )
{
    std::vector< std::string > vDSO;
#ifdef LL_WINDOWS
    vDSO.push_back( "libvlccore.dll" );
    vDSO.push_back( "libvlc.dll" );
#elif LL_LINUX
    vDSO.push_back( "libvlccore.so" );
    vDSO.push_back( "libvlc.so" );
#endif
    
    return resolveVLCSymbols( vDSO );
}
