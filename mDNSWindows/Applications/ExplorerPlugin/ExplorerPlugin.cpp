/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@

    Change History (most recent first):
    
$Log: ExplorerPlugin.cpp,v $
Revision 1.1  2004/01/30 03:01:56  bradley
Explorer Plugin to browse for Rendezvous-enabled Web and FTP servers from within Internet Explorer.

*/

#include	"StdAfx.h"

// The following 2 includes have to be in this order and INITGUID must be defined here, before including the file
// that specifies the GUID(s), and nowhere else. The reason for this is that initguid.h doesn't provide separate 
// define and declare macros for GUIDs so you have to #define INITGUID in the single file where you want to define 
// your GUID then in all the other files that just need the GUID declared, INITGUID must not be defined.

#define	INITGUID
#include	<initguid.h>
#include	"ExplorerPlugin.h"

#include	<comcat.h>
#include	<Shlwapi.h>

#include	"CommonServices.h"
#include	"DebugServices.h"

#include	"ClassFactory.h"
#include	"Resource.h"

// MFC Debugging

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

// DLL Exports

extern "C" BOOL WINAPI	DllMain( HINSTANCE inInstance, DWORD inReason, LPVOID inReserved );

// MFC Support

DEBUG_LOCAL OSStatus	MFCDLLProcessAttach( HINSTANCE inInstance );
DEBUG_LOCAL void		MFCDLLProcessDetach( HINSTANCE inInstance );
DEBUG_LOCAL void		MFCDLLThreadDetach( HINSTANCE inInstance );

// Utilities

DEBUG_LOCAL OSStatus	RegisterServer( HINSTANCE inInstance, CLSID inCLSID, LPCTSTR inName );
DEBUG_LOCAL OSStatus	RegisterCOMCategory( CLSID inCLSID, CATID inCategoryID, BOOL inRegister );

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

HINSTANCE		gInstance		= NULL;
int				gDLLRefCount	= 0;
CWinApp			gApp;

#if 0
#pragma mark -
#pragma mark == DLL Exports ==
#endif

//===========================================================================================================================
//	DllMain
//===========================================================================================================================

BOOL WINAPI	DllMain( HINSTANCE inInstance, DWORD inReason, LPVOID inReserved )
{
	BOOL			ok;
	OSStatus		err;
	
	DEBUG_UNUSED( inReserved );
	
	ok = TRUE;
	switch( inReason )
	{
		case DLL_PROCESS_ATTACH:
			gInstance = inInstance;
			debug_initialize( kDebugOutputTypeWindowsEventLog, "RendezvousBar", inInstance );
			debug_set_property( kDebugPropertyTagPrintLevel, kDebugLevelTrace );
			dlog( kDebugLevelTrace, "\nDllMain: process attach\n" );
			
			err = MFCDLLProcessAttach( inInstance );
			ok = ( err == kNoErr );
			require_noerr( err, exit );
			break;
		
		case DLL_PROCESS_DETACH:
			dlog( kDebugLevelTrace, "DllMain: process detach\n" );
			MFCDLLProcessDetach( inInstance );
			break;
		
		case DLL_THREAD_ATTACH:
			dlog( kDebugLevelTrace, "DllMain: thread attach\n" );
			break;
		
		case DLL_THREAD_DETACH:
			dlog( kDebugLevelTrace, "DllMain: thread detach\n" );
			MFCDLLThreadDetach( inInstance );
			break;
		
		default:
			dlog( kDebugLevelTrace, "DllMain: unknown reason code (%d)\n",inReason );
			break;
	}
	
exit:
	return( ok );
}

//===========================================================================================================================
//	DllCanUnloadNow
//===========================================================================================================================

STDAPI	DllCanUnloadNow( void )
{
	dlog( kDebugLevelTrace, "DllCanUnloadNow (refCount=%d)\n", gDLLRefCount );
	
	return( gDLLRefCount == 0 );
}

//===========================================================================================================================
//	DllGetClassObject
//===========================================================================================================================

STDAPI	DllGetClassObject( REFCLSID inCLSID, REFIID inIID, LPVOID *outResult )
{
	HRESULT				err;
	BOOL				ok;
	ClassFactory *		factory;
	
	dlog( kDebugLevelTrace, "DllGetClassObject\n" );
	
	*outResult = NULL;
	
	// Check if the class ID is supported.
	
	ok = IsEqualCLSID( inCLSID, CLSID_ExplorerBar );
	require_action_quiet( ok, exit, err = CLASS_E_CLASSNOTAVAILABLE );
	
	// Create the ClassFactory object.
	
	factory = NULL;
	try
	{
		factory = new ClassFactory( inCLSID );
	}
	catch( ... )
	{
		// Do not let exception escape.
	}
	require_action( factory, exit, err = E_OUTOFMEMORY );
	
	// Query for the specified interface. Release the factory since QueryInterface retains it.
	
	err = factory->QueryInterface( inIID, outResult );
	factory->Release();
	
exit:
	return( err );
}

//===========================================================================================================================
//	DllRegisterServer
//===========================================================================================================================

STDAPI	DllRegisterServer( void )
{
	HRESULT		err;
	BOOL		ok;
	CString		s;
	
	dlog( kDebugLevelTrace, "DllRegisterServer\n" );
	
	ok = s.LoadString( IDS_NAME );
	require_action( ok, exit, err = E_UNEXPECTED );
	
	err = RegisterServer( gInstance, CLSID_ExplorerBar, s );
	require_noerr( err, exit );
	
	err = RegisterCOMCategory( CLSID_ExplorerBar, CATID_InfoBand, TRUE );
	require_noerr( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	DllUnregisterServer
//===========================================================================================================================

STDAPI	DllUnregisterServer( void )
{
	HRESULT		err;
	
	dlog( kDebugLevelTrace, "DllUnregisterServer\n" );
	
	err = RegisterCOMCategory( CLSID_ExplorerBar, CATID_InfoBand, FALSE );
	require_noerr( err, exit );
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == MFC Support ==
#endif

//===========================================================================================================================
//	MFCDLLProcessAttach
//===========================================================================================================================

DEBUG_LOCAL OSStatus	MFCDLLProcessAttach( HINSTANCE inInstance )
{
	OSStatus				err;
	_AFX_THREAD_STATE *		threadState;
	AFX_MODULE_STATE *		previousModuleState;
	BOOL					ok;
	CWinApp *				app;
	
	app = NULL;
	
	// Simulate what is done in dllmodul.cpp.
	
	threadState = AfxGetThreadState();
	check( threadState );
	previousModuleState = threadState->m_pPrevModuleState;
	
	ok = AfxWinInit( inInstance, NULL, TEXT( "" ), 0 );
	require_action( ok, exit, err = kUnknownErr );
	
	app = AfxGetApp();
	require_action( ok, exit, err = kNotInitializedErr );
	
	ok = app->InitInstance();
	require_action( ok, exit, err = kUnknownErr );
	
	threadState->m_pPrevModuleState = previousModuleState;
	threadState = NULL;
	AfxInitLocalData( inInstance );
	err = kNoErr;
	
exit:
	if( err )
	{
		if( app )
		{
			app->ExitInstance();
		}
		AfxWinTerm();
	}
	if( threadState )
	{
		threadState->m_pPrevModuleState = previousModuleState;
	}
	return( err );
}

//===========================================================================================================================
//	MFCDLLProcessDetach
//===========================================================================================================================

DEBUG_LOCAL void	MFCDLLProcessDetach( HINSTANCE inInstance )
{
	CWinApp *		app;

	// Simulate what is done in dllmodul.cpp.
	
	app = AfxGetApp();
	if( app )
	{
		app->ExitInstance();
	}

#if( DEBUG )
	if( AfxGetModuleThreadState()->m_nTempMapLock != 0 )
	{
		dlog( kDebugLevelWarning, "Warning: Temp map lock count non-zero (%ld).\n", AfxGetModuleThreadState()->m_nTempMapLock );
	}
#endif
	
	AfxLockTempMaps();
	AfxUnlockTempMaps( -1 );

	// Terminate the library before destructors are called.
	
	AfxWinTerm();
	AfxTermLocalData( inInstance, TRUE );
}

//===========================================================================================================================
//	MFCDLLFinalize
//===========================================================================================================================

DEBUG_LOCAL void	MFCDLLThreadDetach( HINSTANCE inInstance )
{
	// Simulate what is done in dllmodul.cpp.
	
#if( DEBUG )
	if( AfxGetModuleThreadState()->m_nTempMapLock != 0 )
	{
		dlog( kDebugLevelWarning, "Warning: Temp map lock count non-zero (%ld).\n", AfxGetModuleThreadState()->m_nTempMapLock );
	}
#endif
	
	AfxLockTempMaps();
	AfxUnlockTempMaps( -1 );
	AfxTermThread( inInstance );
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

//===========================================================================================================================
//	RegisterServer
//===========================================================================================================================

DEBUG_LOCAL OSStatus	RegisterServer( HINSTANCE inInstance, CLSID inCLSID, LPCTSTR inName )
{
	typedef struct	RegistryBuilder		RegistryBuilder;
	struct	RegistryBuilder
	{
		HKEY		rootKey;
		LPCTSTR		subKey;
		LPCTSTR		valueName;
		LPCTSTR		data;
	};
	
	OSStatus			err;
	LPWSTR				clsidWideString;
	TCHAR				clsidString[ 64 ];
	DWORD				nChars;
	size_t				n;
	size_t				i;
	HKEY				key;
	TCHAR				keyName[ MAX_PATH ];
	TCHAR				moduleName[ MAX_PATH ] = TEXT( "" );
	TCHAR				data[ MAX_PATH ];
	RegistryBuilder		entries[] = 
	{
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s" ),					NULL,						inName },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\InprocServer32" ),	NULL,						moduleName },
		{ HKEY_CLASSES_ROOT,	TEXT( "CLSID\\%s\\InprocServer32" ),  	TEXT( "ThreadingModel" ),	TEXT( "Apartment" ) }
	};
	DWORD				size;
	OSVERSIONINFO		versionInfo;
	
	// Convert the CLSID to a string based on the encoding of this code (ANSI or Unicode).
	
	err = StringFromIID( inCLSID, &clsidWideString );
	require_noerr( err, exit );
	require_action( clsidWideString, exit, err = kNoMemoryErr );
	
	#ifdef UNICODE
		lstrcpyn( clsidString, clsidWideString, sizeof_array( clsidString ) );
		CoTaskMemFree( clsidWideString );
	#else
		nChars = WideCharToMultiByte( CP_ACP, 0, clsidWideString, -1, clsidString, sizeof_array( clsidString ), NULL, NULL );
		err = translate_errno( nChars > 0, (OSStatus) GetLastError(), kUnknownErr );
		CoTaskMemFree( clsidWideString );
		require_noerr( err, exit );
	#endif
	
	// Register the CLSID entries.
	
	nChars = GetModuleFileName( inInstance, moduleName, sizeof_array( moduleName ) );
	err = translate_errno( nChars > 0, (OSStatus) GetLastError(), kUnknownErr );
	require_noerr( err, exit );

	n = sizeof_array( entries );
	for( i = 0; i < n; ++i )
	{
		wsprintf( keyName, entries[ i ].subKey, clsidString );		
		err = RegCreateKeyEx( entries[ i ].rootKey, keyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL );
		require_noerr( err, exit );
		
		size = (DWORD)( ( lstrlen( entries[ i ].data ) + 1 ) * sizeof( TCHAR ) );
		err = RegSetValueEx( key, entries[ i ].valueName, 0, REG_SZ, (LPBYTE) entries[ i ].data, size );
		RegCloseKey( key );
		require_noerr( err, exit );
	}
	
	// If running on NT, register the extension as approved.
	
	versionInfo.dwOSVersionInfoSize = sizeof( versionInfo );
	GetVersionEx( &versionInfo );
	if( versionInfo.dwPlatformId == VER_PLATFORM_WIN32_NT )
	{
		lstrcpyn( keyName, TEXT( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved" ), sizeof_array( keyName ) );
		err = RegCreateKeyEx( HKEY_LOCAL_MACHINE, keyName, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL );
		require_noerr( err, exit );
		
		lstrcpyn( data, inName, sizeof_array( data ) );
		size = (DWORD)( ( lstrlen( data ) + 1 ) * sizeof( TCHAR ) );
		err = RegSetValueEx( key, clsidString, 0, REG_SZ, (LPBYTE) data, size );
		RegCloseKey( key );
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	RegisterCOMCategory
//===========================================================================================================================

DEBUG_LOCAL OSStatus	RegisterCOMCategory( CLSID inCLSID, CATID inCategoryID, BOOL inRegister )
{
	HRESULT				err;
	ICatRegister *		cat;

	err = CoInitialize( NULL );
	require( SUCCEEDED( err ), exit );
	
	err = CoCreateInstance( CLSID_StdComponentCategoriesMgr, NULL, CLSCTX_INPROC_SERVER, IID_ICatRegister, (LPVOID *) &cat );
	check( SUCCEEDED( err ) );
	if( SUCCEEDED( err ) )
	{
		if( inRegister )
		{
			err = cat->RegisterClassImplCategories( inCLSID, 1, &inCategoryID );
			check_noerr( err );
		}
		else
		{
			err = cat->UnRegisterClassImplCategories( inCLSID, 1, &inCategoryID );
			check_noerr( err );
		}
		cat->Release();
	}
	CoUninitialize();

exit:
	return( err );
}
