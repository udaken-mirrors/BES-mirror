/* 
 *	Copyright (C) 2005-2019 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "list.h"
static ptrdiff_t pid_has_this_path(
	DWORD pid,
	const PATHINFO * arPathInfo,
	ptrdiff_t nPathInfo,
	TCHAR ** ppPath );
static int s_uPrefix = 1; // for Window Title Hack (IDC_CHANGE_TITLE etc.) @2019-08-15

static BOOL BES_ShowWindow( HWND hDlg, HWND hwnd, int iShow, const TCHAR * lpWinTitle )
{
	int nCmdShow;
	if( iShow == BES_SHOW_MANUALLY )
	{
		TCHAR szWindowText[ MAX_WINDOWTEXT ] = _T("");
		TCHAR msg[ MAX_WINDOWTEXT + 256 ] = _T("");
		
		if( ! GetWindowText( hwnd, szWindowText, _countof(szWindowText) ) )
			_tcscpy_s( szWindowText, _countof(szWindowText), _T("n/a") );

		_stprintf_s( msg, _countof(msg),
					_T("Show this window?\r\n\r\nhWnd : 0x%p\r\n\r\nWindow Title: %s"),
					hwnd,
					szWindowText );
		int iResponse =	MessageBox( hDlg, msg, APP_NAME, 
									MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON2 );
		
		if( iResponse == IDNO ) return TRUE;
		else if( iResponse == IDCANCEL ) return FALSE;
			
		ShowWindow( hwnd, SW_SHOWDEFAULT );
		BringWindowToTop( hwnd );
		RECT rect = {0};
		GetWindowRect( hwnd, &rect );
		RECT area = {0};
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
		SetForegroundWindow( hDlg );
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
	else if( iShow == IDC_CHANGE_TITLE || iShow == IDC_RESTORE_TITLE ) // 2019-08-15
	{
		if( lpWinTitle == NULL ) return FALSE;

		TCHAR szText[ 1000 ] = TEXT("");
		GetWindowText( hwnd, szText, (int) _countof(szText) );
		if( _tcsncmp( szText, lpWinTitle, 40 ) == 0 ) // text length adjusted: 49 cch is enough; we use the first 40 cch max
		{
			if( iShow == IDC_CHANGE_TITLE && ! IsWindowVisible( hwnd ) ) {
				MessageBox( hDlg, szText, TEXT("Error: Invisible Window"), MB_OK | MB_ICONHAND );
				return FALSE;
			}

			TCHAR szNewText[ 1024 ] = TEXT("");
			if( iShow == IDC_CHANGE_TITLE )
				_stprintf_s( szNewText, _countof(szNewText), _T("[%02u] %s"), s_uPrefix, szText );
			else
				_tcscpy_s( szNewText, _countof(szNewText), &szText[ 5 ] );

			TCHAR szMessage[ 4096 ] = TEXT("");
			_stprintf_s( szMessage, _countof(szMessage),
				_T("Current Window Title:\r\n\t%s\r\nNew Window Title:\r\n\t%s"),
				szText, szNewText );

			if( iShow == IDC_RESTORE_TITLE && ! has_prefix( &szText[ 0 ] ) ) return FALSE;

			const int id = MessageBox( hDlg, szMessage,
										iShow == IDC_CHANGE_TITLE ? _TEXT("Change?") : _TEXT("Restore?"),
										MB_OKCANCEL | MB_ICONQUESTION );
			if( id == IDOK )
			{
				SendMessage( hwnd, WM_SETTEXT, 0, (LPARAM) szNewText );
				if( iShow == IDC_CHANGE_TITLE ) ++s_uPrefix;
				return FALSE;
			}
		}
	}

	return TRUE;
}


static bool show_process_win_worker(
	HWND hDlg,
	const DWORD * pTIDs,
	ptrdiff_t numOfThreads,
	WININFO * pWinInfo,
	int iShow,
	const TCHAR * lpWinTitle )
{
	bool ret = false;

	// It would be more efficient if we call BES_ShowWindow for each window directly from
	// EnumProc, instead of once storing every window in a big array; however, BES_ShowWindow
	// may show MessageBox and waiting long, during which the target windows may change,
	// like disappear.  So it may be safer, though less efficient, to get a definitive snap
	// first.
	for( ptrdiff_t threadIndex = 0; threadIndex < numOfThreads; ++threadIndex )
	{
		pWinInfo->numOfItems = 0;
		EnumThreadWindows( pTIDs[ threadIndex ], &WndEnumProc, (LPARAM) pWinInfo );

		if( ! ret ) // true if at least one window is found
			ret = !! pWinInfo->numOfItems;

		const ptrdiff_t numOfWindows = pWinInfo->numOfItems;
		for( ptrdiff_t n = 0; n < numOfWindows; ++n )
		{
			if( ! BES_ShowWindow( hDlg, pWinInfo->hwnd[ n ], iShow, lpWinTitle ) )
				return true;
		}
	}

	return  ret;
}

static void ShowProcessWindow( HWND hDlg, DWORD dwProcessID, int iShow, const TCHAR * lpWinTitle = NULL )
{
	WININFO * lpWinInfo = (WININFO *) MemAlloc( sizeof(WININFO), 1 );
	if( lpWinInfo )
	{
		// lpWinInfo->pid = dwProcessID; /* unused */
		ptrdiff_t numOfThreads = 0;
		DWORD * pTIDs = ListProcessThreads_Alloc( dwProcessID, numOfThreads );
		if( pTIDs )
		{
			if( ! show_process_win_worker( hDlg, pTIDs, numOfThreads, lpWinInfo, iShow, lpWinTitle ) )
				MessageBox( hDlg, _T("No windows!"), APP_NAME, MB_OK | MB_ICONINFORMATION );

			HeapFree( GetProcessHeap(), 0, pTIDs );
		}

		HeapFree( GetProcessHeap(), 0, lpWinInfo );
	}
}

/*
static const PROCESS_THREAD_PAIR * _find_pid( DWORD pid, const PROCESS_THREAD_PAIR * pairs, ptrdiff_t numOfPairs )
{
	if( numOfPairs == 0 ) return NULL;

	ptrdiff_t x = numOfPairs / 2;
	if( pairs[ x ].pid == pid )
	{
		return &pairs[ x ];
	}
	else if( pairs[ x ].pid < pid )
	{
		return _find_pid( pid, &pairs[ x + 1 ], numOfPairs - ( x + 1 ) );
	}
	else // pid < pairs[ x ].pid
	{
		return _find_pid( pid, pairs, x );
	}
}
*/

/*static void GetProcessDetails2( 
	DWORD dwProcessId,
	LPTARGETINFO lpTargetInfo,
	WININFO * lpWinInfo, // caller-provided buffer
	const PROCESS_THREAD_PAIR * sorted_pairs,
	ptrdiff_t numOfPairs,
	ptrdiff_t numOfTIDs // PROCESSENTRY32::cntThreads
)
{
//	DWORD * pTIDs = (DWORD*) HeapAlloc( GetProcessHeap(), 0L, sizeof(DWORD) * numOfTIDs );

//	if( ! pTIDs )
//		return 0;

	
	ptrdiff_t i = 0;

	// bsearch is not really faster
	while( i < numOfPairs && sorted_pairs[ i ].pid < dwProcessId )
		++i;


	get_process_details_worker( &sorted_pairs[ i ], numOfTIDs, lpTargetInfo->szText, lpWinInfo );
//	get_process_details_worker( pTIDs, numOfThreads, lpTargetInfo->szText, lpWinInfo );
//	HeapFree( GetProcessHeap(), 0L, pTIDs );

#ifdef _DEBUG
	ptrdiff_t numOfThreads = 0;
	while( i < numOfPairs && numOfThreads < numOfTIDs )
	{
		if(sorted_pairs[i].pid!=dwProcessId) 
		{
			TCHAR s[100];
			_stprintf_s(s,100,_T("\tDBG pair %lu:%lu for %lu\n"),
				sorted_pairs[i].pid,
				sorted_pairs[i].tid,
				dwProcessId);
			OutputDebugString(s);
		}
		++numOfThreads;
		++i;
//			pTIDs[ numOfThreads++ ] = sorted_pairs[ i++ ].tid;
	}
#endif

}
*/

void PathToExe( const TCHAR * pszPath, TCHAR * pszExe, rsize_t cchExe )
{
	if( ! pszPath ) {
		if( pszExe ) *pszExe = _T('\0');
		return;
	}
	const TCHAR * pBkSlash = _tcsrchr( pszPath, _T('\\') );
	_tcscpy_s( pszExe, cchExe, (pBkSlash && pBkSlash[ 1 ]) ? pBkSlash + 1 : pszPath );
}

CLEAN_POINTER DWORD * PathToProcessIDs( const PATHINFO& pathInfo, ptrdiff_t& numOfPIDs )
{
	numOfPIDs = 0;
	
	DWORD * arPIDs = (DWORD *) MemAlloc( sizeof(DWORD), MAX_SLOTS );
	if( ! arPIDs ) return NULL;

	if( pathInfo.cchPath )
	{
		HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
		if( hProcessSnap != INVALID_HANDLE_VALUE )
		{
			PROCESSENTRY32 pe32;
			pe32.dwSize = sizeof( PROCESSENTRY32 );

			for( BOOL bGoOn = Process32First( hProcessSnap, &pe32 );
					bGoOn && numOfPIDs < MAX_SLOTS;
					bGoOn = Process32Next( hProcessSnap, &pe32 ) )
			{
				if( pid_has_this_path( pe32.th32ProcessID, &pathInfo, 1, NULL ) != -1 )
					arPIDs[ numOfPIDs++ ] = pe32.th32ProcessID;
			} // for(bGoOn)
			
			CloseHandle( hProcessSnap );
		} // hProcessSnap OK
	}

	return arPIDs;
}

typedef struct tagFoundPID {
	TCHAR * lpPath;
	ptrdiff_t index;
	DWORD pid;
	int slot;
} FOUNDPID;

CLEAN_POINTER FOUNDPID * FindPIDs(
	const PATHINFO * arPathInfo,
	ptrdiff_t nPathInfo,
	const DWORD * pdwExcluded,
	ptrdiff_t nExcluded,
	ptrdiff_t& numOfPIDs )
{
	numOfPIDs = 0;
	ptrdiff_t maxNumOfPIDs = 25;
	bool fError = false;
	FOUNDPID * arFound = (FOUNDPID *) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
													sizeof(FOUNDPID) * (size_t) maxNumOfPIDs );
	if( ! arFound ) return NULL;

	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hProcessSnap != INVALID_HANDLE_VALUE )
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof( PROCESSENTRY32 );

		for( BOOL bGoOn = Process32First( hProcessSnap, &pe32 );
				bGoOn;
				bGoOn = Process32Next( hProcessSnap, &pe32 ) )
		{
			const DWORD dwThisPID = pe32.th32ProcessID;
			
			ptrdiff_t n = 0;
			while( n < nExcluded && dwThisPID != pdwExcluded[ n ] ) ++n;
			if( n < nExcluded ) continue; // exclude this PID
			
			TCHAR * lpPath = NULL;
			ptrdiff_t found_index = pid_has_this_path( dwThisPID, arPathInfo, nPathInfo, &lpPath );
			if( found_index != -1 )
			{
				if( numOfPIDs == maxNumOfPIDs )
				{
					if( MAX_SLOTS <= maxNumOfPIDs )
					{
						fError = true;
						break;
					}

					LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, arFound, 
												sizeof(FOUNDPID) * (size_t)( maxNumOfPIDs * 2 ) );
					if( ! lpv )
					{
						fError = true;
						break;
					}

					arFound = (FOUNDPID *) lpv;
					maxNumOfPIDs *= 2;
				}
#ifdef _DEBUG
TCHAR s[1000];swprintf_s(s,1000,L"inde=%d, look=%s, found=%s\n",found_index,
						 arPathInfo[found_index].pszPath,lpPath);
OutputDebugString(s);
#endif
				arFound[ numOfPIDs ].index = found_index;
				arFound[ numOfPIDs ].pid = dwThisPID;
				arFound[ numOfPIDs ].lpPath = lpPath;
				++numOfPIDs;
			}
		} // for(bGoOn)
		
		CloseHandle( hProcessSnap );
	} // hProcessSnap OK


	if( fError )
	{
		for( ptrdiff_t n = 0; n < numOfPIDs; ++n )
			if( arFound[ n ].lpPath ) TcharFree( arFound[ n ].lpPath );
		MemFree( arFound );
		arFound = NULL;
		numOfPIDs = 0;
	}

	return arFound;
}
#if 0
DWORD PathToProcessID(
	const PATHINFO * arPathInfo,
	ptrdiff_t nPathInfo,
	const DWORD * pdwExcluded,
	ptrdiff_t nExcluded,
	TCHAR ** ppPath,
	ptrdiff_t * px
)
{
	if( ppPath ) *ppPath = NULL;
	if( px ) *px = -1;

	if( ! arPathInfo ) return (DWORD) -1;

	DWORD dwRetVal = (DWORD) -1;

	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hProcessSnap != INVALID_HANDLE_VALUE )
	{
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof( PROCESSENTRY32 );

		for( BOOL bGoOn = Process32First( hProcessSnap, &pe32 );
				bGoOn && dwRetVal == (DWORD) -1;
				bGoOn = Process32Next( hProcessSnap, &pe32 ) )
		{
			const DWORD dwThisPID = pe32.th32ProcessID;
			
			ptrdiff_t n = 0;
			while( n < nExcluded && dwThisPID != pdwExcluded[ n ] ) ++n;
			if( n < nExcluded ) continue; // exclude this PID
			
			ptrdiff_t found_index = pid_has_this_path( dwThisPID, arPathInfo, nPathInfo, ppPath );
			if( found_index != -1 )
			{
				dwRetVal = dwThisPID;
				if( px ) *px = found_index;
			}
		} // for(bGoOn)
		
		CloseHandle( hProcessSnap );
	} // hProcessSnap OK

	/*HeapFree( GetProcessHeap(), 0, lpszGivenPath );*/
	return dwRetVal;
}
#endif
typedef DWORD (WINAPI *GetProcessImageFileName_t)(HANDLE,LPTSTR,DWORD);
typedef DWORD (WINAPI *GetLogicalDriveStrings_t)(DWORD,LPTSTR);
typedef DWORD (WINAPI *QueryDosDevice_t)(LPCTSTR,LPTSTR,DWORD);
extern GetProcessImageFileName_t
  g_pfnGetProcessImageFileName;
extern GetLogicalDriveStrings_t
  g_pfnGetLogicalDriveStrings;
extern QueryDosDevice_t
  g_pfnQueryDosDevice;

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

static CLEAN_POINTER TCHAR * ProcessToPath_Alloc( HANDLE hProcess, size_t * pcch )
{
	if( pcch ) *pcch = 0;
	size_t cch = 0;

	// NOTE: We can't handle a really long path here
	TCHAR szBuffer[ CCHPATH ] = _T("");
	TCHAR * lpszPath = NULL;

	if( GetModuleFileNameEx( hProcess, NULL, szBuffer, (DWORD) _countof(szBuffer) )
			&& szBuffer[ 0 ] != _T('\\')
		||
		g_pfnGetProcessImageFileName != NULL
			&& (*g_pfnGetProcessImageFileName)( hProcess, szBuffer, (DWORD) _countof(szBuffer) )
			&& DevicePathToDosPath( szBuffer, (DWORD) _countof(szBuffer) )
	)
	{
		size_t cchMem = _tcslen( szBuffer ) + 1;
		lpszPath = TcharAlloc( cchMem );
		if( ! lpszPath ) return NULL;
		
		cch = GetLongPathName( szBuffer, lpszPath, (DWORD) cchMem );

		if( cchMem <= cch )
		{
//MessageBox(NULL,szBuffer,_T("Short Path!"),MB_ICONEXCLAMATION|MB_TOPMOST);
			cchMem = cch + 1; // +1 just in case (not really needed)
			TcharFree( lpszPath );
			lpszPath = TcharAlloc( cchMem );
			if( ! lpszPath ) return NULL;
			cch = GetLongPathName( szBuffer, lpszPath, (DWORD) cchMem );
		}

		// Use whatever we have now if GetLongPathName failed
		if( ! cch || cchMem <= cch )
		{
			// TODO: use GetLastError()##
#ifndef _UNICODE
			// Path doesn't exist, probably because the actual path contains
			// non-ACP characters and can not be expressed w/o Unicode
			if( GetFileAttributes( szBuffer ) == INVALID_FILE_ATTRIBUTES )
			{
				//if( pdwErr ) *pdwErr = ERROR_NO_UNICODE_TRANSLATION;
				HeapFree( GetProcessHeap(), 0, lpszPath );
				return NULL;
			}
#endif
			_tcsncpy_s( lpszPath, cchMem, szBuffer, _TRUNCATE );
			cch = _tcslen( lpszPath );
		}

	}

	if( pcch ) *pcch = cch;
	return lpszPath;
}






BOOL SaveSnap( HWND hWnd )
{
	TCHAR strPath[ CCHPATH ] = TEXT( "snap.txt" );
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = TEXT( "Text File (*.txt)\0*.txt\0All Files\0*.*\0\0" );
	ofn.nFilterIndex = 1L;
	ofn.lpstrFile = strPath;
	ofn.nMaxFile = CCHPATH;
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
			TCHAR * lpPath = ProcessToPath_Alloc( hProcess, NULL ); // ###
			_ftprintf( fp, _T( "Full Path = %s\r\n" ), lpPath ? lpPath : _T("n/a") );
			if( lpPath ) HeapFree( GetProcessHeap(), 0, lpPath );

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

	CloseHandle( hProcessSnap );

	GetSystemTimeAsFileTime( &ft );
	ULONGLONG rt1 = (ULONGLONG) ft.dwHighDateTime << 32 | (ULONGLONG) ft.dwLowDateTime;
#if defined(_MSC_VER) && _MSC_VER < 1400
	double cost = (__int64)( rt1 - rt0 ) / 10000000.0;
#else
	double cost = ( rt1 - rt0 ) / 10000000.0;
#endif

	_ftprintf( fp, _T( "Cost : %.3f seconds" ), cost );
	fclose( fp );

	return TRUE;
}

#if 0
#ifdef STRICT
#define WNDPROC_FARPROC WNDPROC
#else
#define WNDPROC_FARPROC FARPROC
#endif
static WNDPROC_FARPROC  DefListProc = NULL;
static LRESULT CALLBACK SubListProc( HWND hLB, UINT uMessage, WPARAM wParam, LPARAM lParam )
{/*
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
*/
	return CallWindowProc( DefListProc, hLB, uMessage, wParam, lParam );
}
#endif
static BOOL AppendMenu2( HMENU hMenu, HWND hDlg, int idCtrl, LPCTSTR strItem )
{
	UINT flags = MF_STRING;
	if( ! IsWindowEnabled( GetDlgItem( hDlg, idCtrl ) ) )
		flags |= MF_GRAYED;
	
	return AppendMenu( hMenu, flags, (UINT_PTR) idCtrl, strItem );
}

static void LV_OnContextMenu( HWND hDlg, HWND hLV, LPARAM lParam, int iff, ptrdiff_t Sel )
{
	POINT pt = {0};
//	pt.x = GET_X_LPARAM( lParam );
//	pt.y = GET_Y_LPARAM( lParam );
	GetCursorPos( &pt );

	// Do nothing if the header control is r-clicked
	// (todo: maybe we can show a "column setup" menu here)
	RECT rcHeaderCtrl = {0};
	GetWindowRect( ListView_GetHeader( hLV ), &rcHeaderCtrl );
	if( PtInRect( &rcHeaderCtrl, pt ) ) return;

	ListView_EnsureVisible( hLV, Sel, FALSE ); // This may be needed on [Apps] OR [Shift]+[F10]
	
	// Don't let the popup menu hide the selected sel rect
	TPMPARAMS tpmp = {0};
	tpmp.cbSize = (UINT) sizeof(tpmp);
	ListView_GetItemRect( hLV, Sel, &tpmp.rcExclude, LVIR_BOUNDS );
	MapWindowRect( hLV, HWND_DESKTOP, &tpmp.rcExclude );

	UINT popupFlags = TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_VERTICAL;

	if( GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1 ) // VK_APPS / SHIFT+F10
	{
		RECT rcListViewBox = {0};
		GetWindowRect( hLV, &rcListViewBox );
		pt.x = ( rcListViewBox.left + rcListViewBox.right ) / 2L;
		pt.y = ( tpmp.rcExclude.top + tpmp.rcExclude.bottom ) / 2L;
		popupFlags |= TPM_CENTERALIGN;
	}

	HMENU hMenu = CreatePopupMenu();
	if( hMenu )
	{
		TCHAR szExe[ CCHPATH ] = TEXT("");
		ListView_GetItemText( hLV, Sel, EXE, szExe, _countof(szExe) );
		TCHAR szTitle[ 64 ] = TEXT(""); // text length adjusted: 49 cch is enough
		ListView_GetItemText( hLV, Sel, TITLE, szTitle, _countof(szTitle) );

		TCHAR szText[ CCHPATH + 104 ] = _T("");
		if( *szTitle )
			_stprintf_s( szText, _countof(szText), _T("%s <%s>"), szExe, szTitle );
		AppendMenu( hMenu, MF_STRING | MF_DISABLED, 0, *szTitle ? szText : szExe );
		SetMenuDefaultItem( hMenu, 0, TRUE );
		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );

		GetDlgItemText( hDlg, IDOK, szText, _countof(szText) );
		AppendMenu2( hMenu, hDlg, IDOK, szText );
		
		GetDlgItemText( hDlg, IDC_WATCH, szText, _countof(szText) );
		AppendMenu2( hMenu, hDlg, IDC_WATCH, szText );

		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		GetDlgItemText( hDlg, IDC_SLIDER_BUTTON, szText, _countof(szText) );
		AppendMenu2( hMenu, hDlg, IDC_SLIDER_BUTTON, szText );

		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		AppendMenu2( hMenu, hDlg, IDC_HIDE, TEXT( "&Hide" ) );
		AppendMenu2( hMenu, hDlg, IDC_SHOW, TEXT( "&Show" ) );

		if( szTitle[ 0 ] != _T('\0') )
		{
			HMENU hSubMenu2 = CreatePopupMenu(); // 2019-08-15
			if( hSubMenu2 )
			{
				UINT menuFlags = (UINT)( ! has_prefix(szTitle) && s_uPrefix <= 99 ? (MF_STRING) : (MF_STRING|MF_GRAYED) );
				AppendMenu( hSubMenu2, menuFlags, IDC_CHANGE_TITLE, TEXT( "&Change Title" ) );

				menuFlags = (UINT)( has_prefix(szTitle) ? (MF_STRING) : (MF_STRING|MF_GRAYED) );
				AppendMenu( hSubMenu2, menuFlags, IDC_RESTORE_TITLE, TEXT( "&Restore Title" ) );

				AppendMenu( hSubMenu2, MF_SEPARATOR, 0, NULL );
				menuFlags = (UINT)( s_uPrefix != 1 ? (MF_STRING) : (MF_STRING|MF_GRAYED) );
				AppendMenu( hSubMenu2, menuFlags, IDC_RESET_PREFIX, TEXT( "Reset &Prefix to [01]" ) );
				if( ! AppendMenu( hMenu, MF_POPUP, (UINT_PTR) hSubMenu2, TEXT("Window &Title Hack") ) )
					DestroyMenu( hSubMenu2 );
			}
		}

		AppendMenu( hMenu, MF_SEPARATOR, 0, NULL );
		AppendMenu2( hMenu, hDlg, IDC_FRIEND, TEXT( "Mark as &Friend" ) );
		AppendMenu2( hMenu, hDlg, IDC_RESET_IFF, TEXT( "Mark as &Unknown" ) );
		AppendMenu2( hMenu, hDlg, IDC_FOE, TEXT( "Mark as F&oe" ) );

		HMENU hSubMenu = CreatePopupMenu();
		if( hSubMenu )
		{
			UINT menuFlags = (UINT)( iff == IFF_SYSTEM ? (MF_STRING|MF_GRAYED) : (MF_STRING) );
			AppendMenu( hSubMenu, menuFlags, IDC_NUKE, TEXT("&End Process") );
			if( ! AppendMenu( hMenu, menuFlags | MF_POPUP, (UINT_PTR) hSubMenu, TEXT("&Kill") ) )
				DestroyMenu( hSubMenu );
		}
		
		const int cmd = TrackPopupMenuEx( hMenu, popupFlags, pt.x, pt.y, hDlg, &tpmp );

		DestroyMenu( hMenu ); // This function will destroy the menu and all its submenus

		if( cmd )
		{
			SendMessage( hDlg, WM_COMMAND, (WPARAM) cmd, 0 );
		}
	} // hMenu
}

static bool _always_list_all( void )
{
	const UINT menuState = GetMenuState( GetMenu( g_hWnd ), IDM_ALWAYS_LISTALL, MF_BYCOMMAND );
	return ( menuState != (UINT) -1 && ( MF_CHECKED & menuState ) );
}

#define strDefListBtn _T("List &all")
#define strNondefListBtn _T("Norm&al list")
/*static bool _is_list_button_def( HWND hDlg )
{
	TCHAR strBtn[ 32 ] = _T("");
	GetDlgItemText( hDlg, IDC_LISTALL_SYS, strBtn, (int) _countof(strBtn) );
	return ( _tcscmp( strBtn, strDefListBtn ) == 0 );
}*/


INT_PTR CALLBACK xList( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{

	//static ptrdiff_t maxItems = INIT_MAX_ITEMS;     // remember as static if maxItems is increased
	static ptrdiff_t cItems = 0;
	static ptrdiff_t curIdx = 0;
	static PROCESSINFO * ti = NULL;
#ifdef PTRSORT
	static PROCESSINFO ** lpPtrArr = NULL;
#endif
	///static ptrdiff_t * MetaIndex = NULL; // maxItems

	static TARGETINFO * pTarget = NULL;  // pointer to TARGETINFO ti[ MAX_SLOTS ];

	static HFONT hfontPercent = NULL;
	//static int hot[ MAX_SLOTS ];
	//static int current_IFF = 0;
	static int sort_algo = IFF;
	static int nReloading = 0;
	static bool fRevSort = false;
	static bool fQuickTimer = false;
	static bool fListAll = false;
	static bool fCPUTime = true; // show CPU load: disabled on WM_COMMAND and WM_DESTROY
	static bool fContextMenu = false; // context menu is on
	static bool fPaused = false; // auto-refresh disabled

	static bool fDebug = false;

	switch( message )
	{
		case WM_INITDIALOG:
		{
			fCPUTime = true;
			fContextMenu = false;
			nReloading = 0;

			pTarget = (TARGETINFO *) lParam;

			ti = (PROCESSINFO *)
				HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PROCESSINFO) * 64 );
			cItems = 64; // 20140401
			
			
			if( ! pTarget || ! ti /*|| ! MetaIndex*/ )
			{
				EndDialog( hDlg, XLIST_CANCELED );
				break;
			}

			TCHAR szWindowText[ MAX_WINDOWTEXT ] = _T("");

#ifdef _UNICODE
			if( IS_JAPANESEo )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_JPNo_1000, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_JPN_1000, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_FINNISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FIN_1000, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1000, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_CHINESE_T )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1000T, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_CHINESE_S )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1000S, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1000, -1, szWindowText, MAX_WINDOWTEXT );
			}
			else
#endif
			{
				_tcscpy_s( szWindowText, MAX_WINDOWTEXT, _T( "Which process would you like to limit?" ) );
			}
			SetWindowText( hDlg, szWindowText );

			//for( ptrdiff_t g = 0; g < MAX_SLOTS; ++g ) hot[ g ] = -1;

			SendDlgItemMessage( hDlg, IDC_TARGET_LIST, WM_SETFONT, 
								(WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
			SendDlgItemMessage( hDlg, IDC_EDIT_INFO, WM_SETFONT, 
								(WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
			SendDlgItemMessage( hDlg, IDC_EDIT_INFO, EM_SETMARGINS,
								(WPARAM) ( EC_LEFTMARGIN | EC_RIGHTMARGIN ), MAKELPARAM( 5 , 5 ) );

			_init_DialogPos( hDlg );
			_init_ListView( hDlg );

			sort_algo = (int) GetPrivateProfileInt( TEXT("Options"), TEXT("SortAlgo"), 0, GetIniPath() );
			fRevSort = !!GetPrivateProfileInt( TEXT("Options"), TEXT("RevSort"), 0, GetIniPath() );
			
			if( ! fPaused )
				SendDlgItemMessage( hDlg, IDC_AUTOREFRESH, BM_SETCHECK, BST_CHECKED, 0 );
			fListAll = _always_list_all();
			SetWindowText( GetDlgItem( hDlg, IDC_LISTALL_SYS ), 
							fListAll ? strNondefListBtn : strDefListBtn );

			HDC hDC = GetDC( hDlg );
			hfontPercent = MyCreateFont( hDC, TEXT( "Verdana" ), 12, TRUE, FALSE );
			ReleaseDC( hDlg, hDC );
			SendDlgItemMessage( hDlg, IDC_TEXT_PERCENT, WM_SETFONT, 
									(WPARAM) hfontPercent, MAKELPARAM( FALSE, 0 ) );
			_init_Slider( hDlg );
			

			DefLVProc = (WNDPROC_FARPROC) SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_TARGET_LIST ),
																	GWLP_WNDPROC,
																	(LONG_PTR) &SubLVProc );
			DefTBProc = (WNDPROC_FARPROC) SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_SLIDER ),
																	GWLP_WNDPROC,
																	(LONG_PTR) &SubTBProc );


			// Don't SendMessage (which will block initialization here), but PostMessage.
			PostMessage( hDlg, WM_USER_REFRESH, 0, 0 );
			fQuickTimer = true;
			SetTimer( hDlg, TIMER_ID, 500, NULL ); // retime 0.5 sec later

			PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
			g_hListDlg = hDlg;
			break;
		}

		case WM_TIMER:
		{
			if( wParam != TIMER_ID )
				break;

			DWORD dwPID = (WPARAM) -1;

			if( fQuickTimer )
			{
				KillTimer( hDlg, TIMER_ID ); // quick timer; freq 500 ms
				SetTimer( hDlg, TIMER_ID, 1750, NULL ); // normal timer; freq 1750 ms
				fQuickTimer = false;

				dwPID = 0;
			}

			// Don't update if we're in the menu-mode (eg. context menu is being shown) etc.,
			// because REFRESH may remove the selected item in the list for which
			// the context menu is on.
			if( fCPUTime
				&& ! fPaused
				&& ! nReloading
				&& ! fContextMenu
				&& IsWindowVisible( hDlg )
				&& ! IsLButtonDown()
				&& ! IsRButtonDown()
			)
			{
				SendMessage( hDlg, WM_USER_REFRESH, dwPID, 0 );
			}

			break;
		}

		case WM_USER_REFRESH:
		{
			if( ! ti || ! pTarget )
			{
				EndDialog( hDlg, XLIST_CANCELED );
				break;
			}

			++nReloading;

			if( LOWORD( lParam ) == URF_SORT_ALGO )
			{
				if( HIWORD( lParam ) == sort_algo ) // same subitem clicked consecutively
				{
					fRevSort = ! fRevSort; // reverse the sorting order
				}
				else
				{
					sort_algo = HIWORD( lParam );
					fRevSort = false;
				}
			}

#if 1//def _DEBUG
	LARGE_INTEGER li0 = {0};
	QueryPerformanceCounter(&li0);
#endif

			HWND hwndList = GetDlgItem( hDlg, IDC_TARGET_LIST );
			const int dx = GetScrollPos( hwndList, SBS_HORZ );
			//const int dy = GetScrollPos( hwndList, SBS_VERT );
			const int iTopIndex = ListView_GetTopIndex( hwndList );
			POINT pt = {0};
			ListView_GetItemPosition( hwndList, iTopIndex, &pt );
			POINT pt0 = {0};
			ListView_GetItemPosition( hwndList, 0, &pt0 );
			const LONG dy = pt.y - pt0.y;
			
			if( wParam == (WPARAM) -1 )
			{
				ptrdiff_t sel = curIdx;
			
				if( sel < 0 || cItems <= sel )
				{
					sel = 0;
				}
#ifdef PTRSORT
				wParam = (lpPtrArr && sel < cItems) ? lpPtrArr[ sel ]->dwProcessId : 0 ;
#else
				wParam = (sel < cItems) ? ti[ sel ].dwProcessId : 0 ;
#endif
			}

			//const int listLevel = _is_list_button_def( hDlg ) ? 0 : LIST_SYSTEM_PROCESS;

//			const ptrdiff_t prev_maxItems = maxItems;
			
			CachedList_Refresh( 1000 );
			ptrdiff_t numOfPairs;
			cItems = UpdateProcessSnap( ti, fListAll, &numOfPairs, cItems );

			if( ! ti )
			{
				EndDialog( hDlg, XLIST_CANCELED );
				break;
			}

#ifdef PTRSORT
			if( lpPtrArr ) HeapFree( GetProcessHeap(), 0, lpPtrArr );
			lpPtrArr = (PROCESSINFO**) HeapAlloc( GetProcessHeap(), 0,
													sizeof(PROCESSINFO*) * (SIZE_T) cItems );
			if( ! lpPtrArr )
			{
				EndDialog( hDlg, XLIST_CANCELED );
				--nReloading;
				break;
			}

			
#endif
			/*if( maxItems != prev_maxItems )
			{
				LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 
											MetaIndex, sizeof(ptrdiff_t) * maxItems );
				if( ! lpv )
				{
					HeapFree( GetProcessHeap(), 0, MetaIndex );
					MetaIndex = NULL;
					EndDialog( hDlg, XLIST_CANCELED );
					--nReloading;
					break;
				}

				MetaIndex = (ptrdiff_t*) lpv;
			}*/

			for( ptrdiff_t i = 0; i < cItems; ++i )
			{
				ptrdiff_t g = 0;
				for( ; g < MAX_SLOTS; ++g ) {
					if( ti[ i ].dwProcessId == pTarget[ g ].dwProcessId ) break;
				}
				// NOTE: g==MAX_SLOTS if no slot is used for that process
				
				ti[ i ].slotid = (WORD) g;
				
				/// FIX @ 20190317 ## g==MAX_SLOTS is possible; pTarget[MAX_SLOTS-1] is the last element
				/// ti[ i ].fWatched = pTarget[ g ].fWatch;
				ti[ i ].fWatched = ( g >= MAX_SLOTS ) ? false : pTarget[ g ].fWatch ;

				//ti[ i ].fUnlimited = pTarget[ g ].fUnlimited; // flag not (yet) used: use ! g_bHack[ g ] instead
				
#ifdef PTRSORT
				lpPtrArr[ i ] = &ti[ i ];
#endif
			}

#ifdef PTRSORT
# define SORTEE lpPtrArr
# define SORTSIZE sizeof(PROCESSINFO*)
#else
# define SORTEE ti
# define SORTSIZE sizeof(PROCESSINFO)
#endif
			if( fRevSort )
			{
				if( sort_algo == SLOT )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_SLOT2 );
				else if( sort_algo == IFF )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_IFF2 );
				else if( sort_algo == EXE )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_EXE2 );
				else if( sort_algo == CPU )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_CPU2 );
				else if( sort_algo == TITLE )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_TITLE2 );
				else if( sort_algo == PATH )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_PATH2 );
				else if( sort_algo == THREADS )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_THREADS2 );
				else
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_PID2 );
			}
			else
			{
				if( sort_algo == SLOT )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_SLOT );
				else if( sort_algo == IFF )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_IFF );
				else if( sort_algo == EXE )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_EXE );
				else if( sort_algo == CPU )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_CPU );
				else if( sort_algo == TITLE )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_TITLE );
				else if( sort_algo == PATH )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_PATH );
				else if( sort_algo == THREADS )
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_THREADS );
				else
					qsort( SORTEE, (size_t) cItems, SORTSIZE, &picmp_PID );
			}

			ptrdiff_t cursel = 0;
			const DWORD dwTargetId = (DWORD) wParam;

			SetWindowRedraw( hwndList, FALSE );

			ListView_DeleteAllItems( hwndList );

			for( ptrdiff_t index = 0; index < cItems; ++index )
			{
#ifdef PTRSORT
				_add_item( hwndList, lpPtrArr[ index ]->iIFF, *lpPtrArr[ index ] );
				if( dwTargetId && lpPtrArr[ index ]->dwProcessId == dwTargetId )
					cursel = index;
#else
				_add_item( hwndList, ti[ index ].iIFF, ti[ index ] );
				if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
					cursel = index;
#endif

			}


			/*LVFINDINFO find;
			find.flags = LVFI_PARAM;
			for( ptrdiff_t x = 0; x < MAX_SLOTS; ++x )
			{
				if( x != 3 && g_bHack[ x ] )
				{
					find.lParam = (LONG) pTarget[ x ].dwProcessId;
					hot[ x ] = ListView_FindItem( hwndList, -1, &find );
				}
				else hot[ x ] = -1;
			}*/

			for( int col_idx = 0; col_idx < N_COLS; ++col_idx )
			{
				ListView_SetColumnWidth( hwndList, col_idx, 
					( col_idx == SLOT || col_idx == IFF || col_idx == CPU || col_idx == THREADS )
					? LVSCW_AUTOSIZE_USEHEADER : LVSCW_AUTOSIZE );
			}

			if( ! dwTargetId )
			{
				cursel = 0;
			}

			if( lParam & URF_ENSURE_VISIBLE )
			{
				ListView_SetCurrentSel( hwndList, cursel, TRUE );
				SetWindowRedraw( hwndList, TRUE );
			}
			else
			{
				ListView_SetCurrentSel( hwndList, cursel, FALSE );
				SetWindowRedraw( hwndList, TRUE );
				if( dx || dy ) ListView_Scroll( hwndList, dx, dy );
			}

			if( lParam & URF_RESET_FOCUS )
				SendMessage( hDlg, WM_NEXTDLGCTL, (WPARAM) hwndList, MAKELPARAM( TRUE, 0 ) );


#if 1//def _DEBUG
	if( fDebug )
	{
		LARGE_INTEGER li = {0};
	LARGE_INTEGER freq = {0};
	TCHAR s[ 100 ] = _T("");
	QueryPerformanceCounter(&li);
	QueryPerformanceFrequency(&freq);
	_stprintf_s(s,100,_T("%d Processes, %d Threads: cost=%.2f ms"),
					(int) cItems,
					(int) numOfPairs,
					(freq.QuadPart !=0)	? (double)( li.QuadPart - li0.QuadPart ) / freq.QuadPart * 1000.0 : -1.0
				);
	//OutputDebugString(s);
	//OutputDebugString(_T("\n"));
	SetWindowText(hDlg,s);
	}
#endif	

			--nReloading;
			break;
		}

		case WM_COMMAND:
		{
			fCPUTime = false;

			const int CtrlID = LOWORD( wParam );
			if( CtrlID != IDC_NUKE
				&& CtrlID != IDC_CHANGE_TITLE // 2019-08-15
				&& CtrlID != IDC_RESTORE_TITLE
				&& CtrlID != IDC_RESET_PREFIX
				&& !( CtrlID == IDOK && lParam == XLIST_WATCH_THIS )
				&& ! IsWindowEnabled( GetDlgItem( hDlg, CtrlID ) ) )
			{
				fCPUTime = true;
				break;
			}

			const ptrdiff_t sel = curIdx;

#ifdef PTRSORT
			if( ti && lpPtrArr && 0 <= sel && sel < cItems && pTarget )
#else
			if( ti && 0 <= sel && sel < cItems && pTarget )
#endif
			{
				; // ok
			}
			else
			{
				fCPUTime = true;
				break;
			}
#ifdef PTRSORT
			PROCESSINFO& ProcessInfo = *lpPtrArr[ sel ];
#else
			PROCESSINFO& ProcessInfo = ti[ sel ];
#endif
			const DWORD pid = ProcessInfo.dwProcessId;
			switch( CtrlID )
			{
				case IDOK:
				case IDC_SLIDER_BUTTON:
				{
					ptrdiff_t numOfPairs = 0;
					PROCESS_THREAD_PAIR * sorted_pairs = GetCachedPairs( 1000, numOfPairs );
					BOOL bBES = IsProcessBES( pid, sorted_pairs, numOfPairs );
					if( sorted_pairs )
					{
						MemFree( sorted_pairs );
						sorted_pairs = NULL;
					}
					if( bBES )
					{
						MessageBox( hDlg, ProcessInfo.szPath, TEXT("BES Can't Target BES"),
									MB_OK | MB_ICONEXCLAMATION );
						break;
					}

					// This flag is used in the "Question" function; otherwise not important
					ProcessInfo.fWatch = ( lParam == XLIST_WATCH_THIS );

					ptrdiff_t g = 0;  // if the target is new, g = MAX_SLOTS
					while( g < MAX_SLOTS && ( g == 3 || pid != pTarget[ g ].dwProcessId ) ) ++g;
					
					if( g < MAX_SLOTS && lParam != XLIST_WATCH_THIS && g_bHack[ g ] ) // Unlimit
					{
						// STOPF_NO_LV_REFRESH = "Don't send back WM_USER_REFRESH"
						SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) g, STOPF_NO_LV_REFRESH | STOPF_UNLIMIT );
						SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE | URF_RESET_FOCUS );
						break;
					}
					
					ptrdiff_t iMyId = 0;
					int iSlider = 0;
					int response = 0; // HIWORD: 1=Watch,0=Limit; LOWORD: 2=ByProcess,1=ThreadByThread,0=Cancel
					int mode = DEF_MODE;

					// fLimitThis
					if( CtrlID == IDC_SLIDER_BUTTON
						|| ( response = DialogBoxParam( g_hInst, MAKEINTRESOURCE(IDD_QUESTION), hDlg,
											&Question, (LPARAM) &ProcessInfo ) ) != 0 )
					{
						if( HIWORD( response ) == 1 ) {
							ProcessInfo.fWatch = true;
							lParam = XLIST_WATCH_THIS;
						} else {
							ProcessInfo.fWatch = false;
							lParam = 0;
						}
						mode = LOWORD( response ) - 1;
						if( mode != 1 && mode != 0 ) mode = DEF_MODE;
						if( ProcessInfo.pExe &&
							(ProcessInfo.iIFF == IFF_UNKNOWN || ProcessInfo.iIFF == IFF_FRIEND) )//?
						{
							if( g_numOfEnemies < MAX_ENEMY_CNT )
							{
								size_t cchMem = _tcslen( ProcessInfo.pExe ) + 1;
								g_lpszEnemy[ g_numOfEnemies ]
									= TcharAlloc( cchMem );
								if( g_lpszEnemy[ g_numOfEnemies ] )
									_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], cchMem, ProcessInfo.pExe );
								else break;
							}      

							// If this target (=enemy) is an ex-friend, make the friend list
							// smaller by 1, by removing that ex-friend (at index i).
							// (1) If i!=last, then copy the last guy to the place at i.
							// (2) If i==last, just doing --g_num is enough (we'll write NUL too,
							//     just in case).  Do NOT copy in this case; copying is not
							//     only unnecessary, but the behavior is undefined, since
							//     overlapping (dst==src).
							for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
							{
								if( StrEqualNoCase( ProcessInfo.pExe, g_lpszFriend[ i ] ) )
								{
									// g_numOfFriends>=1: otherwise this code is not executed.
									// g_numOfFriends is thread-safe (unless ReadIni is called by another
									// thread, which isn't) // 20140315 updated
									if( --g_numOfFriends != i )
									{
										HeapFree( GetProcessHeap(), 0, g_lpszFriend[ i ] );
										g_lpszFriend[ i ] = g_lpszFriend[ g_numOfFriends ];
									}
									else
									{
										HeapFree( GetProcessHeap(), 0, g_lpszFriend[ g_numOfFriends ] );
									}
									g_lpszFriend[ g_numOfFriends ] = NULL;
								}
							}
						}

						if( lParam == XLIST_WATCH_THIS )
						{
							// If the target to watch is already limited, unlimit it once.
							for( g = 0; g < MAX_SLOTS; ++g )
							{
								if( g != 3 && pid == pTarget[ g ].dwProcessId )
								{
									if( g_bHack[ g ] )
										SendMessage( g_hWnd, WM_USER_STOP,
												(WPARAM) g, STOPF_NO_LV_REFRESH );
									if( g < 3 )
									{
										for( ptrdiff_t j = (4*g); j < (4*g) + 4; ++j )
											ppszStatus[ j ][ 0 ] = _T('\0');
									}
									
									ResetTi( pTarget[ g ] ); // ?
								}
							}

							iSlider = GetSliderIni( ProcessInfo.szPath, g_hWnd );

							if( g_bHack[ 3 ] )
							{
								size_t cchPath = _tcslen( ProcessInfo.szPath );
								size_t cchMem = 32 + cchPath + 50;
								TCHAR * lpMem = TcharAlloc( cchMem );
								if( ! lpMem ) break;

								_stprintf_s( lpMem, cchMem, _T("bes.exe -J \"%s\" %d;0;0;%d"), ProcessInfo.szPath, iSlider, mode );

								bool fAllowMoreThan99 = IsMenuChecked( g_hWnd, IDM_ALLOWMORETHAN99 );
								HandleJobList( g_hWnd, lpMem, fAllowMoreThan99, pTarget );
								TcharFree( lpMem );
								break;
							}

							// Get a sema
							for( iMyId = 2; 0 <= iMyId; --iMyId )
							{
								if( ! pTarget[ iMyId ].fUnlimited
									&& ! g_bHack[ iMyId ]
									&& WaitForSingleObject( hSemaphore[ iMyId ], 100 )
										== WAIT_OBJECT_0 ) break;
							}

							if( iMyId == -1 )
							{
								for( iMyId = 4; iMyId < MAX_SLOTS; ++iMyId ) // more slots
								{
									if( ! pTarget[ iMyId ].fUnlimited
										&& ! g_bHack[ iMyId ]
										&& WaitForSingleObject( hSemaphore[ iMyId ], 100 )
											== WAIT_OBJECT_0 ) break;
								}
							}

							if( iMyId == MAX_SLOTS )
							{
								MessageBox( hDlg, TEXT("Semaphore Error"), APP_NAME, MB_OK | MB_ICONSTOP );
								break;
							}

							// Slot 2 is already used
							if( iMyId != 2 )
							{
								// Copy the info of the target at slot 2
								if( ! TiCopyFrom( pTarget[ iMyId ], pTarget[ 2 ] ) )
								{
									ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
									break;
								}
//								const DWORD dwOldPID = pTarget[ 2 ].dwProcessId;
								g_Slider[ iMyId ] = g_Slider[ 2 ];
								SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) 2, STOPF_NO_LV_REFRESH );

								// Get sema 2 - wait until slot 2 is free again
								if( WaitForSingleObject( hSemaphore[ 2 ], 3000 ) != WAIT_OBJECT_0 )
								{
									MessageBox( hDlg, TEXT("Busy2"), APP_NAME, MB_ICONSTOP | MB_OK );
									ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
									break;
								}

								if( pTarget[ iMyId ].fUnlimited )
								{
									ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
								}
								else
								{
									// Don't release hSemaphore[ iMyId ]; it'll be used by the ex-slot2 target
									SendMessage( g_hWnd, WM_USER_HACK, (WPARAM) iMyId,
													(LPARAM) &pTarget[ iMyId ] );
								}
								
								iMyId = 2;
							}


							BOOL bReady = TiFromPi( pTarget[ 2 ], ProcessInfo );
							
							ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );

							if( bReady )
							{
								SetTargetPlus( g_hWnd, 
												pTarget,
												ProcessInfo.szPath,
												GetSliderIni( ProcessInfo.szPath, g_hWnd ),
												NULL,
												mode );
							}
						}
						else if( g < MAX_SLOTS && g != 3 ) // relimit
						{
							iSlider = g_Slider[ g ];
							SendMessage( g_hWnd, WM_USER_RESTART, MAKEWPARAM( g, mode ), 0 );
						}
						else // new target (no-watch)
						{
							iSlider = GetSliderIni( ProcessInfo.szPath, g_hWnd ); // +20170906
							LimitProcess_NoWatch( hDlg, pTarget, ProcessInfo, NULL, mode );
						}
					
						SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
						ActivateSlider( hDlg, iSlider );
					}
					else
					{
						ProcessInfo.fWatch = false;
						SetDlgItemFocus( hDlg, GetDlgItem( hDlg, IDC_TARGET_LIST ) );
					}

					break;
				} // case IDOK, IDC_SLIDER_BUTTON
				
				case IDCANCEL:
					EndDialog( hDlg, XLIST_CANCELED );
					break;

				case IDC_WATCH:
					//if( g_bHack[ 3 ] )//20151204@1.7.0.27
					//	SendMessage( g_hWnd, WM_COMMAND, IDM_UNWATCH, 0 ); // unwatch
					//else
						SendMessage( hDlg, WM_COMMAND, IDOK, XLIST_WATCH_THIS );
					break;

				case IDC_LISTALL_SYS:
				{
					fListAll = ! fListAll;
					if( fListAll ) // now list all
					{
						SetDlgItemText( hDlg, IDC_LISTALL_SYS, strNondefListBtn ); // toggle the btn

						//if( ! _always_list_all() )
						//	SendMessage( g_hWnd, WM_COMMAND, IDM_ALWAYS_LISTALL, 0 );
					}
					else // now normal list
					{
						SetDlgItemText( hDlg, IDC_LISTALL_SYS, strDefListBtn ); // toggle the btn
						
						if( _always_list_all() ) // restore the default if Always-List-All is checked
							SendMessage( g_hWnd, WM_COMMAND, IDM_ALWAYS_LISTALL, 0 );
					}

					SendMessage( hDlg, WM_USER_REFRESH, pid, 0 );
					break;
				}

				case IDC_RELOAD:
					if( ! nReloading ) // Don't reload if already reloading
					{
						++nReloading;
						SendMessage( hDlg, WM_USER_REFRESH, pid, URF_RESET_FOCUS );
						--nReloading;
					}
					break;

				case IDC_FOE:
				{
					if( ! ProcessInfo.pExe ) break;
					WriteIni_Time( ProcessInfo.pExe );

					if( g_numOfEnemies < MAX_ENEMY_CNT )
					{
						size_t cchMem = _tcslen( ProcessInfo.pExe ) + 1;
						g_lpszEnemy[ g_numOfEnemies ] = TcharAlloc( cchMem );
						if( g_lpszEnemy[ g_numOfEnemies ] )
							_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], cchMem, ProcessInfo.pExe );
						else break;
					}

					for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
					{
						if( StrEqualNoCase( ProcessInfo.pExe, g_lpszFriend[ i ] ) )
						{
							if( --g_numOfFriends != i )
							{
								HeapFree( GetProcessHeap(), 0, g_lpszFriend[ i ] );
								g_lpszFriend[ i ] = g_lpszFriend[ g_numOfFriends ];
							}
							else
							{
								HeapFree( GetProcessHeap(), 0, g_lpszFriend[ g_numOfFriends ] );
							}
							g_lpszFriend[ g_numOfFriends ] = NULL;
						}
					}

					ProcessInfo.iIFF = IFF_FOE; // <-- not really needed (?!)
					UpdateIFF_Ini( FALSE );
					SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
					break;
				}

				case IDC_FRIEND:
				{
					if( ! ProcessInfo.pExe ) break;
					WriteIni_Time( ProcessInfo.pExe );

					if( g_numOfFriends < MAX_FRIEND_CNT )
					{
						size_t cchMem = _tcslen( ProcessInfo.pExe ) + 1;
						g_lpszFriend[ g_numOfFriends ] = TcharAlloc( cchMem );
						if( g_lpszFriend[ g_numOfFriends ] != NULL )
							_tcscpy_s( g_lpszFriend[ g_numOfFriends++ ], cchMem, ProcessInfo.pExe );
						else break;
					}

					for( ptrdiff_t i = 0; i < g_numOfEnemies; ++i )
					{
						if( StrEqualNoCase( ProcessInfo.pExe, g_lpszEnemy[ i ] ) )
						{
							if( --g_numOfEnemies != i )
							{
								HeapFree( GetProcessHeap(), 0, g_lpszEnemy[ i ] );
								g_lpszEnemy[ i ] = g_lpszEnemy[ g_numOfEnemies ];
							}
							else
							{
								HeapFree( GetProcessHeap(), 0, g_lpszEnemy[ g_numOfEnemies ] );
							}
							g_lpszEnemy[ g_numOfEnemies ] = NULL;
						}
					}

					ProcessInfo.iIFF = IFF_FRIEND; // <-- not really needed (?!)
					UpdateIFF_Ini( FALSE );
					SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
					break;
				}

				case IDC_RESET_IFF:
				{
					if( ! ProcessInfo.pExe ) break;
					if( ProcessInfo.iIFF == IFF_FRIEND ) // ex-friend
					{
						for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
						{
							if( StrEqualNoCase( ProcessInfo.pExe, g_lpszFriend[ i ] ) )
							{
								if( --g_numOfFriends != i )
								{
									HeapFree( GetProcessHeap(), 0, g_lpszFriend[ i ] );
									g_lpszFriend[ i ] = g_lpszFriend[ g_numOfFriends ];
								}
								else
								{
									HeapFree( GetProcessHeap(), 0, g_lpszFriend[ g_numOfFriends ] );
								}
								g_lpszFriend[ g_numOfFriends ] = NULL;
							}
						}
					}
					else if( ProcessInfo.iIFF == IFF_FOE ) // ex-foe
					{
						for( ptrdiff_t i = 0; i < g_numOfEnemies; ++i )
						{
							if( StrEqualNoCase( ProcessInfo.pExe, g_lpszEnemy[ i ] ) )
							{
								if( --g_numOfEnemies != i )
								{
									HeapFree( GetProcessHeap(), 0, g_lpszEnemy[ i ] );
									g_lpszEnemy[ i ] = g_lpszEnemy[ g_numOfEnemies ];
								}
								else
								{
									HeapFree( GetProcessHeap(), 0, g_lpszEnemy[ g_numOfEnemies ] );
								}
								g_lpszEnemy[ g_numOfEnemies ] = NULL;
							}
						}
					}

					ProcessInfo.iIFF = IFF_UNKNOWN; // <-- not really needed (?!)
					UpdateIFF_Ini( FALSE );
					SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
					break;
				}

				case IDC_UNLIMIT_ALL:
					SendMessage( g_hWnd, WM_COMMAND, (WPARAM) IDM_STOP, (LPARAM) 0 );
					SetDlgItemFocus( hDlg, GetDlgItem( hDlg, IDC_TARGET_LIST ) );
					break;

				case IDC_UNFREEZE:
				{
					if( pid == GetCurrentProcessId() )
						break;

					TCHAR msg[ 4096 ] = _T("");
					_stprintf_s( msg, _countof(msg),
						_T( "Trying to unfreeze %s (%08lX).\r\n\r\nUse this command ONLY IN EMERGENCY when" )
						_T( " the target is frozen by BES.exe (i.e. the process has been suspended and won't resume).\r\n\r\n" )
						_T( "Such a situation should be quite exceptional, but might happen if BES crashes" )
						_T( " while being active." ),
						ProcessInfo.szPath, pid );
					if( MessageBox( hDlg, msg, APP_NAME, 
							MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2 ) == IDOK )
					{
						SendMessage( g_hWnd, WM_COMMAND, (WPARAM) IDM_STOP, (LPARAM) 0 );
						Sleep( 1000 );
						if( Unfreeze( hDlg, pid ) )
						{
							//MessageBox( hDlg, TEXT( "Successful!" ),
							//			TEXT( "Unfreezing" ), MB_ICONINFORMATION | MB_OK );
						}
						else
						{
							MessageBox( hDlg, TEXT( "An error occurred.\r\nPlease retry." ),
										TEXT( "Unfreezing" ), MB_ICONEXCLAMATION | MB_OK );
						}
					}

					break;
				}

				case IDC_NUKE:
					if( ProcessInfo.iIFF != IFF_SYSTEM
						&& pid != GetCurrentProcessId() )
					{
						_nuke( hDlg, ProcessInfo );
					}
					break;

				case IDC_AUTOREFRESH:
					fPaused = ( Button_GetCheck( (HWND) lParam ) == BST_UNCHECKED );
					break;

				case IDC_HIDE:
				case IDC_SHOW:
				case IDC_CHANGE_TITLE:  // 2019-08-15
				case IDC_RESTORE_TITLE:  // 2019-08-15
				{
					HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
					SetDlgItemFocus( hDlg, hLV );

					if( CtrlID == IDC_CHANGE_TITLE || CtrlID == IDC_RESTORE_TITLE )
					{
						TCHAR szTitle[ 64 ] = TEXT(""); // text length adjusted: 49 cch is enough
						const ptrdiff_t Sel = ListView_GetCurrentSel( hLV );
						ListView_GetItemText( hLV, Sel, TITLE, szTitle, _countof(szTitle) );
						ShowProcessWindow( hDlg, pid, CtrlID, szTitle );
						break;
					}

					ShowProcessWindow( hDlg, pid,
										CtrlID == IDC_HIDE ? BES_HIDE : BES_SHOW );

					if( ! fContextMenu && g_bSelNextOnHide && sel + 1 < cItems && KEY_UP( VK_SHIFT ) )
					{
						ListView_SetCurrentSel( hLV, sel + 1, TRUE );
						LV_OnSelChange( hDlg, SORTEE, cItems, sel + 1, curIdx );
					}
					break;
				}

				case IDC_RESET_PREFIX:
					if( MessageBox( hDlg, TEXT("The next prefix will be [01]."),
						TEXT("Window Title Hack"), MB_OKCANCEL | MB_ICONINFORMATION ) == IDOK ) s_uPrefix = 1;
					break;
				
				case IDC_SHOW_MANUALLY:
					ShowProcessWindow( hDlg, pid, BES_SHOW_MANUALLY );
					break;

				case IDC_SLIDER_Q:
					MessageBox( hDlg,
#ifdef _UNICODE
				L"NOTE:  You will specify a negative percentage here.  For example \x2212" L"25%"
				L" means that the target will only get 75% of the CPU time it would"
				L" normally get.  By going to \x2212" L"30%, \x2212" L"35%, \x2212" L"40%,\x2026 you will"
				L" THROTTLE MORE, making the target SLOWER."
#else
				TEXT("NOTE:  You will specify a negative percentage here.  For example -25%" )
				TEXT(" means that the target will only get 75% of the CPU time it would" )
				TEXT(" normally get.  By going to -30%, -35%, -40%,... you will" )
				TEXT(" THROTTLE MORE, making the target SLOWER." )
#endif
						,
						APP_NAME,
						MB_ICONINFORMATION | MB_OK );
					break;

				default:
					break;
			}

			fCPUTime = true;
			break;
		}

		case WM_NOTIFY:
		{
			if( ! lParam ) break;

			const NMHDR& notified = *(NMHDR*) lParam;
			if( notified.idFrom == IDC_TARGET_LIST )
			{
				if( notified.code == LVN_ITEMCHANGED )
				{
					NMLISTVIEW& nm = *(NMLISTVIEW*) lParam;
					if( nm.uNewState & LVIS_FOCUSED )
					{
						LV_OnSelChange( hDlg, SORTEE, cItems, nm.iItem, curIdx );
					}
				}
				else if( notified.code == NM_CLICK || notified.code == NM_RCLICK )
				{
					const NMITEMACTIVATE& nm = *(NMITEMACTIVATE*) lParam;
					// Unless LVS_EX_FULLROWSELECT, nm.iItem is -1 when a sub-item is clicked;
					// we can manually check the index.
					LVHITTESTINFO hit = {0};
					hit.pt = nm.ptAction;
					int itemIndex = ListView_SubItemHitTest( notified.hwndFrom, &hit );

					// Empty space after the text can be "nowhere"; this is a workaround.
					if( itemIndex == -1 && (hit.flags & LVHT_NOWHERE) )
					{
						hit.pt.x = 4L;
						itemIndex = ListView_SubItemHitTest( notified.hwndFrom, &hit );
					}
					
					if( itemIndex != -1 )
						ListView_SetCurrentSel( notified.hwndFrom, itemIndex, TRUE );
				}
				else if( notified.code == NM_DBLCLK )
				{
					if( IsWindowEnabled( GetDlgItem( hDlg, IDOK ) ) )
						SendMessage( hDlg, WM_COMMAND, (WPARAM) IDOK, 0 );
				}
				else if( notified.code == NM_CUSTOMDRAW )
				{
					// todo ret val
					return LV_OnCustomDraw( hDlg, notified.hwndFrom, lParam,
											ti, (DWORD) cItems, (DWORD) curIdx );
				}
			}
			else if( notified.code == HDN_ITEMCLICK )
			{
				HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
				if( notified.hwndFrom == ListView_GetHeader( hLV ) )
				{
					NMHEADER& clicked = *(NMHEADER*) lParam;

					if( clicked.iItem == sort_algo ) // same subitem clicked consecutively
					{
						fRevSort = ! fRevSort; // reverse the sorting order
					}
					else
					{
						sort_algo = clicked.iItem;
						fRevSort = false;
					}
					SendMessage( hDlg, WM_USER_REFRESH, (WPARAM) -1, 0 );
				}
			}
			break;
		}

		case WM_HSCROLL:
			if( GetDlgCtrlID( (HWND) lParam ) != IDC_SLIDER ) break;

			if( LOWORD( wParam ) == TB_ENDTRACK )
			{
				SendNotifyIconData( g_hWnd, pTarget, NIM_MODIFY );
			}
			else if( LOWORD( wParam ) < TB_ENDTRACK )
			{
				ptrdiff_t sel = curIdx;
				if( sel < 0 || cItems <= sel ) break;
#ifdef PTRSORT
				UINT k = lpPtrArr[ sel ]->slotid;
#else
				UINT k = ti[ sel ].slotid;
#endif

				if( MAX_SLOTS <= k ) break;
				LRESULT lSlider = SendMessage( (HWND) lParam, TBM_GETPOS, 0, 0 );
				if( lSlider < SLIDER_MIN || lSlider > SLIDER_MAX ) break;

				g_Slider[ k ] = (int) lSlider;

				TCHAR strPercent[ 16 ];
				GetPercentageString( lSlider, strPercent, _countof(strPercent) );

				SetDlgItemText( hDlg, IDC_TEXT_PERCENT, strPercent );
			
				if( k < 3 && g_bHack[ k ] )
				{
					_stprintf_s( ppszStatus[ 0 + 4 * k ], cchStatus,
								_T( "Target #%d [ %s ]" ),
								k + 1,
								strPercent );

					RECT rcStatus;
					SetRect( &rcStatus, 20, 20 + 90 * (int) k, 479, 40 + 90 * (int) k );
					InvalidateRect( g_hWnd, &rcStatus, FALSE );
				}
			}
			break;
		
		case WM_CTLCOLORSTATIC:
		{
			int idCtrl = GetDlgCtrlID( (HWND) lParam );
			if( idCtrl == IDC_TEXT_PERCENT )
			{
				SetBkMode( (HDC) wParam, TRANSPARENT );
				SetBkColor( (HDC) wParam, GetSysColor( COLOR_3DFACE ) );
				
				LRESULT pos = SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_GETPOS, 0, 0 );
				
				if( 1 <= pos && pos <= 99 )
				{
					int x = (int)(( pos - 1 ) * 255.0 / 98.0);
					SetTextColor( (HDC) wParam, RGB( (x), 0, (255-x/2) ) );
				}
				else
				{
					SetTextColor( (HDC) wParam, RGB( 0xff, 0, 0 ) );
				}

				return (INT_PTR) (HBRUSH) GetSysColorBrush( COLOR_3DFACE );
			}
			else if( idCtrl == IDC_EDIT_INFO )
			{
				const ptrdiff_t sel = curIdx;
				if( 0 <= sel && sel < cItems )
				{
#ifdef PTRSORT
					UINT k = lpPtrArr[ sel ]->slotid;
#else
					UINT k = ti[ sel ].slotid;
#endif
					if( k < MAX_SLOTS && g_bHack[ k ] )
					{
						SetBkMode( (HDC) wParam, OPAQUE );
						SetBkColor( (HDC) wParam, RGB( 0xff, 0, 0 ) );
						SetTextColor( (HDC) wParam, RGB( 0xff, 0xff, 0xff ) );
						return (INT_PTR)(HBRUSH) GetSysColorBrush( COLOR_3DFACE );
					}
				}
			}
			return (INT_PTR)(HBRUSH) NULL;
			break;
		}

		case WM_PAINT:
		{
			int current_IFF = 0;
#ifdef PTRSORT
			if( 0 <= curIdx && curIdx < cItems && lpPtrArr ) current_IFF = lpPtrArr[ curIdx ]->iIFF;
#else
			if( 0 <= curIdx && curIdx < cItems ) current_IFF = ti[ curIdx ].iIFF;
#endif
			RECT rc = {0};
			GetWindowRect( GetDlgItem( hDlg, IDC_FRIEND ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			const int y = ( rc.top + rc.bottom ) / 2 - 9;
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
			//TextOut( hdc, 50, 293, strText, (int) _tcslen( strText ) );
			TextOut( hdc, 50, y, strText, (int) _tcslen( strText ) );

			HPEN hPen = CreatePen( PS_SOLID, 1, RGB( 0x80, 0x80, 0x80 ) );


			HPEN hOldPen = (HPEN) SelectObject( hdc, hPen );
			HBRUSH hBrushIFF = CreateSolidBrush(
								( current_IFF > 0 ) ? FOE_COLOR
								: ( current_IFF < 0 ) ? FRIEND_COLOR
								: RGB( 0xff, 0xff, 0x00 ) );
			
			HBRUSH hOldBrush = (HBRUSH) SelectObject( hdc, hBrushIFF );
			//Ellipse( hdc, 20, 292, 40, 312 );
			Ellipse( hdc, 20, y-1, 40, y+19 );

			HBRUSH hBlueBrush = CreateSolidBrush( RGB( 198, 214, 255 ) );
			//HBRUSH hRedBrush = CreateSolidBrush( RGB( 255, 200, 200 ) );
			SelectBrush( hdc, hBlueBrush );

			GetWindowRect( GetDlgItem( hDlg, IDCANCEL ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			const int x1 = rc.left;
			const int x2 = rc.right - 1;

			GetWindowRect( GetDlgItem( hDlg, IDC_HIDE ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			//const int x1 = rc.left - 16;
			const int y1 = rc.top - 9;
			GetWindowRect( GetDlgItem( hDlg, IDC_SHOW_MANUALLY ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			//const int x2 = rc.right + 16;
			const int y2 = rc.bottom + 9;
			//Rectangle( hdc, 518, 257, 618, 371 );
			Rectangle( hdc, x1, y1, x2, y2 );
			
			/*GetWindowRect( GetDlgItem( hDlg, IDC_NUKE ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			HBRUSH hRedBrush = CreateSolidBrush( RGB( 255, 200, 200 ) );
			SelectBrush( hdc, hRedBrush );
			Rectangle( hdc, x1, rc.top - 8, x2,  rc.bottom + 8 );*/

			SelectPen( hdc, hOldPen );
			DeletePen( hPen );
			
			SelectBrush( hdc, hOldBrush );
			//DeleteBrush( hRedBrush );
			DeleteBrush( hBlueBrush );
			DeleteBrush( hBrushIFF );
			
			SelectFont( hdc, hOldFont );
			
			RestoreDC( hdc, iSaved );

			EndPaint( hDlg, &ps );
			break;
		}		

		case WM_CONTEXTMENU:
		{
			if( GetDlgCtrlID( (HWND) wParam ) == IDC_TARGET_LIST
				&& 0 <= curIdx && curIdx < cItems && SORTEE )
			{
				fContextMenu = true;
#ifdef PTRSORT
				int current_IFF = lpPtrArr[ curIdx ]->iIFF;
#else
				int current_IFF = ti[ curIdx ].iIFF;
#endif
				LV_OnContextMenu( hDlg, (HWND) wParam, lParam, current_IFF, curIdx );
				fContextMenu = false;
			}
			break;
		}
/**
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
**/
		case WM_NCLBUTTONDBLCLK:
			fDebug = ! fDebug;
			return FALSE;
			break;

		case WM_DESTROY:
		{
			g_hListDlg = NULL;
			fCPUTime = false;
			KillTimer( hDlg, TIMER_ID );

			RECT rc = {0};
			GetWindowRect( hDlg, &rc );
			TCHAR str[ 256 ] = _T("");
			_stprintf_s( str, _countof(str), _T("%ld,%ld"), rc.left, rc.top );
			WritePrivateProfileString( TEXT( "Window" ), TEXT( "DialogPos" ), str, GetIniPath() );

			INT arr[ N_COLS ] = {0};
			ListView_GetColumnOrderArray( GetDlgItem( hDlg, IDC_TARGET_LIST ), N_COLS, arr );
			TCHAR * p = str;
			for( ptrdiff_t col = 0, cch; col < N_COLS; ++col )
			{
				cch = _stprintf_s( p, _countof(str) - (p - str), _T("%d,"), arr[ col ] );
				if( cch < 0 ) break;
				p += cch;
			}
			WritePrivateProfileString( TEXT( "Options" ), TEXT( "ColumnOrderArray" ),
											str, GetIniPath() );
			_stprintf_s( str, _countof(str), _T("%d"), sort_algo );
			WritePrivateProfileString( TEXT( "Options" ), TEXT( "SortAlgo" ), 
											str, GetIniPath() );
			WritePrivateProfileString( TEXT( "Options" ), TEXT( "RevSort" ), 
											fRevSort ? TEXT("1"):TEXT("0"), GetIniPath() );



			SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_SLIDER ),
										GWLP_WNDPROC, (LONG_PTR) DefTBProc );
			DefTBProc = NULL;
			SetWindowLongPtr_Floral( GetDlgItem( hDlg, IDC_TARGET_LIST ),
										GWLP_WNDPROC, (LONG_PTR) DefLVProc );
			DefLVProc = NULL;

			if( hfontPercent )
			{
				SendDlgItemMessage( hDlg, IDC_TEXT_PERCENT, WM_SETFONT, 
									(WPARAM) NULL, MAKELPARAM( FALSE, 0 ) );
				DeleteFont( hfontPercent );
				hfontPercent = NULL;
			}

#ifdef PTRSORT
			if( lpPtrArr )
			{
				HeapFree( GetProcessHeap(), 0, lpPtrArr );
				lpPtrArr = NULL;
			}
#endif
			if( ti )
			{
				for( ptrdiff_t i = 0; i < cItems; ++i )
				{
					if( ti[ i ].szPath ) HeapFree( GetProcessHeap(), 0, ti[ i ].szPath );
					if( ti[ i ].szText ) HeapFree( GetProcessHeap(), 0, ti[ i ].szText );
				}
				HeapFree( GetProcessHeap(), 0, ti );
				ti = NULL;
			}
			cItems = 0; // 20140401
			
			UpdateProcessSnap( ti, false, NULL, 0 );
			pTarget = NULL;
			break;
		}
		default:
			return FALSE;
	}
    return TRUE;
}

static void _nuke( HWND hDlg, const PROCESSINFO& target )
{
	if( IsProcessBES( target.dwProcessId, NULL, 0 ) )
	{
		MessageBox( hDlg, target.szPath, TEXT("BES Can't Target BES"), MB_OK | MB_ICONEXCLAMATION );
		return;
	}

	TCHAR strWarning[ 1024 ] = _T("");
	_sntprintf_s( strWarning, _countof(strWarning), _TRUNCATE,
		_T( "Killing the following target:\r\n" )
		_T( "\t%s\r\n" )
		_T( "\tProcess ID %lu\r\n\r\n" )
		_T( "Terminating a process forcefully can cause VERY BAD results" )
		_T( " including loss of data and system instability.\r\n" )
		_T( "Are you sure you want to NUKE this target?\r\n" ),
		target.szPath,
		target.dwProcessId );
	
	if( MessageBox( hDlg, strWarning, _T("WARNING"),
					MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2 ) == IDYES )
	{
		HANDLE hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, target.dwProcessId );
		if( hProcess )
		{
			TerminateProcess( hProcess, 0 );
			CloseHandle( hProcess );
		}
	}
}

/*
ptrdiff_t ListProcessThreads( DWORD dwOwnerPID, DWORD * pThreadIDs, ptrdiff_t numOfMaxIDs ) 
{
	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hThreadSnap == INVALID_HANDLE_VALUE ) return 0;

	THREADENTRY32 te32;
	te32.dwSize = sizeof( THREADENTRY32 ); 

	ptrdiff_t numOfIDs = 0;

	for( BOOL bGoOn = Thread32First( hThreadSnap, &te32 );
			bGoOn && ( numOfIDs < numOfMaxIDs );
			bGoOn = Thread32Next( hThreadSnap, &te32 ) )
	{ 
		if( te32.th32OwnerProcessID == dwOwnerPID )
			pThreadIDs[ numOfIDs++ ] = te32.th32ThreadID;
	}

	CloseHandle( hThreadSnap );
	return numOfIDs;
}
*/
DWORD * AllocThreadList( DWORD dwOwnerPID, ptrdiff_t& numOfThreads ) // v1.7.1
{
	DWORD * lpRet = NULL;
	numOfThreads = 0;
	bool fCachedOperation = false;

	if( WaitForSingleObject( hReconSema, 20 ) == WAIT_OBJECT_0 )
	{
		const ptrdiff_t numOfPairs = s_nSortedPairs;

		PROCESS_THREAD_PAIR * sorted_pairs
			= (PROCESS_THREAD_PAIR *) MemAlloc( sizeof(PROCESS_THREAD_PAIR), (size_t) numOfPairs );
		
		if( sorted_pairs && s_lpSortedPairs && numOfPairs )
		{
			memcpy( sorted_pairs, s_lpSortedPairs, numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
			fCachedOperation = true;
		}
		
		ReleaseSemaphore( hReconSema, 1L, NULL );

		if( fCachedOperation )
		{
			ptrdiff_t i = 0;
			while( i < numOfPairs && sorted_pairs[ i ].pid < dwOwnerPID ){ ++i; }

			while( i + numOfThreads < numOfPairs
					&& sorted_pairs[ i + numOfThreads ].pid == dwOwnerPID ){ ++numOfThreads; }
			
			// numOfThreads may be 0.  Don't return NULL just because numOfThreads==0;
			// doing so would confuse the caller because it would look like out-of-memory.
			lpRet = (DWORD *) MemAlloc( sizeof(DWORD), (size_t) numOfThreads );
			if( lpRet )
			{
				for( ptrdiff_t n = 0; n < numOfThreads; ++n )
					lpRet[ n ] = sorted_pairs[ i + n ].tid;
			}
			else numOfThreads = 0;
		}

		if( sorted_pairs )
			HeapFree( GetProcessHeap(), 0, sorted_pairs );
	}

	if( ! fCachedOperation )
	{
#ifdef _DEBUG
		OutputDebugString(_T("\t###ListProcessThreads_Alloc###\n"));
#endif
		lpRet = ListProcessThreads_Alloc( dwOwnerPID, numOfThreads );
	}

	return lpRet;
}


DWORD * ListProcessThreads_Alloc( DWORD dwOwnerPID, ptrdiff_t& numOfThreads ) // v1.7.0
{
	numOfThreads = 0;

	ptrdiff_t maxNumOfThreads = 256;
	DWORD * lpThreadIDs = (DWORD *) MemAlloc( sizeof(DWORD), (size_t) maxNumOfThreads );
	if( ! lpThreadIDs )
	{
		return NULL;
	}

	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hThreadSnap == INVALID_HANDLE_VALUE )
	{
		MemFree( lpThreadIDs );
		return NULL;
	}

	THREADENTRY32 te32;
	te32.dwSize = sizeof( THREADENTRY32 );

	for( BOOL bGoOn = Thread32First( hThreadSnap, &te32 );
			bGoOn;
			bGoOn = Thread32Next( hThreadSnap, &te32 ) )
	{
		if( te32.th32OwnerProcessID == dwOwnerPID )
		{
			if( numOfThreads == maxNumOfThreads )
			{
				// LONG_MAX = 2^31-1 so 2^30 bytes or 2^28 thread IDs are ok in theory;
				// overflow is possible if the # of IDs is over 2^27 (134217728) before doubled.
				if( 134217728 < maxNumOfThreads ) break;

				maxNumOfThreads *= 2;

				LPVOID lpv = HeapReAlloc( GetProcessHeap(), 0, lpThreadIDs, 
											sizeof(DWORD) * maxNumOfThreads );
				
				if( ! lpv )
				{
					HeapFree( GetProcessHeap(), 0, lpThreadIDs );
					lpThreadIDs = NULL;
					numOfThreads = 0;
					break;
				}

				lpThreadIDs = (DWORD *) lpv;
			}

			lpThreadIDs[ numOfThreads++ ] = te32.th32ThreadID;
		}
	}

	CloseHandle( hThreadSnap );

	return lpThreadIDs;
}

CLEAN_POINTER PROCESS_THREAD_PAIR * AllocSortedPairs(
	ptrdiff_t& numOfPairs,
	ptrdiff_t prev_num )
{
	numOfPairs = 0;

	ptrdiff_t maxNumOfPairs = (prev_num) ? prev_num + 128 : 2048 ;

	PROCESS_THREAD_PAIR * lpPairs
		= (PROCESS_THREAD_PAIR *) MemAlloc( sizeof(PROCESS_THREAD_PAIR), (size_t) maxNumOfPairs );

	if( ! lpPairs )	return NULL;

	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hThreadSnap == INVALID_HANDLE_VALUE )
	{
		HeapFree( GetProcessHeap(), 0, lpPairs );
		return NULL;
	}

	THREADENTRY32 te32;
	te32.dwSize = (DWORD) sizeof(THREADENTRY32); 
	for( BOOL bGoOn = Thread32First( hThreadSnap, &te32 ); 
			bGoOn;
			bGoOn = Thread32Next( hThreadSnap, &te32 ) )
	{
		if( numOfPairs == maxNumOfPairs )
		{
			// LONG_MAX = 2^31-1 so 2^30 bytes or 2^27 pairs are ok in theory;
			// overflow is possible if the # of pairs is over 2^26 (67108864) before doubled.
			if( 67108864 < maxNumOfPairs ) break;

			maxNumOfPairs *= 2;

			LPVOID lpv = HeapReAlloc( GetProcessHeap(), 0, lpPairs,
										sizeof(PROCESS_THREAD_PAIR) * maxNumOfPairs );

			if( ! lpv )
			{
				numOfPairs = 0;
				HeapFree( GetProcessHeap(), 0L, lpPairs );
				lpPairs = NULL; // @20140308
				break;
			}
			
			lpPairs = (PROCESS_THREAD_PAIR *) lpv;
		}
	
		lpPairs[ numOfPairs ].tid = te32.th32ThreadID;
		lpPairs[ numOfPairs ].pid = te32.th32OwnerProcessID;
		//memcpy( lpPairs + numOfPairs, &te32.th32ThreadID, sizeof(DWORD) * 2 );
		
		++numOfPairs;

	}

	CloseHandle( hThreadSnap );
	
	if( lpPairs )
	{
		qsort( lpPairs, (size_t) numOfPairs, sizeof(PROCESS_THREAD_PAIR), &pair_comp ); 
	}
	
#if 0//def _DEBUG
	TCHAR s[100];
	_stprintf_s(s,100,_T("0x%x (AllocSortedPairs) %d->%d pairs (max %d) %p\n"),
		GetCurrentThreadId(),
		prev_num,
		numOfPairs,
		maxNumOfPairs,
		lpPairs);
	OutputDebugString(s);
#endif

	return lpPairs;
}


static BOOL CALLBACK EnumThreadWndProc_DetectBES( HWND hwnd, LPARAM lParam )
{
	TCHAR strClass[ 32 ]; // truncated if too long

	// #define APP_CLASS TEXT( "BATTLEENC" ) <-- 9 cch
	if( GetClassName( hwnd, strClass, 32 ) == 9 && ! _tcscmp( strClass, APP_CLASS ) )
	{
		*((BOOL*) lParam) = TRUE;
		return FALSE;
	}
	return TRUE;
}

BOOL IsProcessBES( DWORD dwProcessID, const PROCESS_THREAD_PAIR * sorted_pairs, ptrdiff_t numOfPairs )
{
	if( dwProcessID == 0 || dwProcessID == (DWORD) -1 ) // trivially false
		return FALSE;
	
	if( dwProcessID == GetCurrentProcessId() ) // this instance itself
		return TRUE;

	BOOL bThisIsBES = FALSE; // another instance of BES
	
	if( sorted_pairs )
	{
		ptrdiff_t sx = 0;
		ptrdiff_t base = 0;
		while( sx < numOfPairs && sorted_pairs[ sx ].pid < dwProcessID ) ++sx;
		base = sx;
		while( sx < numOfPairs && sorted_pairs[ sx ].pid == dwProcessID ) ++sx;

		ptrdiff_t numOfThreads = sx - base;
		const PROCESS_THREAD_PAIR * pairs = sorted_pairs + base;
		for( ptrdiff_t i = 0; i < numOfThreads && ! bThisIsBES; ++i )
			EnumThreadWindows( pairs[ i ].tid, &EnumThreadWndProc_DetectBES, (LPARAM) &bThisIsBES );
	}
	else
	{
		ptrdiff_t numOfThreads = 0;
		DWORD * lpdwTID = ListProcessThreads_Alloc( dwProcessID, numOfThreads );
		if( lpdwTID )
		{
			for( ptrdiff_t i = 0; i < numOfThreads && ! bThisIsBES; ++i )
				EnumThreadWindows( lpdwTID[ i ], &EnumThreadWndProc_DetectBES, (LPARAM) &bThisIsBES );

			HeapFree( GetProcessHeap(), 0, lpdwTID );
		}
	}

	return bThisIsBES;
}

static INT_PTR CALLBACK Question( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static PROCESSINFO * lpTarget;

	switch ( message )
	{
		case WM_INITDIALOG:
		{
			TCHAR msg1[ 1024 ] = _T("");
			TCHAR msg2[ 4096 ] = _T("");
			TCHAR format[ 1024 ] = _T("");

			lpTarget = (PROCESSINFO*) lParam;

			if( ! lpTarget )
			{
				EndDialog( hDlg, FALSE );
				break;
			}

			TCHAR szWindowText[ MAX_WINDOWTEXT ] = _T("");
#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					IS_JAPANESEo ? S_JPNo_1001 : S_JPN_1001,
					-1, szWindowText, MAX_WINDOWTEXT );
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
					S_FIN_1001, -1, szWindowText, MAX_WINDOWTEXT );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_FIN : S_QUESTION1_FIN,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1001, -1, szWindowText, MAX_WINDOWTEXT );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_SPA : S_QUESTION1_SPA,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_CHINESE_T )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1001T, -1, szWindowText, MAX_WINDOWTEXT );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_CHIT : S_QUESTION1_CHIT,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_CHINESE_S )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_CHI_1001S, -1, szWindowText, MAX_WINDOWTEXT );
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					lpTarget->fWatch ? S_QUESTION2_CHIS : S_QUESTION1_CHIS,
					-1, msg1, 1023
				);
				_tcscpy_s( format, _countof(format), _T( S_FORMAT2_ASC ) );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1001, -1, szWindowText, MAX_WINDOWTEXT );
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
				_tcscpy_s( szWindowText, MAX_WINDOWTEXT, _T( "Confirmation" ) );
			}
			SetWindowText( hDlg, szWindowText );

			_stprintf_s( msg2, _countof(msg2), format,
				lpTarget->szPath,
				lpTarget->dwProcessId, lpTarget->dwProcessId,
				lpTarget->szText ? lpTarget->szText : _T( "n/a" )
			);

			SendDlgItemMessage( hDlg, IDC_MSG1, WM_SETFONT, (WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );

			SetDlgItemText( hDlg, IDC_MSG1, msg1 );
			SetDlgItemText( hDlg, IDC_EDIT, msg2 );
#ifdef _DEBUG
			OutputDebugString(msg1);
#endif

#if DEF_MODE == 0
# define DEF_MODE_RADIO IDC_RADIO_MODE0
#else
# define DEF_MODE_RADIO IDC_RADIO_MODE1
#endif
			CheckRadioButton( hDlg, IDC_RADIO_MODE0, IDC_RADIO_MODE1, DEF_MODE_RADIO );
			CheckRadioButton( hDlg, IDC_RADIO_LIMIT, IDC_RADIO_WATCH, lpTarget->fWatch ? IDC_RADIO_WATCH : IDC_RADIO_LIMIT );

			PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
			break;
		}

		case WM_COMMAND:
		{
			switch( LOWORD( wParam ) )
			{
				case IDOK:
				{
					int mode = ( IsDlgButtonChecked( hDlg, IDC_RADIO_MODE0 ) ) ? 0 : 1 ;
					BOOL bWatch = ( IsDlgButtonChecked( hDlg, IDC_RADIO_WATCH ) ) ? TRUE : FALSE;

					// LOWORD=2 if mode1, =1 if mode0
					EndDialog( hDlg, MAKELONG( mode + 1, bWatch ) );
					break;
				}
				case IDCANCEL:
					EndDialog( hDlg, 0 );
					break;
				default:
					break;
			}
			break;
		}
/**
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
**/
		case WM_DESTROY:
			/**if( lpSavedWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpSavedWindowText );
				lpSavedWindowText = NULL;
			}**/
			break;

		default:
			return FALSE;
			break;
	}
    return TRUE;
}


bool IsAbsFoe( LPCTSTR strPath )
{
	if( ! strPath ) {
		MessageBox( NULL, _T("DEBUG ME"), _T("IsAbsFoe"), MB_OK );
		return false;
	}
	const TCHAR * pntr = _tcsrchr( strPath, _T('\\') );
	return IsAbsFoe2( (pntr && pntr[ 1 ]) ? pntr + 1 : strPath, strPath );
}

static bool IsAbsFoe2( const TCHAR * pExe, const TCHAR * szPath )
{
	return
			(
				_tcsicmp( _T( "lame.exe" ), pExe ) == 0
					||
				_tcsicmp( _T( "aviutl.exe" ), pExe ) == 0
					||
				_tcsicmp( _T( "virtualdub.exe" ), pExe ) == 0
		//			||
		//		_tcsicmp( _T( "virtualdubmod.exe" ), pExe ) == 0
		//			||
		//		_tcsicmp( _T( "ffmpeg.exe" ), pExe ) == 0
					||
				_tcsstr( szPath, _T( "VirtualDub" )  ) != NULL
					||
				_tcsncmp( pExe, _T("x264"), 4 ) == 0
		);
#if 0
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
#endif
}

static BOOL CALLBACK GetImportantText_EnumProc( HWND hwnd, LPARAM lParam )
{
	TCHAR szWindowText[ MAX_WINDOWTEXT ];

	// assume that a longer text is more important
	DWORD_PTR importanceOfThisText
				= (DWORD_PTR) GetWindowText( hwnd, szWindowText, MAX_WINDOWTEXT );
	if( importanceOfThisText )
	{
		if( _tcsicmp( szWindowText, _T("Default IME") ) != 0 && lParam )
		{
			if( IsWindowVisible( hwnd ) ) importanceOfThisText <<= 7; //*= 128;
			else if( IsIconic( hwnd ) ) importanceOfThisText <<= 6;   //*= 64;

			// if this guy is more important, update the record
			IMPORTANT_TEXT& record = *(IMPORTANT_TEXT*) lParam;
			if( record.dwImportance < importanceOfThisText )
			{
				// both dst and src: max-cch=MAX_WINDOWTEXT
				_tcscpy_s( record.pszText, MAX_WINDOWTEXT, szWindowText );
				record.dwImportance = importanceOfThisText;
			}
		}
	}

	return TRUE;
}


void CachedList_Refresh( DWORD msMaxWait )
{
#ifdef _DEBUG
LARGE_INTEGER li0;
QueryPerformanceCounter(&li0);
#endif

	// Get the newest info into the local variable.
	// Getting the result directly into s_lpSortedPairs is also doable, but then it must be
	// sema-protected, and the sema will be held for like 10 ms because AllocSortedPairs is slow;
	// which means that other threads may have to wait for 10 ms; it's not a good idea.
	// We get the result first into the local variable, and then, while sema-protected,
	// update s_lpSortedPairs (that's very quick; it's just memcpy).
	ptrdiff_t numOfPairs = 0;
	PROCESS_THREAD_PAIR * sorted_pairs = AllocSortedPairs( numOfPairs, s_nSortedPairs );

	// Wait until we can read/write
	if( WaitForSingleObject( hReconSema, msMaxWait ) == WAIT_OBJECT_0 )
	{
		// Free the old memory block
		if( s_lpSortedPairs )
			HeapFree( GetProcessHeap(), 0, s_lpSortedPairs );

		// Create a new one
		s_lpSortedPairs
			= (PROCESS_THREAD_PAIR *) MemAlloc( sizeof(PROCESS_THREAD_PAIR), (size_t) numOfPairs );

		if( s_lpSortedPairs && sorted_pairs )
		{
			memcpy( s_lpSortedPairs, sorted_pairs, sizeof(PROCESS_THREAD_PAIR) * numOfPairs );
			s_nSortedPairs = numOfPairs;
		}
		else
		{
			s_nSortedPairs = 0;
		}

		ReleaseSemaphore( hReconSema, 1L, NULL );

#ifdef _DEBUG
		OutputDebugString(_T("CachedList_Refresh\n"));
#endif
	}

	// Free the local list
	if( sorted_pairs )
		HeapFree( GetProcessHeap(), 0, sorted_pairs );

#ifdef _DEBUG
LARGE_INTEGER li;
QueryPerformanceCounter(&li);
LARGE_INTEGER freq;
QueryPerformanceFrequency( &freq );
TCHAR s[100];
SYSTEMTIME st;GetSystemTime(&st);
_stprintf_s(s,100,
			_T("%02d:%02d:%02d.%03d Refresh: %.2f ms\n"),
			st.wHour,st.wMinute,st.wSecond,st.wMilliseconds,
(double)(li.QuadPart-li0.QuadPart)/freq.QuadPart*1000.0);
OutputDebugString(s);
#endif
}

static bool TheKeysAreDown( void )
{
	// Ctrl+NumLock+Num3
	return ( (unsigned int)(USHORT) GetAsyncKeyState( VK_PAUSE ) & 0x8000 ) 
		&&
	( (unsigned int)(USHORT) GetAsyncKeyState( VK_NUMPAD3 ) & 0x8000 )
		&&
	( (unsigned int)(USHORT) GetAsyncKeyState( VK_CONTROL ) & 0x8000 );
}

// This guy destroys the cache at the end. So don't use the cache operation unless
// you start this thread (if you do, operation itself will be ok, but the memory block
// is not freed).
unsigned __stdcall ReconThread( void * pv )
{
	Sleep( 250 ); // start slowly
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL );

	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

	HANDLE hEvent_Exiting = (HANDLE) pv;

	WriteDebugLog(_T("Recon thread started"));
	HANDLE hToken = NULL;
	DWORD dwPrevAttributes = 0;
	BOOL bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	while( ( WaitForSingleObject( hEvent_Exiting, 10000 ) ) == WAIT_TIMEOUT ) // 10 secs
	{
		if( TheKeysAreDown() )
		{
			for( ptrdiff_t e = 0; e < MAX_SLOTS; ++e )
			{
				g_bHack[ e ] = FALSE;
				if( hChildThread[ e ] )
				{
					CloseHandle( hChildThread[ e ] );
					hChildThread[ e ] = NULL;
				}
				ReleaseSemaphore( hSemaphore[ e ], 1L, NULL );
			}
			MessageBox(NULL,_T("Ctrl+MumLock+Num3 detected!"),APP_NAME,MB_OK|MB_TOPMOST);
		}

		ptrdiff_t i = 0;
		for( ; i < MAX_SLOTS; ++i ) { if( g_bHack[ i ] ) break; }

		if( i < MAX_SLOTS )
		{
#ifdef _DEBUG
WriteDebugLog(_T("(Recon)"));
#endif
			CachedList_Refresh( 1000 );
		}
	}

	WaitForSingleObject( hReconSema, 3000 );
	s_nSortedPairs = 0;
	if( s_lpSortedPairs )
	{
		HeapFree( GetProcessHeap(), 0, s_lpSortedPairs );
		s_lpSortedPairs = NULL;
	}

	extern PATHINFO g_arPathInfo[ MAX_WATCH ];
	extern ptrdiff_t g_nPathInfo;

	for( ptrdiff_t n = 0; n < MAX_WATCH; ++n )
	{
		if( g_arPathInfo[ n ].pszPath )
		{
			TcharFree( g_arPathInfo[ n ].pszPath );
			g_arPathInfo[ n ].pszPath = NULL;
		}
	}

	g_nPathInfo = 0; // TODO: is this ok?

	ReleaseSemaphore( hReconSema, 1L, NULL );

	if( hToken )
	{
		if( bPriv ) 
			AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}

#ifdef _DEBUG
	return 7;
#else
	return 0;
#endif
}

bool ProcessExists( DWORD dwPID )
{
	CachedList_Refresh( 1000 );

	bool fRetVal = false;
	PROCESS_THREAD_PAIR * sorted_pairs = NULL;
	bool fCachedOperation = false;
	ptrdiff_t numOfPairs = 0;
	if( WaitForSingleObject( hReconSema, 100 ) == WAIT_OBJECT_0 )
	{
		numOfPairs = s_nSortedPairs;
		sorted_pairs = (PROCESS_THREAD_PAIR*) HeapAlloc( GetProcessHeap(), 0, 
														numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
		
		if( sorted_pairs && s_lpSortedPairs && numOfPairs )
		{
			memcpy( sorted_pairs, s_lpSortedPairs, numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
			fCachedOperation = true;
		}
		ReleaseSemaphore( hReconSema, 1L, NULL );
	}

	if( fCachedOperation )
	{
		ptrdiff_t i = 0;
		while( i < numOfPairs && sorted_pairs[ i ].pid < dwPID ){ ++i; }
		fRetVal = ( i < numOfPairs && sorted_pairs[ i ].pid == dwPID );
	}

	if( sorted_pairs )
		HeapFree( GetProcessHeap(), 0, sorted_pairs );

	return fRetVal;
}

#if 0
DWORD PathToProcessID_Cached(
	const PATHINFO * arPathInfo,
	ptrdiff_t nPathInfo,
	const DWORD * pdwExcluded,
	ptrdiff_t nExcluded,
	TCHAR ** ppPath,
	ptrdiff_t * px )
{
	if( ppPath ) *ppPath = NULL;
	if( px ) *px = -1;

	PROCESS_THREAD_PAIR * sorted_pairs = NULL;
	bool fCachedOperation = false;
	ptrdiff_t numOfPairs = 0;
	if( WaitForSingleObject( hReconSema, 100 ) == WAIT_OBJECT_0 )
	{
		numOfPairs = s_nSortedPairs;
		sorted_pairs = (PROCESS_THREAD_PAIR*) HeapAlloc( GetProcessHeap(), 0, 
														numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
		
		if( sorted_pairs && s_lpSortedPairs && numOfPairs )
		{
			memcpy( sorted_pairs, s_lpSortedPairs, numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
			fCachedOperation = true;
		}
		ReleaseSemaphore( hReconSema, 1L, NULL );
	}

	if( ! fCachedOperation )
	{
		if( sorted_pairs ) HeapFree( GetProcessHeap(), 0, sorted_pairs );
		return PathToProcessID( arPathInfo, nPathInfo, pdwExcluded, nExcluded, ppPath, px );
	}

	DWORD pid = 0;
	ptrdiff_t found_index = -1;
	for( ptrdiff_t i = 0; i < numOfPairs && found_index == -1; ++i )
	{
		if( sorted_pairs[ i ].pid == 0 || sorted_pairs[ i ].pid == pid )
			continue;

		pid = sorted_pairs[ i ].pid;

		if( pdwExcluded )
		{
			ptrdiff_t n = 0;
			for( ; n < nExcluded; ++n ){ if( pdwExcluded[ n ] == pid ) break; }
			if( n < nExcluded ) continue;
		}

		found_index = pid_has_this_path( pid, arPathInfo, nPathInfo, ppPath );
	}

	HeapFree( GetProcessHeap(), 0, sorted_pairs );

	if( found_index == -1 ) return (DWORD) -1;

	if( px ) *px = found_index;
	return pid;
}
#endif

bool PathEqual( const PATHINFO& pathInfo, const TCHAR * lpPath, size_t cch, bool fSafe )
{
	if( ! pathInfo.pszPath ) return false;

	bool fRet = false;
	if( pathInfo.cchHead || pathInfo.cchTail )
	{
		if( pathInfo.cchHead + pathInfo.cchTail <= cch
			&& ! _tcsncicmp( pathInfo.pszPath, lpPath, pathInfo.cchHead ) )
		{
			fRet = StrEqualNoCase( &pathInfo.pszPath[ pathInfo.cchHead + 1 ], 
									&lpPath[ cch - pathInfo.cchTail ] );
		}
#if 1
		if( fRet && fSafe ) // if (fSafe): No wildcard matching if the target (lpPath) is in the Windows Directory or the System Directory
		{
			TCHAR szWinDir[ MAX_PATH ] = _T("");
			size_t cchWinDir = GetWindowsDirectory( szWinDir, MAX_PATH );
			TCHAR szSysDir[ MAX_PATH ] = _T("");
			size_t cchSysDir = GetSystemDirectory( szSysDir, MAX_PATH );
			if( ( cchWinDir && cchWinDir < MAX_PATH && ! _tcsncicmp( lpPath, szWinDir, cchWinDir ) )
				||
				( cchSysDir && cchSysDir < MAX_PATH && ! _tcsncicmp( lpPath, szSysDir, cchSysDir ) )
			) fRet = false;
		}
#endif
	}
	else
	{
		fRet = ( cch == pathInfo.cchPath && StrEqualNoCase( pathInfo.pszPath, lpPath ) );
	}

	return fRet;
}

static ptrdiff_t pid_has_this_path(
	DWORD pid,
	const PATHINFO * arPathInfo,
	ptrdiff_t nPathInfo,
	TCHAR ** ppPath )
{
	ptrdiff_t ret = -1;
	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, pid );
	if( hProcess )
	{
		size_t cch;
		TCHAR * lpPath = ProcessToPath_Alloc( hProcess, &cch );
		if( lpPath )
		{
			for( ptrdiff_t n = 0; n < nPathInfo && ret == -1; ++n )
			{
				if( PathEqual( arPathInfo[ n ], lpPath, cch, true ) ) ret = n;
			}

			if( ret != -1 && ppPath )
				*ppPath = lpPath;
			else
				HeapFree( GetProcessHeap(), 0, lpPath );
		}
		CloseHandle( hProcess );
	}
	return ret;
}

CLEAN_POINTER PROCESS_THREAD_PAIR * GetCachedPairs( DWORD msWait, ptrdiff_t& numOfPairs )
{
	PROCESS_THREAD_PAIR * sorted_pairs = NULL;
	numOfPairs = 0;
	if( WaitForSingleObject( hReconSema, msWait ) == WAIT_OBJECT_0 )
	{
		sorted_pairs = (PROCESS_THREAD_PAIR*)
						MemAlloc( sizeof(PROCESS_THREAD_PAIR), (size_t) s_nSortedPairs );

		if( sorted_pairs && s_lpSortedPairs && s_nSortedPairs )
		{
			memcpy( sorted_pairs, s_lpSortedPairs, sizeof(PROCESS_THREAD_PAIR) * (size_t) s_nSortedPairs );
			numOfPairs = s_nSortedPairs;
		}

		ReleaseSemaphore( hReconSema, 1L, NULL );
	}

	return sorted_pairs;
}

DWORD ParsePID( const TCHAR * strTarget ) // v1.7.5
{
	// Do nothing if it's not starting with "pid:"
	if( _tcsncmp( strTarget, _T("pid:"), 4 ) != 0 ) return 0UL;

	const TCHAR * strNumber = strTarget + 4;

	DWORD pid = 0UL;

	// OK if it does not contain "-" (i.e. a negative number is bad)
	if( _tcschr( strNumber, _T('-') ) == NULL )
	{
		// "0x" or "0X" will be parsed as a hex
		int radix = ( _tcschr( strNumber, _T('x') ) || _tcschr( strNumber, _T('X') ) ) ? 16 : 10 ;

		TCHAR * endptr;

		_set_errno( 0 );
		pid = _tcstoul( strNumber, &endptr, radix );
		if( errno == ERANGE ) pid = 0UL; // Not OK if overflows
		else
		{
#ifdef _UNICODE
			while( iswspace( *endptr ) ) ++endptr; // skip the trailing spaces if any
#else
			while( isspace( (unsigned char) *endptr ) ) ++endptr; // skip the trailing spaces if any
#endif
			if( *endptr != _T('\0') ) pid = 0UL; // Not OK if there is an unexpected char
		}
	}

	if( ! pid )
	{
		TCHAR str[ 128 ] = _T("");
		_sntprintf_s( str, _countof(str), _TRUNCATE, _T( "[ERROR] couldn't parse \"%s\"" ), strTarget );
		WriteDebugLog( str );
	}

	return pid;
}

TCHAR * PIDToPath_Alloc( DWORD dwProcessID, size_t * pcch ) // v1.7.5
{
	if( ! dwProcessID ) return NULL;

	TCHAR * lpPath = NULL;

	HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION, FALSE, dwProcessID );

	if( hProcess )
	{
		size_t cch;
		lpPath = ProcessToPath_Alloc( hProcess, &cch );
		if( pcch ) *pcch = cch;

		CloseHandle( hProcess );
	}
	else
	{
		TCHAR str[ 128 ] = _T("");
		_stprintf_s( str, _countof(str), _T( "[ERROR] OpenProcess failed!  ProcessID=%lu" ), dwProcessID );
		WriteDebugLog( str );
	}

	return lpPath;
}

BOOL LimitProcess_NoWatch( HWND hDlg, TARGETINFO * pTarget, PROCESSINFO& ProcessInfo, const int * aiParams, int mode ) // v1.7.5
{
	ptrdiff_t iMyId;

	for( iMyId = 0; iMyId < MAX_SLOTS; ++iMyId )
	{
		if( iMyId == 3 ) continue;
		if( ! g_bHack[ iMyId ] && WaitForSingleObject( hSemaphore[ iMyId ], 100 ) == WAIT_OBJECT_0 ) break;
	}

	if( iMyId == MAX_SLOTS )
	{
		MessageBox( hDlg, TEXT("Semaphore Error"), APP_NAME, MB_OK | MB_ICONSTOP );
		return FALSE;
	}

	if( ! TiFromPi( pTarget[ iMyId ], ProcessInfo ) )
	{
		ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
		return FALSE;
	}

	if( aiParams )
	{
		if( aiParams[ 0 ] != -1 ) pTarget[ iMyId ].wCycle = (WORD) aiParams[ 0 ];
		if( aiParams[ 1 ] != -1 ) pTarget[ iMyId ].wDelay = (WORD) aiParams[ 1 ];
	}
	pTarget[ iMyId ].mode = (WORD) mode;

	g_Slider[ iMyId ] = GetSliderIni( ProcessInfo.szPath, g_hWnd );

	SendMessage( g_hWnd, WM_USER_HACK, (WPARAM) iMyId, (LPARAM) &pTarget[ iMyId ] );

	return TRUE;
}

BOOL LimitPID( HWND hWnd, TARGETINFO * pTarget, const DWORD pid, int iSlider, BOOL bUnlimit, const int * aiParams ) // v1.7.5
{
	if( ! pid ) return FALSE;

	ptrdiff_t g = 0;  // if the target is new, g = MAX_SLOTS
	while( g < MAX_SLOTS && ( g == 3 || pid != pTarget[ g ].dwProcessId ) ) ++g;

	if( g < MAX_SLOTS )
	{
		// update param(s)
		if( SLIDER_MIN <= iSlider && iSlider <= SLIDER_MAX ) g_Slider[ g ] = iSlider;

		if( aiParams )
		{
			if( aiParams[ 0 ] != -1 ) pTarget[ g ].wCycle = (WORD) aiParams[ 0 ];
			if( aiParams[ 1 ] != -1 ) pTarget[ g ].wDelay = (WORD) aiParams[ 1 ];
		}

		if( bUnlimit )
		{
			if( g_bHack[ g ] )
			{
				g_bHack[ g ] = FALSE;
				SendMessage( hWnd, WM_USER_STOP, (WPARAM) g, STOPF_UNLIMIT );
			}
		}
		else if( ! g_bHack[ g ] ) // relimit
		{
			SendMessage( hWnd, WM_USER_RESTART, (WPARAM) g, 0 );
		}

		return TRUE;
	}

	PROCESSINFO pi;
	ZeroMemory( &pi, sizeof(PROCESSINFO) );

	pi.dwProcessId = pid;
	pi.szPath = PIDToPath_Alloc( pid, NULL );
	if( ! pi.szPath ) return FALSE;

	SetSliderIni( pi.szPath, iSlider );

	BOOL bRet = LimitProcess_NoWatch( hWnd, pTarget, pi, aiParams );

	MemFree( pi.szPath );

	return bRet;
}