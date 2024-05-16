/**
* @file llcrashlookupwindows.cpp
* @brief Basic Windows crash analysis
*
* Copyright (C) 2011, Kitty Barnett
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
*/
#include "linden_common.h"

#include "stdafx.h"
#include "resource.h"

#include "llcrashlookupwindows.h"

#define MAX_STACK_FRAMES    64

#ifdef LL_SEND_CRASH_REPORTS

LLCrashLookupWindows::LLCrashLookupWindows()
    : LLCrashLookup()
    , m_pDbgClient(NULL)
    , m_pDbgControl(NULL)
    , m_pDbgSymbols(NULL)
    , m_pDbgClient4(0)
{
    CoInitialize(NULL);

    // Create the base IDebugClient object and then query it for the class instances we're interested in
    HRESULT hRes = DebugCreate(__uuidof(IDebugClient), (void**)&m_pDbgClient);
    if (SUCCEEDED(hRes))
    {
        hRes = m_pDbgClient->QueryInterface(__uuidof(IDebugControl4), (void**)&m_pDbgControl);
        if (FAILED(hRes))
            return;
        hRes = m_pDbgClient->QueryInterface(__uuidof(IDebugSymbols2), (void**)&m_pDbgSymbols);
        if (FAILED(hRes))
            return;

        m_pDbgClient->QueryInterface( __uuidof(IDebugClient4), (void**)&m_pDbgClient4 );
    }
}

LLCrashLookupWindows::~LLCrashLookupWindows()
{
    if (m_pDbgSymbols)
    {
        m_pDbgSymbols->Release();
        m_pDbgSymbols = NULL;
    }
    if (m_pDbgControl)
    {
        m_pDbgControl->Release();
        m_pDbgControl = NULL;
    }

    if( m_pDbgClient4 )
        m_pDbgClient4->Release();

    if (m_pDbgClient)
    {
        m_pDbgClient->Release();
        m_pDbgClient = NULL;
    }

    CoUninitialize();
}

bool LLCrashLookupWindows::initFromDump(const std::string& strDumpPath)
{
    m_nInstructionAddr = m_nModuleBaseAddr = 0;
    m_strModule.clear();
    m_nModuleTimeStamp = m_nModuleChecksum = 0;
    m_nModuleVersion = 0;

    // Sanity check - make sure we have all the class instances
    if ( (!m_pDbgClient) || (!m_pDbgControl) || (!m_pDbgSymbols) )
        return false;

    std::wstring strDumpPathW = utf8str_to_utf16str( strDumpPath );
    // Open the minidump and wait to finish processing
    HRESULT hRes(S_OK);

    if( !m_pDbgClient4 )
        hRes = m_pDbgClient->OpenDumpFile (strDumpPath.c_str());
    else
        hRes = m_pDbgClient4->OpenDumpFileWide (strDumpPathW.c_str(),0);

    if (FAILED(hRes))
        return false;
    m_pDbgControl->WaitForEvent(DEBUG_WAIT_DEFAULT, INFINITE);

    // Try to find an event that describes an exception
    ULONG nEventType = 0, nProcessId = 0, nThreadId = 0;
    BYTE bufContext[1024] = {0}; ULONG szContext = 0;
    hRes = m_pDbgControl->GetStoredEventInformation(
        &nEventType, &nProcessId, &nThreadId, bufContext, sizeof(bufContext), &szContext, NULL, 0, 0);
    if ( (FAILED(hRes)) || (DEBUG_EVENT_EXCEPTION != nEventType) )
        return false;

    // Get the stack trace for the exception
    DEBUG_STACK_FRAME dbgStackFrames[MAX_STACK_FRAMES]; ULONG cntStackFrames = 0;
    BYTE* pbufStackFrameContexts = new BYTE[MAX_STACK_FRAMES * szContext];
    hRes = m_pDbgControl->GetContextStackTrace(
        bufContext, szContext, dbgStackFrames, ARRAYSIZE(dbgStackFrames),
        pbufStackFrameContexts, MAX_STACK_FRAMES * szContext, szContext, &cntStackFrames);
    if ( (FAILED(hRes)) || (cntStackFrames < 1) )
        return false;

    // Since the user won't have any debug symbols present we're really only interested in the top stack frame
    m_nInstructionAddr = dbgStackFrames[0].InstructionOffset;
    ULONG idxModule = 0;
    hRes = m_pDbgSymbols->GetModuleByOffset(m_nInstructionAddr, 0, &idxModule, &m_nModuleBaseAddr);
    if (FAILED(hRes))
        return false;

    // Lookup the name of the module where the crash occurred
    CHAR strModule[MAX_PATH] = {0};
    hRes = m_pDbgSymbols->GetModuleNameString(
        DEBUG_MODNAME_MODULE, DEBUG_ANY_ID, m_nModuleBaseAddr, strModule, ARRAYSIZE(strModule) - 1, NULL);
    if (FAILED(hRes))
        return false;
    m_strModule = strModule;

    // Grab some basic properties we use for verification of the image
    DEBUG_MODULE_PARAMETERS dbgModuleParams;
    hRes = m_pDbgSymbols->GetModuleParameters(1, &m_nModuleBaseAddr, 0, &dbgModuleParams);
    if ( (SUCCEEDED(hRes)) && (DEBUG_INVALID_OFFSET != dbgModuleParams.Base) )
    {
        m_nModuleTimeStamp = dbgModuleParams.TimeDateStamp;
        m_nModuleChecksum = dbgModuleParams.Checksum;
    }

    // Grab the version number as well
    BYTE bufVersionInfo[1024] = {0};
    hRes = m_pDbgSymbols->GetModuleVersionInformation(DEBUG_ANY_ID, m_nModuleBaseAddr, "\\", bufVersionInfo, 1024, NULL);
    if (SUCCEEDED(hRes))
    {
        VS_FIXEDFILEINFO* pFileInfo = (VS_FIXEDFILEINFO*)bufVersionInfo;
        m_nModuleVersion = ((U64)pFileInfo->dwFileVersionMS << 32) + pFileInfo->dwFileVersionLS;
    }

    return true;
}

#endif // LL_SEND_CRASH_REPORTS
