/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

extern HANDLE hReconSema;
bool g_fRealTime = false;
bool g_fHide = false;
bool g_fLowerPriv = false;
HINSTANCE g_hInst = NULL;
HWND g_hWnd = NULL;
HWND g_hListDlg = NULL;
HWND g_hSettingsDlg = NULL;
BOOL g_bLogging = FALSE;
static HHOOK hKeyboardHook = NULL;
static LRESULT CALLBACK KeyboardHookProc( int nCode, WPARAM wParam, LPARAM lParam );

unsigned __stdcall ReconThread( void * pv );
BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );

void ReadIni( BOOL& bAllowMulti, BOOL& bLogging );
void ShowCommandLineHelp( HWND hWnd );
CLEAN_POINTER DWORD * PathToProcessIDs( const PATHINFO& pathInfo, ptrdiff_t& numOfPIDs );

static DWORD _bes_init( int& nCmdShow );
static void _bes_uninit( void );
static void _prevent_dll_preloading( void );
static HWND _create_window( HINSTANCE hInstance, DWORD dwStyle );
#if defined(_MSC_VER) && 1400 <= _MSC_VER
void invalid_parameter_handler( const wchar_t * expr, const wchar_t * func, const wchar_t * file, unsigned int line, uintptr_t reserved )
{
	(void) expr; (void) func; (void) file; (void) line; (void) reserved;
	MessageBox( NULL, _T("Invalid Parameter!"), APP_NAME, MB_OK | MB_ICONERROR );
}
#endif

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
#ifdef _DEBUG
	OutputDebugString(_T("WinMain\n"));
#endif
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );
	_prevent_dll_preloading();

	g_hInst = hInstance;

	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

#if defined(_MSC_VER) && 1400 <= _MSC_VER
	_set_invalid_parameter_handler( invalid_parameter_handler );
#endif

	const DWORD dwStyle = _bes_init( nCmdShow );

	if( ! dwStyle )
	{
		_bes_uninit();
		return 0;
	}

	HANDLE hEvent_Exiting
		= CreateEvent(
			NULL,  // default security descriptor
			TRUE,  // manual-reset: once signaled, remains signaled until manually reset
			FALSE, // initially non-signaled
			NULL   // created without a name
		);
	if( ! hEvent_Exiting )
	{
		_bes_uninit();
		return 0;
	}

	hReconSema = CreateSemaphore( NULL, 1L, 1L, NULL );
	if( ! hReconSema )
	{
		CloseHandle( hEvent_Exiting );
		_bes_uninit();
		return 0;
	}

	OleInitialize( NULL ); //*

	HWND hWnd = _create_window( hInstance, dwStyle );

	if( ! hWnd )
	{
		if( hReconSema )
		{
			CloseHandle( hReconSema );
			hReconSema = NULL;
		}
		OleUninitialize(); //*
		CloseHandle( hEvent_Exiting );
		_bes_uninit();
		return 0;
	}

	g_hWnd = hWnd;

	HACCEL hAccelTable
		= LoadAccelerators( hInstance, MAKEINTRESOURCE( IDC_BATTLEENC ) ); // auto-freed
	
	ShowWindow( hWnd, nCmdShow );
	if( nCmdShow != SW_SHOWMINNOACTIVE )
	{
		// Workaround to make AutoHotkey happy
		BringWindowToTop( hWnd );
		SetForegroundWindow( hWnd );
		SetActiveWindow( hWnd );
		SetFocus( g_hListDlg ? g_hListDlg : g_hSettingsDlg ? g_hSettingsDlg : hWnd );
	}
	UpdateWindow( hWnd );

	HANDLE hToken = NULL;
	DWORD dwPrevAttributes = 0;
	BOOL bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	HANDLE hReconThread = CreateThread2( &ReconThread, hEvent_Exiting );

	MSG msg;
	while ( GetMessage( &msg, NULL, 0U, 0U ) )
	{
		if( msg.hwnd != hWnd )
		{
			if( msg.message == WM_LBUTTONUP )
			{
				SendMessage( hWnd, WM_USER_BUTTON, (WPARAM) msg.hwnd, 0L );
			}
		}
	
		/*if( g_hListDlg && IsDialogMessage( g_hListDlg, &msg ) )
		{
			;
		}
		else */if( ! TranslateAccelerator( msg.hwnd, hAccelTable, &msg ) ) 
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}
	SetEvent( hEvent_Exiting );

	if( hToken )
	{
		if( bPriv ) 
			AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}

	WaitForSingleObject( hReconThread, 3000 );
	CloseHandle( hReconThread );
	if( hReconSema )
	{
		CloseHandle( hReconSema );
		hReconSema = NULL;
	}

	OleUninitialize(); //*
	CloseHandle( hEvent_Exiting );

	_bes_uninit();

	return (int) msg.wParam;
}

typedef BOOL (WINAPI * SetDllDirectory_t)(LPCTSTR);
static void _prevent_dll_preloading( void )
{
	// Quick-fix to prevent DLL preloading attacks
	TCHAR szCurDir[ CCHPATH ] = _T("");
	GetCurrentDirectory( CCHPATH, szCurDir );
	TCHAR szSysDir[ CCHPATH ] = _T("");
	GetSystemDirectory( szSysDir, CCHPATH );
	SetCurrentDirectory( szSysDir );

	// Real fix
	HMODULE hModuleKernel32 = LoadLibrary( _T("Kernel32.dll") );
	if( hModuleKernel32 )
	{
		SetDllDirectory_t pfnSetDllDirectory = (SetDllDirectory_t)(void *)
			GetProcAddress( hModuleKernel32, "SetDllDirectory" WorA_ );

		// This is a better fix, but only valid for XP SP1+
		if( pfnSetDllDirectory )
		{
			pfnSetDllDirectory( _T("") );
		}

		FreeLibrary( hModuleKernel32 );
	}

	SetCurrentDirectory( szCurDir );
}

typedef DWORD (WINAPI *GetProcessImageFileName_t)(HANDLE,LPTSTR,DWORD);
typedef DWORD (WINAPI *GetLogicalDriveStrings_t)(DWORD,LPTSTR);
typedef DWORD (WINAPI *QueryDosDevice_t)(LPCTSTR,LPTSTR,DWORD);
typedef BOOL (WINAPI *GetTokenInformation_t)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);
typedef BOOL (WINAPI *LookupPrivilegeValue_t)(LPCTSTR,LPCTSTR,PLUID);
typedef BOOL (WINAPI *AdjustTokenPrivileges_t)(HANDLE,BOOL,PTOKEN_PRIVILEGES,
																	 DWORD,PTOKEN_PRIVILEGES,PDWORD);
typedef BOOL (WINAPI *OpenThreadToken_t)(HANDLE,DWORD,BOOL,PHANDLE);
typedef BOOL (WINAPI *ImpersonateSelf_t)(SECURITY_IMPERSONATION_LEVEL);
typedef BOOL (WINAPI *GetProcessTimes_t)(HANDLE,LPFILETIME,LPFILETIME,LPFILETIME,LPFILETIME);

     GetProcessImageFileName_t
g_pfnGetProcessImageFileName    = NULL;
     GetLogicalDriveStrings_t
g_pfnGetLogicalDriveStrings     = NULL;
     QueryDosDevice_t
g_pfnQueryDosDevice             = NULL;
     GetTokenInformation_t
g_pfnGetTokenInformation        = NULL;
     LookupPrivilegeValue_t
g_pfnLookupPrivilegeValue       = NULL;
     AdjustTokenPrivileges_t
g_pfnAdjustTokenPrivileges      = NULL;
     OpenThreadToken_t
g_pfnOpenThreadToken            = NULL;
     ImpersonateSelf_t
g_pfnImpersonateSelf            = NULL;
     GetProcessTimes_t
g_pfnGetProcessTimes            = NULL;

static HMODULE s_hModulePsapi = NULL;
static HMODULE s_hModuleKernel32 = NULL;
static HMODULE s_hModuleAdvapi32 = NULL;
static void LoadFunctionPtrs( void )
{
	// Quick-fix to prevent DLL preloading attacks
	TCHAR szCurDir[ CCHPATH ] = _T("");
	GetCurrentDirectory( CCHPATH, szCurDir );
	TCHAR szSysDir[ CCHPATH ] = _T("");
	GetSystemDirectory( szSysDir, CCHPATH );
	SetCurrentDirectory( szSysDir );

	// Load dynamically; otherwise probably BES wouldn't run on Win2000
	s_hModulePsapi = LoadLibrary( _T("Psapi.dll") );
	if( s_hModulePsapi )
	{
		g_pfnGetProcessImageFileName = (GetProcessImageFileName_t)(void*)
			GetProcAddress( s_hModulePsapi, "GetProcessImageFileName" WorA_ );
	}

	s_hModuleKernel32 = LoadLibrary( _T("Kernel32.dll") );
	if( s_hModuleKernel32 )
	{
		g_pfnGetLogicalDriveStrings = (GetLogicalDriveStrings_t)(void*)
			GetProcAddress( s_hModuleKernel32, "GetLogicalDriveStrings" WorA_ );
		g_pfnQueryDosDevice = (QueryDosDevice_t)(void*)
			GetProcAddress( s_hModuleKernel32, "QueryDosDevice" WorA_ );
		g_pfnGetProcessTimes = (GetProcessTimes_t)(void*)
			GetProcAddress( s_hModuleKernel32, "GetProcessTimes" );
	}

	s_hModuleAdvapi32 = LoadLibrary( _T("Advapi32.dll") );
	if( s_hModuleAdvapi32 )
	{
		g_pfnGetTokenInformation = (GetTokenInformation_t) (void*)
			GetProcAddress( s_hModuleAdvapi32, "GetTokenInformation" );

		g_pfnLookupPrivilegeValue = (LookupPrivilegeValue_t)(void*)
			GetProcAddress( s_hModuleAdvapi32, "LookupPrivilegeValue" WorA_ );

		g_pfnAdjustTokenPrivileges = (AdjustTokenPrivileges_t)(void*)
			GetProcAddress( s_hModuleAdvapi32, "AdjustTokenPrivileges" );

		g_pfnOpenThreadToken = (OpenThreadToken_t)(void*)
			GetProcAddress( s_hModuleAdvapi32, "OpenThreadToken" );

		g_pfnImpersonateSelf = (ImpersonateSelf_t)(void*)
			GetProcAddress( s_hModuleAdvapi32, "ImpersonateSelf" );
	}

	SetCurrentDirectory( szCurDir );
#if 0
	if(
	g_pfnGetProcessImageFileName &&
	g_pfnGetLogicalDriveStrings     &&
	g_pfnQueryDosDevice              &&
	g_pfnGetTokenInformation         &&
	g_pfnLookupPrivilegeValue        &&
	g_pfnAdjustTokenPrivileges       &&
	g_pfnOpenThreadToken             &&
	g_pfnImpersonateSelf             &&
	g_pfnGetProcessTimes             ) OutputDebugString(_T("\tALLOK^_^\n\n\n"));
#endif
}
static void UnloadFunctionPtrs( void )
{
	g_pfnGetLogicalDriveStrings = NULL;
	g_pfnQueryDosDevice = NULL;
	g_pfnGetProcessImageFileName = NULL;
	g_pfnGetTokenInformation = NULL;

	if( s_hModuleAdvapi32 )
	{
		FreeLibrary( s_hModuleAdvapi32 );
		s_hModuleAdvapi32 = NULL;
	}

	if( s_hModuleKernel32 )
	{
		FreeLibrary( s_hModuleKernel32 );
		s_hModuleKernel32 = NULL;
	}

	if( s_hModulePsapi )
	{
		FreeLibrary( s_hModulePsapi );
		s_hModulePsapi = NULL;
	}
}

#include <Sddl.h>
void tokendebug( HANDLE hProcess )
{
	(void)hProcess;
#if 0
	HANDLE htkProcess = NULL;

	TCHAR tmpstr[ 256 ] = _T("");
	SetLastError(0);
	if( OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &htkProcess ) )
	{
		OutputDebugString(_T("#OpenProcessToken ok\n"));
		DWORD dwLen = 0;
		GetTokenInformation( htkProcess, TokenPrivileges, NULL, 0, &dwLen );
		TOKEN_PRIVILEGES * lpTokenPri = (TOKEN_PRIVILEGES*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen );
		if( lpTokenPri )
		{
			DWORD dwActLen;
			GetTokenInformation( htkProcess, TokenPrivileges, lpTokenPri, dwLen, &dwActLen );
			for( DWORD i = 0; i < lpTokenPri->PrivilegeCount; ++i )
			{
				TCHAR name[128];
				DWORD cch = 128;
				LookupPrivilegeName( NULL, &(lpTokenPri->Privileges[ i ].Luid), name, &cch );
				_stprintf_s( tmpstr, _countof(tmpstr), _T("INFO: %s is %lu"), name, lpTokenPri->Privileges[ i ].Attributes );
				WriteDebugLog( tmpstr );
			}

			MemFree( lpTokenPri );
		}
		CloseHandle( htkProcess );
		htkProcess = NULL;
	}
	else
	{
		_stprintf_s( tmpstr, _countof(tmpstr), _T("#OpenProcessToken failed, Error Code %lu"), GetLastError() );
		WriteDebugLog( tmpstr );
	}

	if( hProcess == NULL )
		return;

	htkProcess = NULL;
	if( OpenProcessToken( hProcess, TOKEN_QUERY, &htkProcess ) )
	{
		OutputDebugString(_T("#OpenProcessToken ok\n"));
		DWORD dwLen = 0;
		GetTokenInformation( htkProcess, TokenPrivileges, NULL, 0, &dwLen );
		SID_AND_ATTRIBUTES * lpSaa = (SID_AND_ATTRIBUTES*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, dwLen );
		if( lpSaa )
		{
			DWORD dwActLen;
			TCHAR str[ 1024 ];
			SetLastError( 0 );
			BOOL b = GetTokenInformation( htkProcess, TokenUser, lpSaa, dwLen, &dwActLen );
			_stprintf_s( str, 1024, _T("GetTokenInformation: b=%d e=%lu"), b, GetLastError() );
			WriteDebugLog( str );

			LPTSTR StringSid = NULL;
			ConvertSidToStringSid( lpSaa->Sid, &StringSid );
			WriteDebugLog( _T("StringSid:") );
			WriteDebugLog( StringSid );
			if( StringSid )
			{
				LocalFree( StringSid );
				StringSid = NULL;
			}

			SID_NAME_USE name_use;
			TCHAR Name[ 256 ] = _T("");
			DWORD cchName = 256;
			TCHAR RefDomName[ 256 ] = _T("");
			DWORD cchRefDomName = 256;
			LookupAccountSid( NULL, lpSaa->Sid, Name, &cchName, RefDomName, &cchRefDomName, &name_use );

			WriteDebugLog( _T("Name:") );
			WriteDebugLog( Name );
			WriteDebugLog( _T("RefDomName:") );
			WriteDebugLog( RefDomName );
		
			MemFree( lpSaa );
		}

		CloseHandle( htkProcess );
		htkProcess = NULL;
	}
#endif
}
static DWORD _bes_init( int& nCmdShow )
{
	LoadFunctionPtrs();

	BOOL bAllowMulti = FALSE;
	ReadIni( bAllowMulti, g_bLogging );
	if( g_bLogging )
	{
		OpenDebugLog();
		tokendebug( NULL );
	}

	TCHAR strOptions[ CCHPATH ] = _T("");
	TCHAR szTargetPath[ CCHPATH ] = _T("");
	const int iSlider = ParseArgs( GetCommandLine(), CCHPATH, szTargetPath, NULL, strOptions, false );

	ptrdiff_t numOfPIDs = 0;

	if( iSlider != IGNORE_ARGV && 2 <= _tcslen( szTargetPath ) )
	{
		PATHINFO pathInfo;
		ZeroMemory( &pathInfo, sizeof(PATHINFO) );
		pathInfo.pszPath = szTargetPath;
		const TCHAR * pStar = _tcschr( szTargetPath, _T('*') );
		if( pStar )
		{
			pathInfo.cchHead = (size_t)( pStar - szTargetPath );
			pathInfo.cchTail = _tcslen( pStar + 1 );
		}
		pathInfo.cchPath = _tcslen( szTargetPath );

		ptrdiff_t numOfPairs = 0;
		ptrdiff_t px = 0;
		DWORD * lpPIDs = PathToProcessIDs( pathInfo, numOfPIDs );
		if( numOfPIDs )
		{
			// Don't use the cached ver.  The cleaner is not running yet.
			PROCESS_THREAD_PAIR * sorted_pairs = AllocSortedPairs( numOfPairs, 0 );
			if( sorted_pairs )
			{
				for( ; px < numOfPIDs; ++px )
				{
					if( IsProcessBES( lpPIDs[ px ], sorted_pairs, numOfPairs ) ) break;
				}
				MemFree( sorted_pairs );
			}
		}

		if( lpPIDs ) MemFree( lpPIDs );

		if( px < numOfPIDs )
		{
			MessageBox( NULL, _T("BES Can't Target BES"), APP_NAME, MB_OK | MB_ICONEXCLAMATION | MB_TOPMOST );
			return 0;
		}
	}

	if( IsOptionSet( strOptions, _T("--help"), _T("-h") )
		|| IsOptionSet( strOptions, _T("/?"), NULL ) )
	{
#if 0
		const TCHAR str[]= APP_LONGNAME _T("\n\n");
		
		HANDLE hStd = GetStdHandle(STD_OUTPUT_HANDLE);
		AttachConsole( ATTACH_PARENT_PROCESS );
		
		DWORD cb;
		WriteConsole( hStd, str,_countof(str),&cb,NULL);
		FreeConsole();

		CloseHandle( hStd );
#endif
		ShowCommandLineHelp( NULL );
		return 0;
	}

	if( IsOptionSet( strOptions, _T("--version"), NULL ) )
	{
		MessageBox( NULL, APP_LONGNAME, APP_NAME, MB_OK | MB_ICONINFORMATION | MB_TOPMOST );
		return 0;
	}

	const BOOL bEmptyCmd = ( iSlider == IGNORE_ARGV );
	const BOOL bExitNow = ! bEmptyCmd
							&& ( IsOptionSet( strOptions, _T("--exitnow"), _T("-e") ) || IsOptionSet( strOptions, _T("-x"), NULL ) );
	const BOOL bMinimize = ! bEmptyCmd && ! bExitNow && IsOptionSet( strOptions, _T("--minimize"), _T("-m") );
	const BOOL bUnlimit = ! bEmptyCmd && ! bExitNow && IsOptionSet( strOptions, _T("--unlimit"), _T("-u") );

	if( IsOptionSet( strOptions, _T("--allow-multi"), NULL ) )
		bAllowMulti = TRUE;

	if( IsOptionSet( strOptions, _T("--disallow-multi"), NULL )
		|| IsOptionSet( strOptions, _T("--add"), _T("-a") )
		|| IsOptionSet( strOptions, _T("--job-list"), _T("-J") )
	)
	{
		bAllowMulti = FALSE;
	}

	if( bMinimize )
	{
		nCmdShow = SW_SHOWMINNOACTIVE;
	}

	BOOL bReshow = FALSE;
	if( IsOptionSet( strOptions, _T("--hide"), NULL ) )
	{
		g_fHide = true;
		nCmdShow = SW_HIDE;
	}
	else bReshow = IsOptionSet( strOptions, _T("--reshow"), NULL );
	
	if( IsOptionSet( strOptions, _T("--delay"), _T("-D") ) )
	{
		Sleep( 1000 );
	}

	DWORD_PTR Handled = FALSE;
	HWND hwndToShow = NULL;
	BES_COPYDATA mydata = {0};

	// deny multiple instances?
	//if( ! bAllowMulti || bExitNow )
	
	if( ! IsOptionSet( strOptions, _T("--selfish"), NULL ) )
	{
		HWND hwndFound = NULL;
		memset( &mydata, 0, sizeof(mydata) );
#ifdef _UNICODE
		wcsncpy_s( mydata.wzCommand, CCH_MAX_CMDLINE, GetCommandLineW(), _TRUNCATE );
#else
		// mydata.wzCommand can store 2*cchMax CHARs; use one half only
		strncpy_s( (char*) mydata.wzCommand, CCH_MAX_CMDLINE, 
					GetCommandLineA(), _TRUNCATE );
		mydata.uFlags |= BESCDF_ANSI;
#endif

#if 1//def _DEBUG
		mydata.dwParam = GetCurrentProcessId();
#endif
		COPYDATASTRUCT cd;
		cd.dwData = BES_COPYDATA_ID;
		cd.cbData = (DWORD) sizeof(mydata);
		cd.lpData = &mydata;

		HWND hwndList[ 32 ];
		memset( hwndList, 0, sizeof(hwndList) );

		// safety prevents an infinite loop, possibly caused by FindWindowEx if the window order
		// is changed before this for-loop is broken.
		for( ptrdiff_t idxFound = 0; 
			( hwndFound = FindWindowEx( NULL, hwndFound, APP_CLASS, NULL ) ) != NULL && idxFound < 32;
			++idxFound )
		{
			if( bEmptyCmd )
			{
				hwndToShow = hwndFound;
				break;
			}

			ptrdiff_t k = 0;
			for( ; k < idxFound && hwndList[ k ] != hwndFound; ++k )
				;
			
			// ignore an already-seen window, if any
			if( k < idxFound ) continue;

			hwndList[ idxFound ] = hwndFound;
			
			if( bReshow && hwndFound )
			{
				ShowWindow( hwndFound, SW_SHOWDEFAULT );
			}
			
			if( bMinimize && ! bExitNow && ! g_fHide )
			{
				if( IsWindowVisible( hwndFound ) )
				{
					//ShowWindow( hwndFound, SW_MINIMIZE );
					ShowWindow( hwndFound, SW_SHOWMINNOACTIVE ); //changed@20120425
				}
			}
			else hwndToShow = hwndFound;

			if( bExitNow )
				mydata.uCommand = BES_COPYDATA_COMMAND_EXITNOW;
			else
				mydata.uCommand = BES_COPYDATA_COMMAND_PARSE;
	
#ifdef _DEBUG
			TCHAR str[ 1024 ] = _T("");
			_stprintf_s( str, 1024, _T( "BES PID 0x%08lx: Sending message to %p" ), 
						GetCurrentProcessId(), hwndFound );
			WriteDebugLog( str );
#endif
			Handled = FALSE;
#ifdef _DEBUG
			LRESULT lres =
#endif
				SendMessageTimeout( hwndFound,
									WM_COPYDATA,
									(WPARAM)(HWND) NULL,
									(LPARAM) &cd,
									SMTO_NORMAL | SMTO_ABORTIFHUNG,
									15 * 1000,
									&Handled );
			
			mydata.uFlags |= BESCDF_SENT;
	
#ifdef _DEBUG
			_stprintf_s( str, 1024, _T( "BES PID 0x%08lx: Result=%ld Handled=%d (from %p)" ), 
						GetCurrentProcessId(), (long) lres, (BOOL) Handled, hwndFound );
			WriteDebugLog( str );
#endif
			if( Handled || ! bAllowMulti && ! bUnlimit && ! bExitNow )
				break;
		}
	}

	if( bExitNow || bUnlimit && ( mydata.uFlags & BESCDF_SENT )
		|| Handled || hwndToShow && ! bAllowMulti )
	{
		if( hwndToShow && ! bMinimize && ! bExitNow && ! bAllowMulti && ! g_fHide )
		{
			// Workaround to make AutoHotkey happy (another BES is now tray-ed)
			ShowWindow( hwndToShow, SW_HIDE );
		
			ShowWindow( hwndToShow, SW_RESTORE );
			
			BringWindowToTop( hwndToShow );
			SetForegroundWindow( hwndToShow );
			SetActiveWindow( hwndToShow );
			SetFocus( hwndToShow );
		}

		return 0UL;
	}

	INITCOMMONCONTROLSEX iccex; 
	iccex.dwICC = ICC_WIN95_CLASSES;
	iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
	InitCommonControlsEx( &iccex );


	SetPriorityClass( GetCurrentProcess(), 
						(DWORD)( g_fRealTime ? REALTIME_PRIORITY_CLASS : HIGH_PRIORITY_CLASS ) );
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );

	InitSWIni();

	hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD, &KeyboardHookProc, NULL, GetCurrentThreadId() );

	DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	if( ! bMinimize && ! g_fHide )
		dwStyle |= WS_VISIBLE;
	
	return dwStyle;
}

static void _bes_uninit( void )
{
	if( hKeyboardHook )
	{
		UnhookWindowsHookEx( hKeyboardHook );
		hKeyboardHook = NULL;
	}

	WriteIni( g_fRealTime ); // this frees pointers like g_lpszEnemy

	CloseDebugLog();

	g_hWnd = NULL;
	UnloadFunctionPtrs();
}

static HWND _create_window( HINSTANCE hInstance, DWORD dwStyle )
{
	HWND hWnd = NULL;

	WNDCLASSEX wcex = {0};
	wcex.cbSize = sizeof( WNDCLASSEX ); 
	wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.lpfnWndProc	= &WndProc;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon( hInstance, MAKEINTRESOURCE( IDI_IDLE ) );
	wcex.hIconSm		= wcex.hIcon;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= MAKEINTRESOURCE( IDC_BATTLEENC );
	wcex.lpszClassName	= APP_CLASS;

	if( RegisterClassEx( &wcex ) )
	{
		POINT pt;
		RECT rcWin;
		GetWindowPosIni( &pt, &rcWin );

		hWnd = CreateWindow( APP_CLASS, TEXT(""), dwStyle,
							(int) pt.x,
							(int) pt.y,
							(int)( rcWin.right - rcWin.left ),
							(int)( rcWin.bottom - rcWin.top ),
							NULL, NULL, hInstance, NULL );
	}

	return hWnd;
}


#define FIRST_PRESSED_CONTEXT_0( lParam ) ( ! ( ((ULONG_PTR)lParam) & 0xE0000000UL ) )
#define BEING_PRESSED( lParam )  ( ! ( ((ULONG_PTR)(lParam)) & 0x80000000 ) )
#define BEING_RELEASED( lParam ) (     ((ULONG_PTR)(lParam)) & 0x80000000   )
#define FIRST_PRESSED( lParam )           ( ! ( ((ULONG_PTR)lParam) & 0xC0000000 ) )
#define KEY_DOWN(vk) ( (int) GetKeyState( (vk) ) < 0 )
static LRESULT CALLBACK KeyboardHookProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	if( nCode < 0 )
	{
		return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
	}
#define IsMenuMode(lp) ((lp) & 0x10000000L)
	else if( FIRST_PRESSED( lParam ) && ! IsMenuMode( lParam ) )
	{
		if( wParam == _T('B') || wParam == _T('M') ) // boss-key
		{
			HWND hwndFocus = GetFocus();
			if( hwndFocus && hwndFocus != g_hWnd )
				hwndFocus = GetParent( hwndFocus );

			if( hwndFocus
				&& ( hwndFocus == g_hWnd || hwndFocus == g_hListDlg || hwndFocus == g_hSettingsDlg ) )
			{
				/*
				// SendMessage may time out if the dialogbox is there but is about to be destroyed.
				if( g_hListDlg )
					SendMessageTimeout( g_hListDlg, WM_COMMAND, IDCANCEL, 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
			
				if( g_hSettingsDlg )
					SendMessageTimeout( g_hSettingsDlg, WM_COMMAND, IDCANCEL, 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
			
				ShowWindow( g_hWnd, SW_SHOWMINIMIZED );*/
				SendMessage( g_hWnd, WM_COMMAND, IDM_MINIMIZE, 0 );
				return 1;
			}
		}

		if( ! g_hListDlg )
		{
			return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
		}

		HWND hFocus = GetFocus();

		if( hFocus && hFocus != g_hListDlg )
			hFocus = GetParent( hFocus );

		if( hFocus != g_hListDlg )
		{
			return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
		}

		if( wParam == VK_F5 || wParam == _T('R') )
		{
			SendMessageTimeout( g_hListDlg, WM_COMMAND, MAKEWPARAM( IDC_RELOAD, BN_CLICKED ), 
									(LPARAM) GetDlgItem( g_hListDlg, IDC_RELOAD ),
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 3000, NULL );
			return 1;
		}

		if( KEY_DOWN(VK_CONTROL) )
		{
			if( _T('1') <= wParam && wParam <= _T('7') )
			{
#define URF_SORT_ALGO      0x0004
				SendMessageTimeout( g_hListDlg, WM_USER_REFRESH,
										(WPARAM) -1,
										MAKELPARAM( URF_SORT_ALGO, wParam - _T('1') ),
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 3000, NULL );
				return 1;
			}
			return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
		}

		int wId = -1;
		if( wParam == _T('A') ) wId = IDC_LISTALL_SYS;
		//else if( wParam == _T('B') ) wId = -1; // Boss key
		else if( wParam == _T('C') ) wId = IDCANCEL;
		else if( wParam == _T('E') ) wId = IDC_AUTOREFRESH;
		else if( wParam == _T('F') ) wId = IDC_FRIEND;
		else if( wParam == _T('H') ) wId = IDC_HIDE;
		else if( wParam == _T('I') ) wId = IDC_SLIDER_BUTTON;
		else if( wParam == _T('K') ) wId = IDC_SHOW_MANUALLY;
		else if( wParam == _T('L') ) wId = IDOK;
		else if( wParam == _T('N') ) wId = IDC_RESET_IFF;
		else if( wParam == _T('O') ) wId = IDC_FOE;
		else if( wParam == _T('R') ) wId = IDC_RELOAD;
		else if( wParam == _T('S') ) wId = IDC_SHOW;
		else if( wParam == _T('U') ) wId = IDC_UNLIMIT_ALL;
		else if( wParam == _T('W') ) wId = IDC_WATCH;
		else if( wParam == _T('Z') ) wId = IDC_UNFREEZE;
		else if( wParam >= _T('A') && wParam <= _T('Z') ) wId = -1;
		else return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );

		if( wId != -1 )
		{
			HWND hwndCtrl = GetDlgItem( g_hListDlg, wId );
			if( IsWindowEnabled( hwndCtrl ) )
			{
				DWORD_PTR result;

				if( wId == IDC_AUTOREFRESH )
				{
					SendMessageTimeout( hwndCtrl, BM_GETCHECK,
										(WPARAM) 0, (LPARAM) 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, &result );
					
					SendMessageTimeout( hwndCtrl, BM_SETCHECK,
										(WPARAM) ! result, (LPARAM) 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
				}

				// SetDlgItemFocusNow
				if( wId != IDOK
					&& wId != IDC_WATCH
					&& wId != IDC_SLIDER_BUTTON
					&& wId != IDC_HIDE && wId != IDC_SHOW )
				{
					SendMessageTimeout( g_hListDlg, WM_NEXTDLGCTL,
										(WPARAM) hwndCtrl, MAKELPARAM( TRUE, 0 ),
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
				}

				SendMessageTimeout( g_hListDlg, WM_COMMAND,
									MAKEWPARAM( wId, BN_CLICKED ), (LPARAM) hwndCtrl,
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
			}
		}
		return 1;
	}

	return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}
