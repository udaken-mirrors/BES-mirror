#include "BattleEnc.h"

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hListDlg;
extern HFONT g_hFont;
extern volatile DWORD g_dwTargetProcessId[ 4 ];
extern volatile BOOL g_bHack[ 4 ];
extern HANDLE g_hMutexDLL;

BOOL g_bSelNextOnHide = FALSE;

TCHAR g_lpszEnemy[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
int g_numOfEnemies = 0;

TCHAR g_lpszFriend[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
int g_numOfFriends = 0;

void WriteIni_Time( const TCHAR * pExe );

static PROCESS_THREAD_PAIR * CacheProcessThreads_Alloc( ptrdiff_t * num, ptrdiff_t * maxdup );

static INT_PTR CALLBACK Question( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

static BOOL SetProcessInfoMsg( HWND hDlg, const TARGETINFO * lpTarget );

static BOOL ProcessToPath( HANDLE hProcess, TCHAR * szPath, DWORD cchPath );
static void LoadFunctions( void );
static void UnloadFunctions( void );
static bool IsAbsFoe2( LPCTSTR strExe, LPCTSTR strPath );

#define MAX_WINDOWS_CNT (1024)
#define CCH_ITEM_MAX    (128)
typedef struct tagWinInfo {
	TCHAR szTitle[ MAX_WINDOWS_CNT ][ MAX_WINDOWTEXT ];
	HWND hwnd[ MAX_WINDOWS_CNT ];
	ptrdiff_t numOfItems;
	INT_PTR dummy;
} WININFO;
static BOOL CALLBACK WndEnumProc( HWND hwnd, LPARAM lParam )
{
	if( ! lParam )
		return FALSE;

	WININFO& winfo = *(WININFO *) lParam;
	
	winfo.hwnd[ winfo.numOfItems ] = hwnd;

	*winfo.szTitle[ winfo.numOfItems ] = _T('\0');
	GetWindowText( hwnd, winfo.szTitle[ winfo.numOfItems ], MAX_WINDOWTEXT );

//	winfo.dwThreadId[ winfo.numOfItems ] = GetWindowThreadProcessId( hwnd, NULL );

	return ( ++winfo.numOfItems < MAX_WINDOWS_CNT );
}


BOOL BES_ShowWindow( HWND hCurWnd, HWND hwnd, int iShow )
{
	int nCmdShow;
	if( iShow == BES_SHOW_MANUALLY )
	{
		TCHAR lpWindowText[ MAX_WINDOWTEXT ];
		TCHAR msg[ MAX_WINDOWTEXT + 256 ];
		GetWindowText( hwnd, lpWindowText, MAX_WINDOWTEXT );
		if( ! *lpWindowText ) _tcscpy_s( lpWindowText, MAX_WINDOWTEXT, _T( "n/a" ) );
		_stprintf_s( msg, _countof(msg),
			_T( "Show this window?\r\n\r\nhWnd : 0x%p\r\n\r\nWindow Text: %s" ),
			hwnd, lpWindowText
		);
		int iResponse =	MessageBox( hCurWnd,
			msg,
			APP_NAME, 
			MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON2
		);
		if( iResponse == IDNO ) return TRUE;
		else if( iResponse == IDCANCEL ) return FALSE;
			
		ShowWindow( hwnd, SW_SHOWDEFAULT );
		BringWindowToTop( hwnd );
		RECT rect;
		GetWindowRect( hwnd, &rect );
		RECT area;
		SystemParametersInfo( SPI_GETWORKAREA, 0, &area, 0 );
		MoveWindow( hwnd, (int) area.left, (int) area.top, rect.right - rect.left, rect.bottom - rect.top, TRUE );
		return TRUE;
	}
	else if( iShow == BES_SHOW )
	{
		nCmdShow = GetCmdShow( hwnd );
		
		if( IsWindowVisible( hwnd ) || nCmdShow == BES_ERROR )
		{
			return TRUE;
		}

		ShowWindow( hwnd, nCmdShow );
		//if( IsIconic( hwnd ) ) ShowWindow( hwnd, SW_RESTORE );
		if( ! IsIconic( hwnd ) ) BringWindowToTop( hwnd );
		SaveCmdShow( hwnd, BES_DELETE_KEY );
		SetForegroundWindow( hCurWnd );
		return TRUE;
	}
	else if( iShow == BES_HIDE )
	{
		if( ! IsWindowVisible( hwnd ) )
		{
			return TRUE;
		}
		else if( IsIconic( hwnd ) )
		{
			nCmdShow = /*SW_SHOWMINIMIZED*/ SW_SHOWMINNOACTIVE;
		}
		else if( IsZoomed( hwnd ) )
		{
			nCmdShow = SW_SHOWMAXIMIZED;
		}
		else
		{
			nCmdShow = SW_SHOW;
		}
		SaveCmdShow( hwnd, nCmdShow );
		ShowWindow( hwnd, SW_HIDE );
		return TRUE;
	}

	return TRUE;
}


static bool show_process_win_worker( 
	HWND hDlg, const DWORD * pTIDs, ptrdiff_t numOfThreads, WININFO * pWinInfo, int iShow )
{
	bool ret = false;
	for( ptrdiff_t i = 0; i < numOfThreads; ++i )
	{
		pWinInfo->numOfItems = 0;
		EnumThreadWindows( pTIDs[ i ], &WndEnumProc, (LPARAM) pWinInfo );

		if( ! ret ) // true if at least one window is found
			ret = !! pWinInfo->numOfItems;

		ptrdiff_t j = 0;
		for( ; j < pWinInfo->numOfItems; ++j )
		{
			if( ! BES_ShowWindow( hDlg, pWinInfo->hwnd[ j ], iShow ) )
				return true;
		}
	}

	return  ret;
}

static void ShowProcessWindow( HWND hDlg, DWORD dwProcessID, int iShow )
{
	WININFO * lpWinInfo = (WININFO*) HeapAlloc( GetProcessHeap(), 0L, sizeof(WININFO) );
	if( lpWinInfo )
	{
		ptrdiff_t numOfThreads = 0;
		DWORD * pTIDs = ListProcessThreads_Alloc( dwProcessID, &numOfThreads );
		if( pTIDs )
		{
			if( ! show_process_win_worker( hDlg, pTIDs, numOfThreads, lpWinInfo, iShow ) )
				MessageBox( hDlg, _T("No windows!"), APP_NAME, MB_OK | MB_ICONINFORMATION );

			HeapFree( GetProcessHeap(), 0L, pTIDs );
		}

		HeapFree( GetProcessHeap(), 0L, lpWinInfo );
	}
}

static void get_process_details_worker(
	const DWORD * pTID, ptrdiff_t numOfThreads, TCHAR * pWindowText, WININFO * lpWinInfo )
{
	size_t uTitleImportance0 = 0;

	*pWindowText = _T('\0');

	for( ptrdiff_t i = 0; i < numOfThreads; ++i )
	{
		lpWinInfo->numOfItems = 0;
		EnumThreadWindows( pTID[ i ], &WndEnumProc, (LPARAM) lpWinInfo );

		ptrdiff_t copyme = -1;
		for( ptrdiff_t j = 0; j < (ptrdiff_t) lpWinInfo->numOfItems; ++j )
		{
			if( StrEqualNoCase( lpWinInfo->szTitle[ j ], _T("Default IME") ) )
				continue;

			size_t uTitleImportance = _tcslen( lpWinInfo->szTitle[ j ] );
			if( IsWindowVisible( lpWinInfo->hwnd[ j ] ) ) uTitleImportance *= 128;
			else if( IsIconic( lpWinInfo->hwnd[ j ] ) ) uTitleImportance *= 64;
			if( uTitleImportance > uTitleImportance0 )
			{
				copyme = j;
				uTitleImportance0 = uTitleImportance;
			}
		}
		
		if( 0 <= copyme )
			_tcscpy_s( pWindowText, MAX_WINDOWTEXT, lpWinInfo->szTitle[ copyme ] );
	}
	
	if( ! pWindowText[ 0 ] )
		_tcscpy_s( pWindowText, MAX_WINDOWTEXT, _T( "<no text>" ) );
}

static void GetProcessDetails2( 
	DWORD dwProcessId,
	LPTARGETINFO lpTargetInfo,
	WININFO * lpWinInfo, // caller-providing buffer
	const PROCESS_THREAD_PAIR * pairs,
	ptrdiff_t numOfPairs,
	ptrdiff_t maxdup )
{
	if( ! maxdup ) maxdup = 256;
	
	DWORD * pTIDs = (DWORD*) HeapAlloc( GetProcessHeap(), 0L, sizeof(DWORD) * (size_t) maxdup );

	if( ! pTIDs )
		return;
	
	ptrdiff_t numOfThreads = 0;
	if( pairs )
	{
		for( ptrdiff_t i = 0; i < numOfPairs && numOfThreads < maxdup; ++i )
		{
			if( pairs[ i ].pid == dwProcessId )
				pTIDs[ numOfThreads++ ] = pairs[ i ].tid;
			else if( dwProcessId < pairs[ i ].pid )
				break;
		}
		//TCHAR s[100];swprintf_s(s,100,L"%d/%d\n",numOfThreads,maxdup);
		//OutputDebugString(s);
	}
	else
	{
		numOfThreads = ListProcessThreads( dwProcessId, pTIDs, maxdup );
	}

	get_process_details_worker( pTIDs, numOfThreads, lpTargetInfo->szText, lpWinInfo );
	HeapFree( GetProcessHeap(), 0L, pTIDs );
}


BOOL PathToExe( LPCTSTR strPath, LPTSTR lpszExe, int iBufferSize )
{
	if( ! strPath || ! lpszExe )
		return FALSE;

	if( (int) _tcslen( strPath ) >= iBufferSize )
		return FALSE;

	_tcscpy_s( lpszExe, (rsize_t) iBufferSize, strPath );
	return PathToExe( lpszExe );
}

BOOL PathToExe( LPTSTR strPath )
{
	if( ! strPath )
		return FALSE;
	/*
	C:\path\to\some.exeZ
	1234567890123456789  : len = 19
	012345678901234567*  : strPath[ i ] for i = len - 1 = 18
	0123456789*          : strPath[ 10 ] == _T('\\') --> i = 10
	           123456789 : cchToCopy = len - i  = 19 - 10 = 9 (incl. term NUL)
	
	
	*/

  	/*int len = (int) _tcslen( strPath );
	for( int i = len - 1; i >= 0; i-- )
	{
		if( strPath[ i ] == TEXT( '\\' ) )
		{
			//lstrcpy( strPath, &strPath[ i + 1 ] ); // @fix 20111207
			memmove( strPath, &strPath[ i + 1 ], (size_t)( len - i ) * sizeof(TCHAR) );
			break;
		}
	}*/
	//OutputDebugStringW(L"\r\n[in] ");
	//OutputDebugStringW(strPath);
	const TCHAR * pntr = _tcsrchr( strPath, _T('\\') );
	if( pntr )
		memmove( strPath, pntr + 1, ( _tcslen( pntr + 1 ) + 1 ) * sizeof(TCHAR) );

	//OutputDebugStringW(L"\r\n[out] ");
	//OutputDebugStringW(strPath);
	//OutputDebugStringW(L"\r\n");
	return TRUE;
}


BOOL PathToExeEx( LPTSTR strPath, int iBufferSize )
{
	if( ! PathToExe( strPath ) )
		return FALSE;
	int len = (int) _tcslen( strPath );
	if( iBufferSize <= len + 8 ) return FALSE;

	if(
		(int) _tcslen( strPath ) < 15 ||
		Lstrcmpi( &strPath[ len - 4 ], _T( ".exe" ) ) == 0
	)
	{
		return TRUE;
	}
/*
c:\path\something.exZ
12345678901234567890 len = 20
01234567890123456*   len-3 = 17 --> used 17 cch not incl '.'
*/
		/*//lstrcpy( &strPath[ len - 3 ], TEXT( ".exe" ) );
		_tcscpy_s( &strPath[ len - 3 ], (rsize_t)( iBufferSize - ( len - 3 ) ), _T(".exe") );
		// If case-insensitive, strPath[len]=_T('e'),strPath[len+1]=_T('\0') would enough.*/

	int s = 0;
	if(	Lstrcmpi( &strPath[ len - 3 ], _T(".ex") ) == 0 )
	{
		s = 3;
	}
	else if( strPath[ len - 2 ] == _T('.')
				&& ( strPath[ len - 1 ] == _T('e') || strPath[ len - 1 ] == _T('E') ) )
	{
		s = 2;
	}
	else if( strPath[ len - 1 ] == _T('.') )
	{
		s = 1;
	}

	if( s )
	{
		--s;
		_tcscpy_s( &strPath[ len - s ], (rsize_t)( iBufferSize - ( len - s ) ), _T("exe") );
		return TRUE;
	}

/* @1.1b5 Will no longer do this
	if
	(
		strPath[ len - 1 ] == TEXT( ' ' )
	)
	{
		strPath[ len - 1 ] = TEXT( '\0' );
	}
*/

	_tcscat_s( strPath, (rsize_t) iBufferSize, _T( "~.exe" ) );
	return TRUE;
}


DWORD PathToProcessID( const TCHAR * pPath, const DWORD * pdwExcluded, ptrdiff_t nExcluded )
{
	if( ! pPath || CCHPATH <= _tcslen( pPath ) )
	{
		return (DWORD) -1;
	}

#ifdef _UNICODE
	WCHAR szTargetLongPath[ CCHPATH ] = L"";

	DWORD dwResult1 = GetLongPathName( pPath, szTargetLongPath, CCHPATH );
	if( dwResult1 == 0UL || CCHPATH <= dwResult1 )
	{
		// Use it as it is if GetLongPathName fails
		_tcscpy_s( szTargetLongPath, _countof(szTargetLongPath), pPath );
	}
#else
	char lpszTargetExe[ CCHPATH ] = "";
	PathToExe( pPath, lpszTargetExe, CCHPATH );
	const BOOL bLongExeName = ( strlen( lpszTargetExe ) > 15 );
#endif

	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0UL );

	if( hProcessSnap == INVALID_HANDLE_VALUE )
	{
		return (DWORD) -1;
	}

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	if( ! Process32First( hProcessSnap, &pe32 ) )
	{
	    CloseHandle( hProcessSnap );
		return (DWORD) -1;
	}

	LoadFunctions();

	do
	{
		if( pdwExcluded )
		{
			ptrdiff_t j = 0;
			
			for( ; j < nExcluded; ++j )
			{
				if( pe32.th32ProcessID == pdwExcluded[ j ] )
					break;
			}
			
			if( j < nExcluded )
				continue;
		}
		
		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
										FALSE, pe32.th32ProcessID );
		if( hProcess != NULL ) 
		{
#ifdef _UNICODE
			WCHAR szPath[ MAX_PATH * 2 ] = L"";
//+ 1.1 b5 -------------------------------------------------------------------
			if( ProcessToPath( hProcess, szPath, MAX_PATH * 2 )
				&&
				Lstrcmpi( szTargetLongPath, szPath ) == 0 )
			{
				CloseHandle( hProcess );
				UnloadFunctions();
				CloseHandle( hProcessSnap );
				return pe32.th32ProcessID;
			}
// ---------------------------------------------------------------------------
#else
			if( Lstrcmpi( lpszTargetExe, pe32.szExeFile ) == 0 )
			{
				CloseHandle( hProcess );
				UnloadFunctions();
				CloseHandle( hProcessSnap );
				return pe32.th32ProcessID;
			}

			// fixed @ 1.1b5
			if( bLongExeName )
			{
				if( _strnicmp( lpszTargetExe, pe32.szExeFile, 15 ) == 0 )
				{
					CloseHandle( hProcess );
					UnloadFunctions();
					CloseHandle( hProcessSnap );
					return pe32.th32ProcessID;
				}
			}
#endif
			CloseHandle( hProcess );
		}
	} while( Process32Next( hProcessSnap, &pe32 ) );

	UnloadFunctions();
	CloseHandle( hProcessSnap );

	return (DWORD) -1;
}


typedef DWORD (WINAPI *GetProcessImageFileName_t)(HANDLE,LPTSTR,DWORD);
static GetProcessImageFileName_t g_pfnGetProcessImageFileName = NULL;
typedef DWORD (WINAPI *GetLogicalDriveStrings_t)(DWORD,LPTSTR);
static GetLogicalDriveStrings_t g_pfnGetLogicalDriveStrings = NULL;
typedef DWORD (WINAPI *QueryDosDevice_t)(LPCTSTR,LPTSTR,DWORD);
static QueryDosDevice_t g_pfnQueryDosDevice = NULL;
static HMODULE g_hModulePsapi = NULL;
static HMODULE g_hModuleKernel32 = NULL;
static int refCount = 0; // non-volatile as mutex-protected

static BOOL DevicePathToDosPath( TCHAR * strBuffer, DWORD cchBuffer )
{
	if( ! g_pfnGetLogicalDriveStrings || ! g_pfnQueryDosDevice )
		return FALSE;

//#define _MAX_DRIVE  3   /* max. length of drive component */

	TCHAR strDriveNames[ 512 ] = TEXT( "" );
	DWORD cchDriveNames =
		(*g_pfnGetLogicalDriveStrings)( (DWORD) _countof(strDriveNames), strDriveNames );
	TCHAR * strDrive = strDriveNames;
	while( strDrive < strDriveNames + cchDriveNames )
	{
		size_t cchDrive = _tcslen( strDrive );
		TCHAR * strDriveEnd = strDrive + cchDrive;
		
		if( strDrive < strDriveEnd ) // non-empty name
		{
			// kill the trailing back-slash if exists
			if( *( strDriveEnd - 1 ) == TEXT( '\\' ) )
				*( strDriveEnd - 1 ) = TEXT( '\0' );
		
			TCHAR strDevice[ 256 ] = TEXT( "" );
			if( (*g_pfnQueryDosDevice)( strDrive, strDevice, (DWORD) _countof(strDevice) ) )
			{
				size_t cchDevice = _tcslen( strDevice );
				// Add a backslash and include it in comparison;
				// Otherwise, we may be confused by, for example,
				// "HarddiskVolume1" and "HarddiskVolume10".
				if( 1 <= cchDevice
					&& cchDevice <= 254
					&& _tcscat_s( strDevice, _countof(strDevice), _T("\\") ),
						_tcsncicmp( strBuffer, strDevice, cchDevice + 1 ) == 0 )
				{
					size_t cchResult = cchDrive
										+ _tcslen( strBuffer + cchDevice );

					TCHAR strResult[ MAX_PATH * 2 ] = _T("");
					if( cchResult < MAX_PATH * 2 && cchResult < cchBuffer )
					{
						_stprintf_s( strResult, _countof(strResult),
									_T("%s%s"),
									strDrive,
									strBuffer + cchDevice );
						_tcscpy_s( strBuffer, cchBuffer, strResult );

						return TRUE;
					}

					break;
				}
			}
		}		
		
		strDrive = strDriveEnd + 1;
	}

	return FALSE;
}

static void LoadFunctions( void )
{
	if( g_hMutexDLL && WaitForSingleObject( g_hMutexDLL, 1000 ) != WAIT_OBJECT_0 )
		return;

	if( refCount++ == 0 )
	{
		// Load dynamically; otherwise probably this program wouldn't run on Win2000
		g_hModulePsapi = SafeLoadLibrary( _T("Psapi.dll") );
		if( g_hModulePsapi )
		{
			g_pfnGetProcessImageFileName = (GetProcessImageFileName_t)(void*)
											GetProcAddress( g_hModulePsapi,
															"GetProcessImageFileName" WorA_ );
		}

		g_hModuleKernel32 = SafeLoadLibrary( _T("Kernel32.dll") );
		if( g_hModuleKernel32 )
		{
			g_pfnGetLogicalDriveStrings = (GetLogicalDriveStrings_t)(void*)
											GetProcAddress( g_hModuleKernel32,
															"GetLogicalDriveStrings" WorA_ );
			g_pfnQueryDosDevice = (QueryDosDevice_t)(void*)
											GetProcAddress( g_hModuleKernel32,
															"QueryDosDevice" WorA_ );
		}
	}

	if( g_hMutexDLL )
		ReleaseMutex( g_hMutexDLL );
}


static void UnloadFunctions( void )
{
	if( g_hMutexDLL && WaitForSingleObject( g_hMutexDLL, 1000 ) != WAIT_OBJECT_0 )
		return;

	if( --refCount == 0 )
	{
		g_pfnGetLogicalDriveStrings = NULL;
		g_pfnQueryDosDevice = NULL;
		g_pfnGetProcessImageFileName = NULL;

		if( g_hModuleKernel32 )
		{
			FreeLibrary( g_hModuleKernel32 );
			g_hModuleKernel32 = NULL;
		}

		if( g_hModulePsapi )
		{
			FreeLibrary( g_hModulePsapi );
			g_hModulePsapi = NULL;
		}
	}
	
	if( g_hMutexDLL )
		ReleaseMutex( g_hMutexDLL );
}

static BOOL ProcessToPath( HANDLE hProcess, TCHAR * szPath, DWORD cchPath )
{
	if( ! szPath )
		return FALSE;

	TCHAR strFilePath[ MAX_PATH * 2 ] = _T("");

	if( GetModuleFileNameEx( hProcess, NULL, strFilePath, _countof(strFilePath) )
			&& strFilePath[ 0 ] != _T('\\')
		||
		g_pfnGetProcessImageFileName != NULL
			&& (*g_pfnGetProcessImageFileName)( hProcess, strFilePath, (DWORD) _countof(strFilePath) )
			&& DevicePathToDosPath( strFilePath, (DWORD) _countof(strFilePath) )
	)
	{
		DWORD dwResult = GetLongPathName( strFilePath, szPath, cchPath );
		// Use whatever we have now if GetLongPathName failed
		if( ! dwResult || cchPath <= dwResult )
		{
#ifndef _UNICODE
			// strFilePath doesn't exist, probably because the true path contains non-ACP characters
			// and was not converted to a usable ANSI string.
			if( GetFileAttributes( strFilePath ) == INVALID_FILE_ATTRIBUTES )
			{
				return FALSE;
			}
#endif
			if( _tcslen( strFilePath ) < cchPath )
				_tcscpy_s( szPath, cchPath, strFilePath );
			else
				return FALSE;
		}
		return TRUE;
	}

	return FALSE;
}





static int target_comp( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );

	int cmp = _tcsicmp( target1.szExe, target2.szExe );
	if( ! cmp )
		cmp = (int)( target1.dwProcessId - target2.dwProcessId );

	return cmp;
}

static int pair_comp( const void * pv1, const void * pv2 )
{
	const PROCESS_THREAD_PAIR& pair1 = *static_cast<const PROCESS_THREAD_PAIR *>( pv1 );
	const PROCESS_THREAD_PAIR& pair2 = *static_cast<const PROCESS_THREAD_PAIR *>( pv2 );

	if( pair1.pid != pair2.pid )
		return ( pair1.pid < pair2.pid ) ? -1 : +1 ;

	if( pair1.tid != pair2.tid )
		return ( pair1.tid < pair2.tid ) ? -1 : +1 ;
		
	return 0;
}


#define LIST_SYSTEM_PROCESS (2)
#define ListMore(listLevel) (LIST_SYSTEM_PROCESS<=(listLevel))
#define ListLess(listLevel) ((listLevel)<LIST_SYSTEM_PROCESS)

static ptrdiff_t UpdateProcessSnap( LPTARGETINFO& target, ptrdiff_t& maxItems, int listLevel )
{
	//** HOTFIX @ 20140306
	memset( target, 0, maxItems * sizeof(TARGETINFO) );
	// HOTFIX END**

	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0UL );
	if( hProcessSnap == INVALID_HANDLE_VALUE ) return 0;
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof( PROCESSENTRY32 );
	if( ! Process32First( hProcessSnap, &pe32 ) )
	{
	    CloseHandle( hProcessSnap );
		return 0;
	}

	WININFO * lpWinInfo = (WININFO*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WININFO) );
	if( ! lpWinInfo )
	{
	    CloseHandle( hProcessSnap );
		return 0;
	}

	LoadFunctions();
	ptrdiff_t index = 0;

	ptrdiff_t numOfPairs = 0;
	ptrdiff_t maxdup = 0;
	// pairs may be NULL
	PROCESS_THREAD_PAIR * pairs = CacheProcessThreads_Alloc( &numOfPairs, &maxdup );

#ifdef _DEBUG
	LARGE_INTEGER li0,li;
	QueryPerformanceCounter(&li0);
	TCHAR s[100];
	_stprintf_s(s,100,_T("%d pairs\n"),(int)numOfPairs);
	OutputDebugString(s);
#endif

	do
	{
		target[ index ].iIFF = IFF_UNKNOWN;

		if( pe32.th32ProcessID == 0UL ) continue;

		if( pe32.th32ProcessID == GetCurrentProcessId() && ListLess(listLevel) )
		{
			continue;
		}

		HANDLE hProcess = OpenProcess(
			PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_VM_READ,
			FALSE,
			pe32.th32ProcessID
		);

		if( hProcess == NULL )
		{
			if( ListMore( listLevel ) ) // list everything anyway
			{
				target[ index ].iIFF = IFF_SYSTEM;

				hProcess = OpenProcess(
					PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
					FALSE,
					pe32.th32ProcessID
				);
			}
			else
			{
				continue;
			}
		}

		if( hProcess != NULL )
		{
//#ifdef _UNICODE
			if( ProcessToPath( hProcess, target[ index ].szPath, MAX_PATH * 2 ) )
			{
				PathToExe( target[ index ].szPath, target[ index ].szExe, MAX_PATH * 2 );
			}
			else
			{
				_tcscpy_s( target[ index ].szExe, MAX_PATH * 2, pe32.szExeFile );
				_tcscpy_s( target[ index ].szPath, MAX_PATH * 2, pe32.szExeFile );
			}
//#endif
			CloseHandle( hProcess );

			// HOTFIX--- 20140302 Fix regression in v1.6.1
			if( _tcsicmp( target[ index ].szExe, _T("System") ) == 0 )
			{
				if( ListLess( listLevel ) ) goto DO_NOT_LIST;
					
				target[ index ].iIFF = IFF_SYSTEM;
			} //---END HOTFIX
		}
		else
		{
			_tcscpy_s( target[ index ].szExe, MAX_PATH * 2, pe32.szExeFile );
			_tcscpy_s( target[ index ].szPath, MAX_PATH * 2, pe32.szExeFile );
		}

		// pairs can be NULL
		GetProcessDetails2( pe32.th32ProcessID, &target[ index ], lpWinInfo, pairs, numOfPairs, maxdup );

		target[ index ].dwProcessId = pe32.th32ProcessID;

		// These are special
		if(
			pe32.th32ProcessID == GetCurrentProcessId()
			||
			_tcsicmp( target[ index ].szExe , _T( "BES.EXE" ) ) == 0
					&& IsProcessBES( pe32.th32ProcessID, pairs, numOfPairs )
		)
		{
			// Don't mess with them
			if( ListLess( listLevel ) ) goto DO_NOT_LIST;
				
			target[ index ].fThisIsBES = true;
			target[ index ].iIFF = IFF_SYSTEM;
		}

		// These are absolutely foes (IFF_ABS_FOE)
		// We should make sure they are in the Enemy list.
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			if( IsAbsFoe2( target[ index ].szExe, target[ index ].szPath ) ) //20111230
			{
				target[ index ].iIFF = IFF_ABS_FOE;
			}
		}

		// Detect the user-defined foes
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			for( int i = 0; i < g_numOfEnemies; ++i )
			{
				if( Lstrcmpi( target[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
				{
					target[ index ].iIFF = IFF_FOE;
					break;
				}
			}
		}

		// detect friends
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			for( int i = 0; i < g_numOfFriends; ++i )
			{
				if( Lstrcmpi( target[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
				{
					target[ index ].iIFF = IFF_FRIEND;
					break;
				}
			}
		}

		++index;

		if( index == maxItems && maxItems < 32768 ) // 32768: hardcoded abs max limit
		{
			LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 
										target, sizeof(TARGETINFO) * ( maxItems * 2 ) );
			if( lpv )
			{
				maxItems *= 2;
				target = (LPTARGETINFO) lpv;
			}
		}

		DO_NOT_LIST:
		{
			; /* do nothing */
		}
	} while( Process32Next( hProcessSnap, &pe32 ) && index < maxItems );

	if( pairs )
		HeapFree( GetProcessHeap(), 0L, pairs );

#ifdef _DEBUG
	QueryPerformanceCounter(&li);
	LARGE_INTEGER freq;
	QueryPerformanceFrequency( &freq );
	_stprintf_s(s,100,_T("cost=%.2f ms\n"),(double)(li.QuadPart-li0.QuadPart)/freq.QuadPart*1000.0);
	OutputDebugString(s);
#endif	
	
	UnloadFunctions();
	HeapFree( GetProcessHeap(), 0L, lpWinInfo );
    CloseHandle( hProcessSnap );

	qsort( target, (size_t) index, sizeof(TARGETINFO), &target_comp );
	return index;
}

BOOL SaveSnap( HWND hWnd )
{
	TCHAR strPath[ MAX_PATH * 2 ] = TEXT( "snap.txt" );
	OPENFILENAME ofn;
	ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = TEXT( "Text file (*.txt)\0*.txt\0All files (*.*)\0*.*\0\0" );
	ofn.nFilterIndex = 1L;
	ofn.lpstrFile = strPath;
	ofn.nMaxFile = MAX_PATH * 2L;
#ifdef _UNICODE
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_ENABLESIZING | OFN_DONTADDTORECENT | OFN_HIDEREADONLY;
#else
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_ENABLESIZING | OFN_HIDEREADONLY;
#endif
	ofn.lpstrDefExt = TEXT( "txt" );
	ofn.lpstrTitle = TEXT( "Save a Snapshot of the Processes" );

	if( ! GetSaveFileName( &ofn ) ) return FALSE;

	return !! SaveSnap( strPath );
}

BOOL SaveSnap( LPCTSTR lpszSavePath )
{
	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0UL );
	if( hProcessSnap == INVALID_HANDLE_VALUE ) return FALSE;

	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof( PROCESSENTRY32 );

	if( ! Process32First( hProcessSnap, &pe32 ) )
	{
	    CloseHandle( hProcessSnap );
		return FALSE;
	}
	
	FILE * fp = NULL;	
	_tfopen_s( &fp, lpszSavePath, _T( "wb" ) );
	if( fp == NULL ) return FALSE;

	LoadFunctions();

#ifdef _UNICODE
	fputwc( L'\xFEFF', fp );
#endif
	_fputts( _T( "---------------------------\r\n" ), fp );
	_fputts( _T( " Snapshot of the Processes \r\n" ), fp );
	_fputts( _T( "---------------------------\r\n\r\n" ), fp );

	PrintFileHeader( fp );

	FILETIME ft;
	GetSystemTimeAsFileTime( &ft );
	ULONGLONG rt0 = (ULONGLONG) ft.dwHighDateTime << 32 | (ULONGLONG) ft.dwLowDateTime;

	do
	{
		if( pe32.th32ProcessID == 0 ) continue; // this case could be interesting tho...

		_ftprintf( fp, _T( "[ %s ]\r\nProcess ID = %08lX\r\nThread Count = %lu\r\n" ),
			pe32.szExeFile, pe32.th32ProcessID, pe32.cntThreads
		);

		_ftprintf( fp, _T( "Base Priority = %ld\r\n" ), pe32.pcPriClassBase );

		HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
			FALSE, pe32.th32ProcessID );
		if( hProcess != NULL ) 
		{
//#ifdef _UNICODE
			TCHAR szPath[ MAX_PATH * 2 ] = _T("");

			if( ! ProcessToPath( hProcess, szPath, MAX_PATH * 2 ) )
				_tcscpy_s( szPath, _countof(szPath), _T("n/a") );

			_ftprintf( fp, _T( "Full Path = %s\r\n" ), szPath );
//#endif
			DWORD dwPriorityClass = GetPriorityClass( hProcess );
			_ftprintf( fp, _T( "Process Priority = %lu\r\n" ), dwPriorityClass );
			CloseHandle( hProcess );
		}
		else
		{
			continue;
		}

		HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0UL );

		if( hThreadSnap == INVALID_HANDLE_VALUE )
		{
			continue;
		}

		THREADENTRY32 te32;
		te32.dwSize = sizeof( THREADENTRY32 ); 

		if( ! Thread32First( hThreadSnap, &te32 ) )
		{
			CloseHandle( hThreadSnap );
			continue;
		}

		do 
		{ 
			if( te32.th32OwnerProcessID == pe32.th32ProcessID )
			{
				_ftprintf( fp, _T("  Thread %08lX : Priority %2ld\r\n"),
							te32.th32ThreadID, te32.tpBasePri
				);
			}
		} while( Thread32Next( hThreadSnap, &te32 ) ); 

		CloseHandle( hThreadSnap );
		_fputts( _T( "\r\n\r\n* * *\r\n\r\n" ), fp );

	} while( Process32Next( hProcessSnap, &pe32 ) );

	UnloadFunctions();
	CloseHandle( hProcessSnap );

	GetSystemTimeAsFileTime( &ft );
	ULONGLONG rt1 = (ULONGLONG) ft.dwHighDateTime << 32 | (ULONGLONG) ft.dwLowDateTime;
	double cost = ( rt1 - rt0 ) / 10000000.0;
	_ftprintf( fp, _T( "Cost : %.3f seconds" ), cost );
	fclose( fp );

	return TRUE;
}


#ifdef STRICT
#define WNDPROC_FARPROC WNDPROC
#else
#define WNDPROC_FARPROC FARPROC
#endif
static WNDPROC_FARPROC  DefListProc = NULL;
static LRESULT CALLBACK SubListProc( HWND hLB, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	if( uMessage == WM_RBUTTONDOWN || uMessage == WM_RBUTTONUP )
	{
		POINT pt;
		GetCursorPos( &pt );
		ScreenToClient( hLB, &pt );
		LRESULT Item = SendMessage( hLB, LB_ITEMFROMPOINT, 0, MAKELPARAM( pt.x, pt.y ) );
		
		if( uMessage == WM_RBUTTONDOWN )
		{
			if( HIWORD( Item ) == 0 )
			{
				// This scrolls the item into view if necessary
				SendMessage( hLB, LB_SETCURSEL, LOWORD( Item ), 0 );
				SendMessage( GetParent( hLB ), WM_COMMAND,
								MAKEWPARAM( IDC_TARGET_LIST, LBN_SELCHANGE ), (LPARAM) hLB );
			}
		}
		else
		{
			if( HIWORD( Item ) == 1 ) // outside (not on any item)
				return (LRESULT) 1;   // eat this to prevent WM_CONTEXTMENU
		}
	}

	return CallWindowProc( DefListProc, hLB, uMessage, wParam, lParam );
}

static BOOL AppendMenu2( HMENU hMenu, HWND hDlg, int idCtrl, LPCTSTR strItem )
{
	UINT flags = MF_STRING;
	if( ! IsWindowEnabled( GetDlgItem( hDlg, idCtrl ) ) )
		flags |= MF_GRAYED;
	
	return AppendMenu( hMenu, flags, (UINT_PTR) idCtrl, strItem );
}

static void OnContextMenu( HWND hDlg, HWND hLB, LPARAM lParam )
{
	const LRESULT Sel = SendMessage( hLB, LB_GETCURSEL, 0, 0 );
	if( Sel == LB_ERR )	return;
	SendMessage( hLB, LB_SETCURSEL, (WPARAM) Sel, 0 ); // ensure visible

	TPMPARAMS tpmp;
	tpmp.cbSize = (UINT) sizeof(tpmp);
	if( SendMessage( hLB, 
					LB_GETITEMRECT,
					(WPARAM) Sel, 
					(LPARAM) &tpmp.rcExclude ) == LB_ERR ) return;

	MapWindowRect( hLB, HWND_DESKTOP, &tpmp.rcExclude );

	TCHAR strItem[ CCH_ITEM_MAX ] = TEXT( "" );
	const LRESULT Len = SendMessage( hLB, LB_GETTEXTLEN, (WPARAM) Sel, 0 );
	if( Len == LB_ERR || CCH_ITEM_MAX <= Len ) return;
	SendMessage( hLB, LB_GETTEXT, (WPARAM) Sel, (LPARAM) strItem );

	HMENU hMenu = CreatePopupMenu();
	if( hMenu )
	{
		AppendMenu( hMenu, MF_STRING | MF_DISABLED, 0, strItem );
		SetMenuDefaultItem( hMenu, 0, TRUE );

		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		AppendMenu2( hMenu, hDlg, IDOK, TEXT( "&Limit this" ) );
		AppendMenu2( hMenu, hDlg, IDC_WATCH, TEXT( "Limit/&Watch" ) );
		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		AppendMenu2( hMenu, hDlg, IDC_HIDE, TEXT( "&Hide" ) );
		AppendMenu2( hMenu, hDlg, IDC_SHOW, TEXT( "&Show" ) );

		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		AppendMenu2( hMenu, hDlg, IDC_FRIEND, TEXT( "Mark as &Friend" ) );
		AppendMenu2( hMenu, hDlg, IDC_RESET_IFF, TEXT( "Mark as &Unknown" ) );
		AppendMenu2( hMenu, hDlg, IDC_FOE, TEXT( "Mark as F&oe" ) );

		POINT pt;
		if( GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1 )
		{
			pt.x = ( tpmp.rcExclude.left + tpmp.rcExclude.right ) / 2L;
			pt.y = ( tpmp.rcExclude.top + tpmp.rcExclude.bottom ) / 2L;
		}
		else
			GetCursorPos( &pt );
		
		const int cmd = TrackPopupMenuEx( hMenu, 
										TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_VERTICAL,
										pt.x, pt.y, hDlg, &tpmp );
		if( cmd )
		{
			// No SelNextOnHide for a context menu command
			const BOOL bOriginalFlagSaved = g_bSelNextOnHide;
			g_bSelNextOnHide = FALSE;
			SendMessage( hDlg, WM_COMMAND, (WPARAM) cmd, 0u );
			g_bSelNextOnHide = bOriginalFlagSaved;
		}

		DestroyMenu( hMenu );
	
	} // hMenu
}

static bool _always_list_all( void )
{
	const UINT menuState = GetMenuState( GetMenu( g_hWnd ), IDM_ALWAYS_LISTALL, MF_BYCOMMAND );
	return ( menuState != (UINT) -1 && ( MF_CHECKED & menuState ) );
}

#define strDefListBtn _T("List &all")
#define strNondefListBtn _T("Norm&al list")
static bool _is_list_button_def( HWND hDlg )
{
	TCHAR strBtn[ 32 ] = _T("");
	GetDlgItemText( hDlg, IDC_LISTALL_SYS, strBtn, 32 );
	return ( _tcscmp( strBtn, strDefListBtn ) == 0 );
}

static void _add_item( HWND hwndList, const TCHAR * hdr, const TARGETINFO& ti )
{
	TCHAR strShortText[ 50 ]; // up to 48 cch
	_tcsncpy_s( strShortText, 50, ti.szText, 49 );

	if( _tcslen( strShortText ) == 49 ) // if len==49, too long; if len=48, not truncated
	{
		strShortText[ 45 ] = _T('.');
		strShortText[ 46 ] = _T('.');
		strShortText[ 47 ] = _T('.');
		strShortText[ 48 ] = _T('\0');
	}

	TCHAR strItem[ CCH_ITEM_MAX ];
	_stprintf_s( strItem, _countof(strItem), _T("%s%s <%s>"), hdr, ti.szExe, strShortText );
	SendMessage( hwndList, LB_INSERTSTRING, (WPARAM) -1, (LPARAM) strItem );
}

static inline void Tcscpy_s( TCHAR * pDst, size_t cchDst, const TCHAR * pSrc )
{
	if( pDst != pSrc ) _tcscpy_s( pDst, cchDst, pSrc );
}

INT_PTR CALLBACK xList( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static ptrdiff_t maxItems = 128;     // remember as static if maxItems is increased
	static ptrdiff_t cItems = 0;
	static TARGETINFO * ti = NULL;       // maxItems
	static ptrdiff_t * MetaIndex = NULL; // maxItems
	static LPHACK_PARAMS lphp;

	static TCHAR * lpSavedWindowText = NULL;
	static HBRUSH hListBoxBrush = NULL;
	static int current_IFF = 0;

	switch( message )
	{
		case WM_INITDIALOG:
		{
			lphp = (LPHACK_PARAMS) lParam;

			ti = (TARGETINFO *)
				HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TARGETINFO) * maxItems );
			
			MetaIndex = (ptrdiff_t *)
				HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ptrdiff_t) * maxItems );
			
			lpSavedWindowText = (TCHAR*)
				HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(TCHAR) * MAX_WINDOWTEXT );
			
			if( ! lphp || ! ti || ! MetaIndex || ! lpSavedWindowText )
			{
				EndDialog( hDlg, XLIST_CANCELED );
				break;
			}

#ifdef _UNICODE
			if( IS_JAPANESEo )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_JPNo_1000, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_JPN_1000, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_FINNISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FIN_1000, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1000, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_CHINESE_T )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1000T, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_CHINESE_S )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1000S, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1000, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
			}
			else
#endif
			{
				_tcscpy_s( lpSavedWindowText, MAX_WINDOWTEXT, _T( "Which process would you like to limit?" ) );
				SetWindowText( hDlg, lpSavedWindowText );
			}


			if( g_bHack[ 2 ] || g_bHack[ 3 ] )
			{
				EnableWindow( GetDlgItem( hDlg, IDC_WATCH ), FALSE );
			}

			SendDlgItemMessage( hDlg, IDC_TARGET_LIST, WM_SETFONT, 
								(WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
			SendDlgItemMessage( hDlg, IDC_EDIT_INFO, WM_SETFONT, 
								(WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
			SendDlgItemMessage( hDlg, IDC_EDIT_INFO, EM_SETMARGINS,
								(WPARAM) ( EC_LEFTMARGIN | EC_RIGHTMARGIN ), MAKELPARAM( 5 , 5 ) );

			SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_SETHORIZONTALEXTENT, 600U, 0 );
			
			SetWindowText( GetDlgItem( hDlg, IDC_LISTALL_SYS ), 
							_always_list_all() ? strNondefListBtn : strDefListBtn );
			
			SendMessage( hDlg, WM_USER_REFRESH, 0U, 0L );

			DefListProc = (WNDPROC_FARPROC)
							SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_TARGET_LIST ),
												GWLP_WNDPROC,
												(LONG_PTR) &SubListProc );

			PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
			g_hListDlg = hDlg;
			break;
		}

		case WM_USER_REFRESH:
		{
			if( ! ti || ! MetaIndex ) break;

			BOOL bBusySSTP = FALSE;

			const int listLevel = _is_list_button_def( hDlg ) ? 0 : LIST_SYSTEM_PROCESS;
			
			const ptrdiff_t prev_maxItems = maxItems;
			cItems = UpdateProcessSnap( ti, maxItems, listLevel );
			if( prev_maxItems != maxItems )
			{
				LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 
											MetaIndex, sizeof(ptrdiff_t) * maxItems );
				if( ! lpv )
				{
					HeapFree( GetProcessHeap(), 0L, MetaIndex );
					MetaIndex = NULL;
					EndDialog( hDlg, XLIST_CANCELED );
					break;
				}

				MetaIndex = (ptrdiff_t*) lpv;
			}

			ptrdiff_t index = 0;
			ptrdiff_t m = 0;
			ptrdiff_t cursel = 0;
			const DWORD dwTargetId = (DWORD) wParam;

			HWND hwndList = GetDlgItem( hDlg, IDC_TARGET_LIST );
			SetWindowRedraw( hwndList, FALSE );
			SendMessage( hwndList, LB_RESETCONTENT, 0U, 0L );
			SendMessage( hwndList, LB_INITSTORAGE, 
						(WPARAM) cItems, (LPARAM)( 32 * sizeof(TCHAR) * cItems ) );

			for( index = 0; index < cItems; ++index )
			{
				if( ti[ index ].iIFF == IFF_ABS_FOE )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, _T("[++] "), ti[ index ] );
					
					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;

					if( Lstrcmpi( _T( "aviutl.exe" ), ti[ index ].szExe ) == 0 )
					{
						bBusySSTP = SSTP_Aviutl( hDlg, ti[ index ].szText );
					}
				}
			}

			for( index = 0; index < cItems; ++index )
			{
				if( ti[ index ].iIFF == IFF_FOE )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, _T("[+] "), ti[ index ] );

					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;
				}
			}

			for( index = 0; index < cItems; ++index )
			{
				if( ti[ index ].iIFF == IFF_UNKNOWN )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, _T(""), ti[ index ] );

					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;
				}
			}
			
			for( index = 0; index < cItems; ++index )
			{
				if( ti[ index ].iIFF == IFF_FRIEND )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, _T("[-] "), ti[ index ] );

					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;
				}
			}

			for( index = 0; index < cItems; ++index )
			{
				if( ti[ index ].iIFF == IFF_SYSTEM )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, _T("[--] "), ti[ index ] );

					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;
				}
			}

			if( cItems )
			{
				SendMessage( hwndList, LB_SETCURSEL, (WPARAM) cursel, 0L );
				SendMessage( hDlg, WM_COMMAND, 
								MAKEWPARAM( IDC_TARGET_LIST, LBN_SELCHANGE ), (LPARAM) hwndList );
			}

			SetFocus( hwndList );
			SetWindowRedraw( hwndList, TRUE );
			InvalidateRect( hwndList, NULL, TRUE );

			if( ! bBusySSTP	&& LANG_JAPANESE == PRIMARYLANGID( GetSystemDefaultLangID() ) )
			{
				SYSTEMTIME st;
				GetSystemTime( &st );
				DirectSSTP( hDlg, (st.wSecond & 1) ? S_TARGET_SJIS : S_TARGET2_SJIS, "", "Shift_JIS" );
			}
			break;
		}

		case WM_COMMAND:
		{
			if( ! ti || ! MetaIndex ) break;

#define THIS_BUTTON_IS_DISABLED(id)	\
	( ! IsWindowEnabled( GetDlgItem( hDlg, (id) ) ) )

			switch( LOWORD( wParam ) )
			{
				case IDOK:
				{
					if( THIS_BUTTON_IS_DISABLED(IDOK) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );

					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];

					if( IsProcessBES( ti[ index ].dwProcessId ) )
					{
						MessageBox( hDlg, TEXT( "BES can't target itself!" ),
									APP_NAME, MB_OK | MB_ICONEXCLAMATION );
						return 1;
					}

					if( lParam != (LPARAM) XLIST_WATCH_THIS )
					{
						for( int i = 0; i < 3; i++ )
						{
							if(
								ti[ index ].dwProcessId == g_dwTargetProcessId[ i ]
								&&
								g_bHack[ i ] 
							)
							{
								TCHAR str[ 1024 ] = _T("");
								_stprintf_s( str, _countof(str), _T( "%s [ 0x%08lX ] is already targeted as #%d." ),
									ti[ index ].szExe, ti[ index ].dwProcessId, ( i + 1 )
								);
								MessageBox( hDlg, str, APP_NAME, MB_OK | MB_ICONEXCLAMATION );
								return 1L;
							}
						}
					}
					
					// update the flag @20120927
					ti[ index ].fWatch = ( lParam == (LPARAM) XLIST_WATCH_THIS );

					const INT_PTR limitThis = DialogBoxParam( g_hInst,
														(LPCTSTR) IDD_QUESTION, hDlg, &Question,
														(LPARAM) &ti[ index ] );

					if( limitThis ) 
					{
						lphp->lpTarget->dwProcessId = ti[ index ].dwProcessId;


						/*for( ptrdiff_t s = 0; s < (ptrdiff_t) ti[ index ].wThreadCount; ++s )
						{
							lphp->lpTarget->dwThreadId[ s ] = ti[ index ].dwThreadId[ s ];
						}*/
						lphp->lpTarget->iIFF = ti[ index ].iIFF;
						_tcscpy_s( lphp->lpTarget->szExe, _countof(lphp->lpTarget->szExe), ti[ index ].szExe );
						_tcscpy_s( lphp->lpTarget->szPath, _countof(lphp->lpTarget->szPath), ti[ index ].szPath );
						_tcscpy_s( lphp->lpTarget->szText, _countof(lphp->lpTarget->szText), ti[ index ].szText );
						//lphp->lpTarget->wThreadCount = ti[ index ].wThreadCount;

						if( ti[ index ].iIFF == IFF_UNKNOWN
							|| ti[ index ].iIFF == IFF_FRIEND )
						{
							if( g_numOfEnemies < MAX_PROCESS_CNT )
								_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], MAX_PATH * 2, ti[ index ].szExe );

							// $FIX(1/5) @ 20140306 (ported 20140307)
							for( int i = 0; i < g_numOfFriends; ++i )
							{
								// formerly considered as Friend
								if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
								{
									if( g_numOfFriends >= 2 )
									{
										Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );
										*g_lpszFriend[ g_numOfFriends - 1 ] = _T('\0');
									}
									--g_numOfFriends;
									break;
								}
							}
						}

						if( lParam == (LPARAM) XLIST_WATCH_THIS )
						{
							EndDialog( hDlg, XLIST_WATCH_THIS );
						}
						else if( ti[ index ].dwProcessId == g_dwTargetProcessId[ 0 ] )
						{
							EndDialog( hDlg, XLIST_RESTART_0 );
						}
						else if( ti[ index ].dwProcessId == g_dwTargetProcessId[ 1 ] )
						{
							EndDialog( hDlg, XLIST_RESTART_1 );
						}
						else if( ti[ index ].dwProcessId == g_dwTargetProcessId[ 2 ] )
						{
							EndDialog( hDlg, XLIST_RESTART_2 );
						}
						else
						{
							EndDialog( hDlg, XLIST_NEW_TARGET );
						}
					} // limitThis
					else // cancel
					{
						// reset the flag @20120927
						ti[ index ].fWatch = false;
					}
					
					break;
				
				} // case IDOK:

				
				case IDCANCEL:
				{
					EndDialog( hDlg, XLIST_CANCELED );
					break;
				}

				case IDC_WATCH:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_WATCH) ) break;
					SendMessage( hDlg, WM_COMMAND, IDOK, (LPARAM) XLIST_WATCH_THIS );
					break;
				}

				case IDC_LISTALL_SYS:
				{
					const LRESULT selectedItem
						= SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
					
					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];
					
					if( _is_list_button_def( hDlg ) ) // btn status is currently default ("List all")
					{
						SetDlgItemText( hDlg, IDC_LISTALL_SYS, strNondefListBtn ); // toggle the btn
					}
					else // btn status is currently non-default ("Normal list")
					{
						SetDlgItemText( hDlg, IDC_LISTALL_SYS, strDefListBtn ); // toggle the btn
						
						if( _always_list_all() ) // restore the default if Always-List-All is checked
							SendMessage( g_hWnd, WM_COMMAND, IDM_ALWAYS_LISTALL, 0 );
					}

					SendMessage( hDlg, WM_USER_REFRESH, ti[ index ].dwProcessId, 0 );
					break;
				}

				case IDC_RELOAD:
				{
					HWND hBtn = GetDlgItem( hDlg, IDC_RELOAD );

					// Don't reload if reloading. This makes sense in v1.5.x, where reloading
					// may take more than 300 ms; in v1.6.0, reloading takes only about 10 ms or less.
					// Still, better safe than sorry.
					if( ! IsWindowEnabled( hBtn ) )
						break;

					EnableWindow( hBtn, FALSE );

					LRESULT sel = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
					
					if( sel < 0 || cItems <= sel )
						sel = 0;

					const ptrdiff_t index = MetaIndex[ sel ];
					SendMessage( hDlg, WM_USER_REFRESH, ti[ index ].dwProcessId, 0 );
					
					EnableWindow( hBtn, TRUE );
					break;
				}

				case IDC_FOE:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_FOE) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];

					WriteIni_Time( ti[ index ].szExe );

					if( g_numOfEnemies < MAX_PROCESS_CNT )
						_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], MAX_PATH * 2, ti[ index ].szExe );

					for( int i = 0; i < g_numOfFriends; ++i )
					{
						// formerly considered as Friend
						if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
						{
							// $FIX(2/5) see above
							if( g_numOfFriends >= 2 )
							{
								Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );
								*g_lpszFriend[ g_numOfFriends - 1 ] = _T('\0');
							}
							--g_numOfFriends;
							break;
						}
					}

					SendMessage( hDlg, WM_USER_REFRESH, (WPARAM) ti[ index ].dwProcessId, 0L );
					break;
				}

				case IDC_FRIEND:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_FRIEND) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];

					WriteIni_Time( ti[ index ].szExe );

					if( g_numOfFriends < MAX_PROCESS_CNT )
						_tcscpy_s( g_lpszFriend[ g_numOfFriends++ ], MAX_PATH *2, ti[ index ].szExe );

					for( int i = 0; i < g_numOfEnemies; ++i )
					{
						// formerly considered as Foe
						if( Lstrcmpi( ti[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
						{
							// $FIX(3/5): see above.
							if( g_numOfEnemies >= 2 )
							{
								Tcscpy_s( g_lpszEnemy[ i ], MAX_PATH * 2, g_lpszEnemy[ g_numOfEnemies - 1 ] );
								*g_lpszEnemy[ g_numOfEnemies - 1 ] = _T('\0');
							}
							--g_numOfEnemies;
							break;
						}
					}

					SendMessage( hDlg, WM_USER_REFRESH, ti[ index ].dwProcessId, 0 );
					break;
				}

				case IDC_RESET_IFF:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_RESET_IFF) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];

					if( ti[ index ].iIFF == IFF_FRIEND )
					{
						ti[ index ].iIFF = IFF_UNKNOWN;
						for( int i = 0; i < g_numOfFriends; ++i )
						{
							if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
							{
								// $FIX(4/5): see above.
								if( g_numOfFriends >= 2 )
								{
									Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );
									*g_lpszFriend[ g_numOfFriends - 1 ] = _T('\0');
								}
								--g_numOfFriends;
								break;
							}
						}
					}
					else if( ti[ index ].iIFF == IFF_FOE )
					{
						ti[ index ].iIFF = IFF_UNKNOWN;
						for( int i = 0; i < g_numOfEnemies; ++i )
						{
							// formerly considered as Foe
							if( Lstrcmpi( ti[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
							{
								// $FIX(5/5): see above.
								if( g_numOfEnemies >= 2 )
								{
									Tcscpy_s( g_lpszEnemy[ i ], MAX_PATH * 2, g_lpszEnemy[ g_numOfEnemies - 1 ] );
									*g_lpszEnemy[ g_numOfEnemies - 1 ] = _T('\0');
								}
								--g_numOfEnemies;
								break;
							}
						}
					}

					SendMessage( hDlg, WM_USER_REFRESH, ti[ index ].dwProcessId, 0 );
					break;
				}

				case IDC_UNFREEZE:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_UNFREEZE) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0U, 0L );
					if( selectedItem < 0 || cItems <= selectedItem )
						break;

					const ptrdiff_t index = MetaIndex[ selectedItem ];
					if( ti[ index ].dwProcessId == GetCurrentProcessId() )
						break;

					TCHAR msg[ 4096 ] = _T("");
					_stprintf_s( msg, _countof(msg),
						_T( "Trying to unfreeze %s (%08lX).\r\n\r\nUse this command ONLY IN EMERGENCY when" )
						_T( " the target is frozen by BES.exe (i.e. the process has been suspended and won't resume).\r\n\r\n" )
						_T( "Such a situation should be quite exceptional, but might happen if BES crashes" )
						_T( " while being active." ),
						ti[ index ].szPath, ti[ index ].dwProcessId );
					if( IDCANCEL == 
						MessageBox( hDlg, msg, APP_NAME, MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2 ) ) break;

					*lphp->lpTarget = ti[ index ];
					EndDialog( hDlg, XLIST_UNFREEZE );
					break;
				}

				case IDC_HIDE:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_HIDE) ) break;

					HWND hLB = GetDlgItem( hDlg, IDC_TARGET_LIST );
					LONG_PTR idxListItem = SendMessage( hLB, LB_GETCURSEL, 0, 0 );
					if( idxListItem < 0 || cItems <= idxListItem ) // LB_ERR
						break;

					const ptrdiff_t index = MetaIndex[ idxListItem ];
					
					ShowProcessWindow( hDlg, ti[ index ].dwProcessId, BES_HIDE );
#ifndef KEY_UP				
#define KEY_UP(vk)   (0 <= (int) GetKeyState(vk))
#endif
					if( g_bSelNextOnHide && KEY_UP(VK_SHIFT) )
					{
						LONG_PTR lCount = SendMessage( hLB, LB_GETCOUNT, 0, 0 );
						if( idxListItem == lCount - 1 ) break;
						SendMessage( hLB, LB_SETCURSEL, (WPARAM)( idxListItem + 1 ), 0 );
						SendMessage( hDlg, WM_COMMAND, MAKEWPARAM( IDC_TARGET_LIST, LBN_SELCHANGE ), (LPARAM) hLB );
					}
					break;
				}
				
				case IDC_SHOW:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_SHOW) ) break;

					HWND hLB = GetDlgItem( hDlg, IDC_TARGET_LIST );
					LONG_PTR idxListItem = SendMessage( hLB, LB_GETCURSEL, 0, 0 );
					if( idxListItem < 0 || cItems <= idxListItem ) // LB_ERR
						break;
					
					const ptrdiff_t index = MetaIndex[ idxListItem ];
					
					ShowProcessWindow( hDlg, ti[ index ].dwProcessId, BES_SHOW );
					
					if( g_bSelNextOnHide && KEY_UP(VK_SHIFT) )
					{
						LONG_PTR lCount = SendMessage( hLB, LB_GETCOUNT, 0, 0 );
						if( idxListItem == lCount - 1 ) break;
						SendMessage( hLB, LB_SETCURSEL, (WPARAM)( idxListItem + 1 ), 0 );
						SendMessage( hDlg, WM_COMMAND, MAKEWPARAM( IDC_TARGET_LIST, LBN_SELCHANGE ), (LPARAM) hLB );
					}
					break;
				}
				
				case IDC_SHOW_MANUALLY:
				{
					if( THIS_BUTTON_IS_DISABLED(IDC_SHOW_MANUALLY) ) break;

					INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0U, 0L );
					if( selectedItem < 0 || cItems <= selectedItem )
						break;
					
					const ptrdiff_t index = MetaIndex[ selectedItem ];
					
					ShowProcessWindow( hDlg, ti[ index ].dwProcessId, BES_SHOW_MANUALLY );
					break;
				}

				case IDC_TARGET_LIST:
				{
					if( HIWORD( wParam ) == LBN_SELCHANGE )
					{
						const LRESULT selectedItem = 
										SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
						if( selectedItem < 0 || cItems <= selectedItem )
							break;

						const ptrdiff_t index = MetaIndex[ selectedItem ];
						
						SetProcessInfoMsg( hDlg, &ti[ index ] );

						EnableWindow( GetDlgItem( hDlg, IDOK ), TRUE );

						BOOL bWatchable = TRUE;
						EnableWindow( GetDlgItem( hDlg, IDC_FRIEND ), TRUE );
						EnableWindow( GetDlgItem( hDlg, IDC_FOE), TRUE );
						EnableWindow( GetDlgItem( hDlg, IDC_UNFREEZE ), TRUE );
						EnableWindow( GetDlgItem( hDlg, IDC_RESET_IFF ), FALSE );
						EnableWindow( GetDlgItem( hDlg, IDC_SHOW ), TRUE );
						EnableWindow( GetDlgItem( hDlg, IDC_SHOW_MANUALLY ), TRUE );
						EnableWindow( GetDlgItem( hDlg, IDC_HIDE ), TRUE );

						for( ptrdiff_t i = 0; i < 3; ++i )
						{
							if(	ti[ index ].dwProcessId == g_dwTargetProcessId[ i ]
								&& g_bHack[ i ] )
							{
								EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );
								bWatchable = FALSE;
								break;
							}
						}

						if( g_bHack[ 2 ] || g_bHack[ 3 ] )
						{
							bWatchable = FALSE;
						}

						if( ti[ index ].fThisIsBES )
						{
							EnableWindow( GetDlgItem( hDlg, IDOK ), FALSE );

							bWatchable = FALSE;
							EnableWindow( GetDlgItem( hDlg, IDC_FRIEND ), FALSE );
							EnableWindow( GetDlgItem( hDlg, IDC_FOE), FALSE );
							
							if( ti[ index ].dwProcessId == GetCurrentProcessId() )
							{
								EnableWindow( GetDlgItem( hDlg, IDC_UNFREEZE ), FALSE );
								EnableWindow( GetDlgItem( hDlg, IDC_SHOW_MANUALLY ), FALSE );
							}
							
							EnableWindow( GetDlgItem( hDlg, IDC_SHOW ), FALSE );
							EnableWindow( GetDlgItem( hDlg, IDC_HIDE ), FALSE );
						}
						else if( ti[ index ].iIFF == IFF_FRIEND )
						{
							EnableWindow( GetDlgItem( hDlg, IDC_FRIEND ), FALSE );
							EnableWindow( GetDlgItem( hDlg, IDC_RESET_IFF ), TRUE );
						}
						else if( ti[ index ].iIFF == IFF_FOE )
						{
							EnableWindow( GetDlgItem( hDlg, IDC_FOE), FALSE );
							EnableWindow( GetDlgItem( hDlg, IDC_RESET_IFF ), TRUE );
						}
						else if( ti[ index ].iIFF == IFF_ABS_FOE
							|| ti[ index ].iIFF == IFF_SYSTEM )
						{
							EnableWindow( GetDlgItem( hDlg, IDC_FRIEND ), FALSE );
							EnableWindow( GetDlgItem( hDlg, IDC_FOE), FALSE );
						}
						
						EnableWindow( GetDlgItem( hDlg, IDC_WATCH ), bWatchable );

						current_IFF = ti[ index ].iIFF;

						const RECT rect = { 10L, 290L, 150L, 320L };
						InvalidateRect( hDlg, &rect, TRUE );
					}
					else if( HIWORD( wParam ) == LBN_DBLCLK )
					{
						INT_PTR selectedItem = SendDlgItemMessage( hDlg, IDC_TARGET_LIST, LB_GETCURSEL, 0, 0 );
						if( selectedItem < 0 || cItems <= selectedItem )
							break;

						SendMessage( hDlg, WM_COMMAND, (WPARAM) IDOK, 0 );
					}
					break;
				}
			}

			break;
		}
		
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint( hDlg, &ps );
			const int iSaved = SaveDC( hdc );

			HFONT hOldFont = SelectFont( hdc, g_hFont );
			
			TCHAR strText[ 16 ];
			_tcscpy_s( strText, _countof(strText),
								( current_IFF < 0 ) ? _T("Friend")
								: ( current_IFF > 0 ) ? _T("Foe")
								: _T("Unknown") );
			
			
			SetBkMode( hdc, TRANSPARENT );
			SetTextColor( hdc, RGB( 0, 0, 0xff ) );
			TextOut( hdc, 50, 293, strText, (int) _tcslen( strText ) );

			HPEN hPen = CreatePen( PS_SOLID, 1, RGB( 0x80, 0x80, 0x80 ) );


			HPEN hOldPen = (HPEN) SelectObject( hdc, hPen );
			HBRUSH hBrushIFF = CreateSolidBrush(
								( current_IFF > 0 ) ? RGB( 0xff, 0x00, 0x00 )
								: ( current_IFF < 0 ) ? RGB( 0x00, 0xff, 0x66 )
								: RGB( 0xff, 0xff, 0x00 ) );
			
			HBRUSH hOldBrush = (HBRUSH) SelectObject( hdc, hBrushIFF );
			Ellipse( hdc, 20, 292, 40, 312 );

			HBRUSH hBlueBrush = CreateSolidBrush( RGB( 198, 214, 255 ) );
			SelectBrush( hdc, hBlueBrush );
			Rectangle( hdc, 518, 257, 618, 371 );
			
			SelectPen( hdc, hOldPen );
			DeletePen( hPen );
			
			SelectBrush( hdc, hOldBrush );
			DeleteBrush( hBlueBrush );
			DeleteBrush( hBrushIFF );
			
			SelectFont( hdc, hOldFont );
			
			RestoreDC( hdc, iSaved );

			EndPaint( hDlg, &ps );
			break;
		}		

		case WM_CTLCOLORLISTBOX:
		{
			HBRUSH ret = NULL;
			if( GetDlgCtrlID( (HWND) lParam ) == IDC_TARGET_LIST )
			{
				SetBkMode( (HDC) wParam, TRANSPARENT );
				SetTextColor( (HDC) wParam, RGB( 0x00, 0x00, 0x99 ) );
				
				if( ! hListBoxBrush )
					hListBoxBrush = CreateSolidBrush( RGB( 0xff, 0xff, 0xcc ) );

				ret = hListBoxBrush;
			}
			
			return (INT_PTR) ret;
			break;
		}

		case WM_CONTEXTMENU:
		{
			if( GetDlgCtrlID( (HWND) wParam ) == IDC_TARGET_LIST )
				OnContextMenu( hDlg, (HWND) wParam, lParam );
			break;
		}

		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			if( wParam == 0 && lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, MAX_WINDOWTEXT );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}
			
			if( message == WM_USER_CAPTION )
				break;

			return FALSE;
			break;
		}
		
		case WM_NCUAHDRAWCAPTION:
		{
			if( lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}
			return FALSE;
			break;
		}

		case WM_DESTROY:
		{
			g_hListDlg = NULL;

			SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_TARGET_LIST ),
										GWLP_WNDPROC, (LONG_PTR) DefListProc );
			DefListProc = NULL;

			if( hListBoxBrush )
			{
				DeleteBrush( hListBoxBrush );
				hListBoxBrush = NULL;
			}

			if( lpSavedWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpSavedWindowText );
				lpSavedWindowText = NULL;
			}

			if( MetaIndex )
			{
				HeapFree( GetProcessHeap(), 0L, MetaIndex );
				MetaIndex = NULL;
			}

			if( ti )
			{
				HeapFree( GetProcessHeap(), 0L, ti );
				ti = NULL;
			}
			
			lphp = NULL;
			break;
		}
		default:
			return FALSE;
	}
    return TRUE;
}


static BOOL SetProcessInfoMsg( HWND hDlg, const TARGETINFO * lpTarget )
{
	ptrdiff_t numOfThreads = 0;
	DWORD * pThreadIDs = ListProcessThreads_Alloc( lpTarget->dwProcessId, &numOfThreads );
	if( ! pThreadIDs )
	{
		SetDlgItemText( hDlg, IDC_EDIT_INFO, _T("") );
		return FALSE;
	}

	const size_t cchmsg = CCHPATH * 2 + MAX_WINDOWTEXT + 512 + 32 * (size_t) numOfThreads;
	TCHAR * msg = (TCHAR*) HeapAlloc( GetProcessHeap(), 0L, sizeof(TCHAR) * cchmsg );
	if( ! msg )
	{
		HeapFree( GetProcessHeap(), 0L, pThreadIDs );
		SetDlgItemText( hDlg, IDC_EDIT_INFO, _T("") );
		return FALSE;
	}
	*msg = _T('\0');

	TCHAR strFormat[ 1024 ] = TEXT( S_FORMAT1_ASC );
#ifdef _UNICODE
	if( IS_JAPANESE )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FORMAT1_JPN, -1, strFormat, 1023 );
	}
#endif

	_stprintf_s( msg, cchmsg, strFormat, // ~2400 (plus 32*numOfThreads)
					lpTarget->szExe, // 520
					lpTarget->szPath, // 520
					lpTarget->szText, // 1024
					lpTarget->dwProcessId, // 8
					lpTarget->dwProcessId, // 10
					(unsigned) numOfThreads // 10
				);

#ifdef _UNICODE
	if( ! IS_JAPANESE )
	{
		// "including %u thread(s)" <-- this (s)
		if( numOfThreads != 1 ) _tcscat_s( msg, cchmsg, _T( "s" ) );
	}
#endif
	_tcscat_s( msg, cchmsg, _T( ":\r\n" ) );

	for( ptrdiff_t i = 0; i < numOfThreads; ++i )
	{
		TCHAR line[ 100 ];
		_stprintf_s( line, _countof(line),
						_T( "Thread #%03u: ID 0x%04X %04X\r\n" ),
						(int) i + 1,
						HIWORD( pThreadIDs[ i ] ),
						LOWORD( pThreadIDs[ i ] )
		);

		_tcscat_s( msg, cchmsg, line );
	}

	_tcscat_s( msg, cchmsg, _T( "\r\n" ) );

	SetDlgItemText( hDlg, IDC_EDIT_INFO, msg );

	HeapFree( GetProcessHeap(), 0L, msg );
	HeapFree( GetProcessHeap(), 0L, pThreadIDs );
	return TRUE;
}

DWORD * ListProcessThreads_Alloc( DWORD dwOwnerPID, ptrdiff_t * pNumOfThreads )
{
	*pNumOfThreads = 0;
	
	ptrdiff_t num = 0;
	
	DWORD * pThreadIDs = NULL;
	
	for( ptrdiff_t maxNumOfThreads = 128; ; maxNumOfThreads *= 2 )
	{
		pThreadIDs = (DWORD *) HeapAlloc( GetProcessHeap(), 0L, 
											(SIZE_T)( sizeof(DWORD) * maxNumOfThreads ) );
	
		if( ! pThreadIDs )
			break;

		num = ListProcessThreads( dwOwnerPID, pThreadIDs, (ptrdiff_t) maxNumOfThreads );

		// All threads have been listed, or there are too many threads
		if( num < maxNumOfThreads || 32768 <= maxNumOfThreads )
			break;

		// Free the old memory block before retrying
		HeapFree( GetProcessHeap(), 0L, pThreadIDs );
		pThreadIDs = NULL;
	}

	if( pThreadIDs )
		*pNumOfThreads = num;
	
	return pThreadIDs;
}

ptrdiff_t ListProcessThreads( DWORD dwOwnerPID, DWORD * pThreadIDs, ptrdiff_t numOfMaxIDs ) 
{
	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0UL );
	if( hThreadSnap == INVALID_HANDLE_VALUE ) return 0;

	THREADENTRY32 te32;
	te32.dwSize = sizeof( THREADENTRY32 ); 

	if( ! Thread32First( hThreadSnap, &te32 ) )
	{
		CloseHandle( hThreadSnap );
		return 0;
	}

	ptrdiff_t numOfIDs = 0; 
	do 
	{ 
		if( te32.th32OwnerProcessID == dwOwnerPID )
		{
			pThreadIDs[ numOfIDs++ ] = te32.th32ThreadID;
		}
	} while( Thread32Next( hThreadSnap, &te32 )	&& numOfIDs < numOfMaxIDs ); 

	CloseHandle( hThreadSnap );
	return numOfIDs;
}


static PROCESS_THREAD_PAIR * CacheProcessThreads_Alloc( ptrdiff_t * num, ptrdiff_t * maxdup )
{
	*num = 0;

	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0UL );
	if( hThreadSnap == INVALID_HANDLE_VALUE )
		return NULL;

	THREADENTRY32 te32;
	te32.dwSize = (DWORD) sizeof(THREADENTRY32); 

	if( ! Thread32First( hThreadSnap, &te32 ) )
	{
		CloseHandle( hThreadSnap );
		return NULL;
	}

	ptrdiff_t maxPairs = 2048;
	PROCESS_THREAD_PAIR * pairs = (PROCESS_THREAD_PAIR *)
								HeapAlloc( GetProcessHeap(), 0L, 
											(SIZE_T)( sizeof(PROCESS_THREAD_PAIR) * maxPairs ) );

	if( ! pairs )
	{
		CloseHandle( hThreadSnap );
		return NULL;
	}

	ptrdiff_t numOfPairs = 0; 
	do 
	{
		if( numOfPairs == maxPairs )
		{
			LPVOID lpv = HeapReAlloc( GetProcessHeap(), 0L, pairs,
										(SIZE_T)( sizeof(PROCESS_THREAD_PAIR) * maxPairs * 2 ) );
			if( ! lpv )
			{
				HeapFree( GetProcessHeap(), 0L, pairs );
				pairs = NULL; // @20140308
				break;
			}
			
			pairs = (PROCESS_THREAD_PAIR *) lpv;
			maxPairs *= 2;
		}
	
		pairs[ numOfPairs ].pid = te32.th32OwnerProcessID;
		pairs[ numOfPairs ].tid = te32.th32ThreadID;
		
		++numOfPairs;

#define TOO_MANY (1048576L)

	} while( Thread32Next( hThreadSnap, &te32 )	&& numOfPairs < TOO_MANY ); 

	CloseHandle( hThreadSnap );
	
	if( pairs )
	{
		qsort( pairs, (size_t) numOfPairs, sizeof(PROCESS_THREAD_PAIR), &pair_comp ); 
		
		DWORD pid = (DWORD) -1;
		
		ptrdiff_t numOfDups = 0;
		for( ptrdiff_t j = 0; j < numOfPairs; ++j, ++numOfDups )
		{
			if( pairs[ j ].pid != pid )
			{
				if( *maxdup < numOfDups )
					*maxdup = numOfDups;

				numOfDups = 0;
				pid = pairs[ j ].pid;
			}
		}

		*num = numOfPairs;
	}
	
	return pairs;
}


static BOOL CALLBACK EnumThreadWndProc_DetectBES( HWND hwnd, LPARAM lParam )
{
	TCHAR strClass[ 32 ] = _T(""); // truncated if too long

	// #define APP_CLASS TEXT( "BATTLEENC" ) <-- 9 cch
	if( GetClassName( hwnd, strClass, 32 ) == 9 && ! _tcscmp( strClass, APP_CLASS ) )
	{
		*((BOOL*) lParam) = TRUE;
		return FALSE;
	}
	return TRUE;
}

BOOL IsProcessBES( DWORD dwProcessId, const PROCESS_THREAD_PAIR * pairs, ptrdiff_t numOfPairs )
{
	if( dwProcessId == 0 || dwProcessId == (DWORD) -1 ) // trivially false
		return FALSE;
	
	if( dwProcessId == GetCurrentProcessId() ) // this instance itself
		return TRUE;

	BOOL bThisIsBES = FALSE; // another instance of BES
	
	const ptrdiff_t maxNumOfThreads = 32; // BES doesn't have more than 32 threads
	DWORD rgThreadIDs[ maxNumOfThreads ];
	ptrdiff_t numOfThreads = 0;
	if( pairs )
	{
		for( ptrdiff_t i = 0; i < numOfPairs; ++i )
		{
			if( pairs[ i ].pid == dwProcessId )
			{
				rgThreadIDs[ numOfThreads++ ] = pairs[ i ].tid;
				if( numOfThreads == maxNumOfThreads )
					break;
			}
			else if( dwProcessId < pairs[ i ].pid )
				break;
		}
	}
	else
	{
		numOfThreads = ListProcessThreads( dwProcessId, rgThreadIDs, maxNumOfThreads );
	}

	for( ptrdiff_t i = 0; i < numOfThreads && ! bThisIsBES; ++i )
		EnumThreadWindows( rgThreadIDs[ i ], &EnumThreadWndProc_DetectBES, (LPARAM) &bThisIsBES );

	return bThisIsBES;
}

static INT_PTR CALLBACK Question( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static LPTARGETINFO lpTarget;
	static TCHAR * lpSavedWindowText = NULL;
	switch ( message )
	{
		case WM_INITDIALOG:
		{
			TCHAR msg1[ 1024 ] = _T("");
			TCHAR msg2[ 4096 ] = _T("");
			TCHAR format[ 1024 ] = _T("");

			lpTarget = (LPTARGETINFO) lParam;
			lpSavedWindowText = (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
													MAX_WINDOWTEXT * sizeof(TCHAR) );
			if( ! lpTarget || ! lpSavedWindowText )
			{
				EndDialog( hDlg, FALSE );
				break;
			}

#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					IS_JAPANESEo ? S_JPNo_1001 : S_JPN_1001,
					-1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_JPN : S_QUESTION1_JPN,
					-1, msg1, 1023
				);
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FORMAT2_JPN, -1, format, 1023 );
			}
			else if( IS_FINNISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FIN_1001, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_FIN : S_QUESTION1_FIN,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1001, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_SPA : S_QUESTION1_SPA,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_CHINESE_T )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1001T, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_CHIT : S_QUESTION1_CHIT,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_CHINESE_S )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1001S, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_CHIS : S_QUESTION1_CHIS,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1001, -1, lpSavedWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpSavedWindowText );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_FRE : S_QUESTION1_FRE,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else
#endif
			{
				_tcscpy_s( msg1, _countof(msg1), lpTarget->fWatch ?
					_T( S_QUESTION2_ASC ) : _T( S_QUESTION1_ASC ) );
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );

				_tcscpy_s( lpSavedWindowText, MAX_WINDOWTEXT, _T( "Confirmation" ) );
				SetWindowText( hDlg, lpSavedWindowText );
			}

			_stprintf_s( msg2, _countof(msg2), format,
				lpTarget->szPath,
				lpTarget->dwProcessId, lpTarget->dwProcessId,
				lpTarget->szText[ 0 ] ? lpTarget->szText : _T( "n/a" )
			);

			SendDlgItemMessage( hDlg, IDC_MSG1, WM_SETFONT, (WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
			//SendDlgItemMessage( hDlg, IDC_EDIT, WM_SETFONT, (WPARAM) hFont, MAKELPARAM( FALSE, 0 ) );
			SetDlgItemText( hDlg, IDC_MSG1, msg1 );
			SetDlgItemText( hDlg, IDC_EDIT, msg2 );
			
			PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
			break;
		}

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				case IDOK:
					EndDialog( hDlg, TRUE );
					break;
				case IDCANCEL:
					EndDialog( hDlg, FALSE );
					break;
				default:
					break;
			}
			break;
		}

		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			if( wParam == 0 && lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}
			
			if( message == WM_USER_CAPTION )
				break;
			
			return FALSE;
			break;
		}

		case WM_NCUAHDRAWCAPTION:
		{
			if( lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, MAX_WINDOWTEXT );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}
			return FALSE;
			break;
		}

		case WM_DESTROY:
			if( lpSavedWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpSavedWindowText );
				lpSavedWindowText = NULL;
			}
			break;

		default:
			return FALSE;
			break;
	}
    return TRUE;
}


bool IsAbsFoe( LPCTSTR strPath )
{
	const TCHAR * pntr = _tcsrchr( strPath, _T('\\') );
	return IsAbsFoe2( pntr ? pntr + 1 : strPath, strPath );
}

static bool IsAbsFoe2( LPCTSTR strExe, LPCTSTR strPath )
{
	bool fRetVal = false;
	size_t cchExe = _tcslen( strExe );
	TCHAR * lpExe = (TCHAR*) HeapAlloc( GetProcessHeap(), 0L, ( cchExe + 1 ) * sizeof(TCHAR) );
	if( lpExe )
	{
		// _totlower (towlower) works just like LCMapString w/ LCMAP_LOWERCASE if setlocale'ed
		// properly; this should be more efficient than calling Lstrcmpi again and again.
		const TCHAR * src = strExe;
		TCHAR * dst = lpExe;
		while( *src )
		{
			*dst++ = (TCHAR) _totlower( *src++ );
		}
		*dst = _T('\0');

		fRetVal = 
			(
				_tcscmp( _T( "lame.exe" ), lpExe ) == 0
					||
				_tcscmp( _T( "aviutl.exe" ), lpExe ) == 0
					||
				_tcscmp( _T( "virtualdubmod.e" ), lpExe ) == 0
					||
				_tcscmp( _T( "virtualdub.exe" ), lpExe ) == 0
					||
				_tcscmp( _T( "virtualdubmod.exe" ), lpExe ) == 0
					||
				_tcscmp( _T( "ffmpeg.exe" ), lpExe ) == 0
					||
				_tcsstr( strPath, _T( "VirtualDub" )  ) != NULL
					||
				_tcsncmp( lpExe, _T("x264"), 4 ) == 0
		);

		HeapFree( GetProcessHeap(), 0L, lpExe );
	}
	
	return fRetVal;
}

HMODULE SafeLoadLibrary( LPCTSTR pLibName )
{
	TCHAR strCurDir[ CCHPATH ] = _T("");
	GetCurrentDirectory( CCHPATH, strCurDir );
	
	// Quick-fix to prevent DLL preloading attacks
	TCHAR strSysDir[ CCHPATH ] = _T("");
	GetSystemDirectory( strSysDir, CCHPATH );
	SetCurrentDirectory( strSysDir );
	
	HMODULE hModule = LoadLibrary( pLibName );

	SetCurrentDirectory( strCurDir );
	return hModule;
}