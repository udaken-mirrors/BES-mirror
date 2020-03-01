#include "list.h"

static BOOL BES_ShowWindow( HWND hDlg, HWND hwnd, int iShow )
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
		RECT rect;
		GetWindowRect( hwnd, &rect );
		RECT area;
//		SystemParametersInfoW( SPI_GETWORKAREA, 0, &area, 0 );
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

	return TRUE;
}


static bool show_process_win_worker(
	HWND hDlg,
	const DWORD * pTIDs,
	ptrdiff_t numOfThreads,
	WININFO * pWinInfo,
	int iShow )
{
	bool ret = false;

	// It would be more efficient if we call BES_ShowWindow for each window directly from
	// EnumProc, instead of once storing every window in a big array; however, BES_ShowWindow
	// may show MessageBox and waiting long, while which the target windows may change,
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
			if( ! BES_ShowWindow( hDlg, pWinInfo->hwnd[ n ], iShow ) )
				return true;
		}
	}

	return  ret;
}

static void ShowProcessWindow( HWND hDlg, DWORD dwProcessID, int iShow )
{
	WININFO * lpWinInfo = (WININFO*) HeapAlloc( GetProcessHeap(), 0, sizeof(WININFO) );
	if( lpWinInfo )
	{
		ptrdiff_t numOfThreads = 0;
		DWORD * pTIDs = ListProcessThreads_Alloc( dwProcessID, numOfThreads );
		if( pTIDs )
		{
			if( ! show_process_win_worker( hDlg, pTIDs, numOfThreads, lpWinInfo, iShow ) )
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

#if 1//def _UNICODE // ANSIFIX@20140307(4/5)
	TCHAR szTargetLongPath[ CCHPATH ] = _T("");

	DWORD dwResult1 = GetLongPathName( pPath, szTargetLongPath, CCHPATH );
	if( dwResult1 == 0UL || CCHPATH <= dwResult1 )
	{
		// Use it as it is if GetLongPathName failed
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
#if 1//def _UNICODE // ANSIFIX@20140307(5/5)
			TCHAR szPath[ MAX_PATH * 2 ] = _T("");
//+ 1.1 b5 -------------------------------------------------------------------
			if( ProcessToPath( hProcess, szPath, MAX_PATH * 2 )
				&&
				Lstrcmpi( szTargetLongPath, szPath ) == 0 )
			{
				CloseHandle( hProcess );
				CloseHandle( hProcessSnap );
				return pe32.th32ProcessID;
			}
// ---------------------------------------------------------------------------
#else
			if( Lstrcmpi( lpszTargetExe, pe32.szExeFile ) == 0 )
			{
				CloseHandle( hProcess );
				CloseHandle( hProcessSnap );
				return pe32.th32ProcessID;
			}

			// fixed @ 1.1b5
			if( bLongExeName )
			{
				if( _strnicmp( lpszTargetExe, pe32.szExeFile, 15 ) == 0 )
				{
					CloseHandle( hProcess );
					CloseHandle( hProcessSnap );
					return pe32.th32ProcessID;
				}
			}
#endif
			CloseHandle( hProcess );
		}
	} while( Process32Next( hProcessSnap, &pe32 ) );

	CloseHandle( hProcessSnap );

	return (DWORD) -1;
}

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

static BOOL ProcessToPath( HANDLE hProcess, TCHAR * szPath, DWORD cchPath )
{
	if( ! szPath )
		return FALSE;

	TCHAR strFilePath[ MAX_PATH * 2 ] = _T("");

	if( GetModuleFileNameEx( hProcess, NULL, strFilePath, _countof(strFilePath) )
			&& strFilePath[ 0 ] != _T('\\')
		||
		g_pfnGetProcessImageFileName != NULL
			&& g_pfnGetProcessImageFileName( hProcess, strFilePath, (DWORD) _countof(strFilePath) )
			&& DevicePathToDosPath( strFilePath, (DWORD) _countof(strFilePath) )
	)
	{
		DWORD dwResult = GetLongPathName( strFilePath, szPath, cchPath );
		// Use whatever we have now if GetLongPathName failed
		if( ! dwResult || cchPath <= dwResult )
		{
#ifndef _UNICODE
			// strFilePath doesn't exist, probably because the actual path contains
			// non-ACP characters and can not be expressed w/o Unicode
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

	CloseHandle( hProcessSnap );

	GetSystemTimeAsFileTime( &ft );
	ULONGLONG rt1 = (ULONGLONG) ft.dwHighDateTime << 32 | (ULONGLONG) ft.dwLowDateTime;
	double cost = ( rt1 - rt0 ) / 10000000.0;
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

static void LV_OnContextMenu( HWND hDlg, HWND hLV, LPARAM lParam, int iff )
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

	const LRESULT Sel = ListView_GetCurrentSel( hLV );
	if( Sel < 0 ) return;
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

		DestroyMenu( hMenu );

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
static bool _is_list_button_def( HWND hDlg )
{
	TCHAR strBtn[ 32 ] = _T("");
	GetDlgItemText( hDlg, IDC_LISTALL_SYS, strBtn, 32 );
	return ( _tcscmp( strBtn, strDefListBtn ) == 0 );
}


INT_PTR CALLBACK xList( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
#define INIT_MAX_ITEMS (64)
	static ptrdiff_t maxItems = INIT_MAX_ITEMS;     // remember as static if maxItems is increased
	static ptrdiff_t cItems = 0;
	static TARGETINFO * ti = NULL;       // maxItems
	static ptrdiff_t * MetaIndex = NULL; // maxItems
	static LPHACK_PARAMS lphp;

	static TCHAR * lpSavedWindowText = NULL;
	static HFONT hfontPercent = NULL;
	//static HBRUSH hListBoxBrush = NULL;
	static int hot[ 3 ];
	static int current_IFF = 0;
	static int sort_algo = IFF;
	static bool fRevSort = false;
	static bool fQuickTimer = false;
	static bool fReloading = false;

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
			fReloading = false;

			lphp = (LPHACK_PARAMS) lParam;

			if( maxItems < INIT_MAX_ITEMS ) maxItems = INIT_MAX_ITEMS; // this should never happen
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


			//if( g_bHack[ 2 ] || g_bHack[ 3 ] )
			/*if( g_bHack[ 3 ] )
			{
				EnableWindow( GetDlgItem( hDlg, IDC_WATCH ), FALSE );
			}*/
			hot[0]=hot[1]=hot[2]=-1;

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

			SetWindowText( GetDlgItem( hDlg, IDC_LISTALL_SYS ), 
							_always_list_all() ? strNondefListBtn : strDefListBtn );

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
				&& ! fReloading
				&& ! fContextMenu
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
			if( ! ti || ! MetaIndex ) break;

			fReloading = true;
#if 1//def _DEBUG
	LARGE_INTEGER li0,li;
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
#ifdef _DEBUG
//			TCHAR s[100];swprintf_s(s,100,L"dy=%d iTop=%d\n",dy,iTopIndex);
//			OutputDebugString(s);
#endif
			
			
			if( wParam == (WPARAM) -1 )
			{
				ptrdiff_t sel = ListView_GetCurrentSel( hwndList );
			
				if( sel < 0 || cItems <= sel )
				{
					sel = 0;
				}

				wParam = ti[ MetaIndex[ sel ] ].dwProcessId;
			}

			const int listLevel = _is_list_button_def( hDlg ) ? 0 : LIST_SYSTEM_PROCESS;

			const ptrdiff_t prev_maxItems = maxItems;
			int numOfPairs;
			cItems = UpdateProcessSnap( &ti, &maxItems, listLevel, &numOfPairs );

			if( prev_maxItems < maxItems )
			{
				LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 
											MetaIndex, sizeof(ptrdiff_t) * maxItems );
				if( ! lpv )
				{
					HeapFree( GetProcessHeap(), 0, MetaIndex );
					MetaIndex = NULL;
					EndDialog( hDlg, XLIST_CANCELED );
					fReloading = false;
					break;
				}

				MetaIndex = (ptrdiff_t*) lpv;
			}

			if( fRevSort )
			{
				if( sort_algo == CPU )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_CPU2 );
				else if( sort_algo == EXE )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_EXE2 );
				else if( sort_algo == TITLE )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_TITLE2 );
				else if( sort_algo == PATH )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_PATH2 );
				else if( sort_algo == THREADS )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_THREADS2 );
				else
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_PID2 );
			}
			else
			{
				if( sort_algo == CPU )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_CPU );
				else if( sort_algo == EXE )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_EXE );
				else if( sort_algo == TITLE )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_TITLE );
				else if( sort_algo == PATH )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_PATH );
				else if( sort_algo == THREADS )
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_THREADS );
				else
					qsort( ti, (size_t) cItems, sizeof(TARGETINFO), &target_comp_PID );
			}

			ptrdiff_t index = 0;
			ptrdiff_t m = 0;
			ptrdiff_t cursel = 0;
			const DWORD dwTargetId = (DWORD) wParam;

			SetWindowRedraw( hwndList, FALSE );

			ListView_DeleteAllItems( hwndList );

			if( sort_algo == IFF )
			{
				if( fRevSort )
				{
					for( int iff = IFF_SYSTEM; iff <= IFF_ABS_FOE; ++iff )
					{
						for( index = 0; index < cItems; ++index )
						{
							if( ti[ index ].iIFF == iff )
							{
								MetaIndex[ m ] = index;
								_add_item( hwndList, iff, ti[ index ] );
								
								if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
									cursel = m;
								
								++m;
							}
						}
					}
				}
				else
				{
					for( int iff = IFF_ABS_FOE; IFF_SYSTEM <= iff; --iff )
					{
						for( index = 0; index < cItems; ++index )
						{
							if( ti[ index ].iIFF == iff )
							{
								MetaIndex[ m ] = index;
								_add_item( hwndList, iff, ti[ index ] );
								
								if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
									cursel = m;
								
								++m;
							}
						}
					}
				}
			}
			else
			{
				for( index = 0; index < cItems; ++index )
				{
					MetaIndex[ m ] = index;
					_add_item( hwndList, ti[ index ].iIFF, ti[ index ] );

					if( dwTargetId && ti[ index ].dwProcessId == dwTargetId )
						cursel = m;
					
					++m;
				}
			}

			LVFINDINFO find;
			find.flags = LVFI_PARAM;
			for( ptrdiff_t x = 0; x < 3; ++x )
			{
				if( g_bHack[ x ] )
				{
					find.lParam = (LONG) g_dwTargetProcessId[ x ];
					hot[ x ] = ListView_FindItem( hwndList, -1, &find );
				}
				else hot[ x ] = -1;
			}

			for( int col_idx = 0; col_idx < N_COLS; ++col_idx )
			{
				ListView_SetColumnWidth( hwndList, col_idx, 
					( col_idx == IFF || col_idx == CPU || col_idx == THREADS )
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


			LV_OnSelChange( hDlg, MetaIndex, ti, cItems, current_IFF );

#if 1//def _DEBUG
	if( fDebug )
	{
	QueryPerformanceCounter(&li);
	LARGE_INTEGER freq;
	QueryPerformanceFrequency( &freq );
	TCHAR s[100];
	_stprintf_s(s,100,_T("%d Processes, %d Threads: cost=%.2f ms"),
		(int) cItems,
		numOfPairs,
		(double)(li.QuadPart-li0.QuadPart)/freq.QuadPart*1000.0);
	//OutputDebugString(s);
	//OutputDebugString(_T("\n"));
	SetWindowText(hDlg,s);
	}
#endif	

			fReloading = false;
			break;
		}

		case WM_COMMAND:
		{
			fCPUTime = false;

			const int CtrlID = LOWORD( wParam );
			if( CtrlID != IDC_NUKE
				&& !( CtrlID == IDOK && lParam == XLIST_WATCH_THIS )
				&& ! IsWindowEnabled( GetDlgItem( hDlg, CtrlID ) ) )
			{
				fCPUTime = true;
				break;
			}

			const ptrdiff_t sel
				= ListView_GetCurrentSel( GetDlgItem( hDlg, IDC_TARGET_LIST ) );

			ptrdiff_t index = 0;
			DWORD pid = 0;
			if( ti && MetaIndex && 0 <= sel && sel < cItems )
			{
				index = MetaIndex[ sel ];
				pid = ti[ index ].dwProcessId;
			}
			else
			{
				fCPUTime = true;
				break;
			}

			switch( CtrlID )
			{
				case IDOK:
				case IDC_SLIDER_BUTTON:
				{
					bool fLimitThis = false;
					if( IsProcessBES( pid ) )
					{
						MessageBox( hDlg, ti[ index ].szPath,
									TEXT("BES Can't Target BES"),
									MB_OK | MB_ICONEXCLAMATION );
						break;
					}

					// update the flag @20120927
					// This flag is used in the "Question" function; otherwise not important
					ti[ index ].fWatch = ( lParam == XLIST_WATCH_THIS );

					ptrdiff_t g = 0;
					for( ; g < 3; ++g )
					{
						if( pid == g_dwTargetProcessId[ g ] )
							break;
					}
					
					// Unlimit
					if( lParam != XLIST_WATCH_THIS )
					{
						if( g < 3 && g_bHack[ g ] )
						{
							if( g == 2 && g_bHack[ 3 ] )
							{
								SendMessage( g_hWnd, WM_COMMAND, IDM_UNWATCH, 0 );
							}

							// STOPF_NO_LV_REFRESH = "Don't send back WM_USER_REFRESH"
							SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) g, STOPF_NO_LV_REFRESH );
							SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE | URF_RESET_FOCUS );
							break;
						}
					}
					
					fLimitThis = (CtrlID == IDC_SLIDER_BUTTON)
									|| DialogBoxParam( g_hInst, MAKEINTRESOURCE(IDD_QUESTION),
														hDlg, &Question, (LPARAM) &ti[ index ] );

					ptrdiff_t iMyId = 0;
					int iSlider = 0;
					if( fLimitThis )
					{
						if( ti[ index ].iIFF == IFF_UNKNOWN || ti[ index ].iIFF == IFF_FRIEND )
						{
							if( g_numOfEnemies < MAX_PROCESS_CNT )
								_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], CCHPATH, ti[ index ].szExe );

							// If this target (=enemy) is an ex-friend, make the friend list
							// smaller by 1, by removing that ex-friend (at index i).
							// (1) If i!=last, then copy the last guy to the place at i.
							// (2) If i==last, just doing --g_num is enough (we'll write NUL too,
							//     just in case).  Do NOT copy in this case; copying is not
							//     only unnecessary, but the behavior is undefined, since
							//     overlapping (dst==src).
							// (3) Tcscpy_s does _tcscpy_s if (1), and does nothing if (2).
							for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
							{
								// formerly considered as Friend
								if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
								{
									Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );

									// Clean the last guy's place, who is now at index i if still a friend.
									// g_numOfFriends is > 0: otherwise this for-loop is never executed.
									*g_lpszFriend[ g_numOfFriends-- ] = _T('\0');
									// break;
								}
							}
						}

						if( lParam == XLIST_WATCH_THIS )
						{
							if( g_bHack[ 0 ] && g_bHack[ 1 ] && g_bHack[ 2 ] )
							{
								for( g = 0; g < MAX_SLOTS; ++g )
								{
									if( pid == g_dwTargetProcessId[ g ] )
									{
										SendMessage( g_hWnd, WM_USER_STOP,
													(WPARAM) g, STOPF_NO_LV_REFRESH );
										break;
									}
								}

								if( g == MAX_SLOTS )
								{
									MessageBox( hDlg, _T("4"), APP_NAME, MB_OK | MB_ICONSTOP );
									break;
								}
							}

							for( iMyId = MAX_SLOTS - 1; 0 <= iMyId; --iMyId )
							{
								if( !g_bHack[ iMyId ] &&
										WaitForSingleObject( hSemaphore[ iMyId ], 100 )
										== WAIT_OBJECT_0 ) break;
							}

							if( iMyId == -1 )
							{
								MessageBox( hDlg, TEXT( "Semaphore Error" ), APP_NAME, MB_OK | MB_ICONSTOP );
								break;
							}

							// Slot2 is already taken and active: we will get slot 2 after moving
							// the current target @ slot 2 to slot iMyId
							if( iMyId != 2 && g_bHack[ 2 ] )
							{
								// If old@2 and new@2 are the same (i.e. if we're gonna watch what
								// we are already limiting but not watching), just stop #2 then
								// reuse it to watch #2. The slot @ iMyId is freed.
								if( lphp[ 2 ].lpTarget->dwProcessId == pid )
								{
									// original iMyId sema is not needed
									ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );

									// hp[2] will be cleared by the called
									SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) 2, STOPF_NO_LV_REFRESH );

									// wait until slot 2 is free again
									if( WaitForSingleObject( hSemaphore[ 2 ], 2000 ) != WAIT_OBJECT_0 )
									{
										MessageBox( hDlg, TEXT("2->2"), APP_NAME, MB_ICONSTOP | MB_OK );
										break;
									}
								}
								else
								{
									// Prepare to move old target @ 2 to the new slot @ iMyId
									*lphp[ iMyId ].lpTarget = *lphp[ 2 ].lpTarget;
									lphp[ iMyId ].iMyId = iMyId;

									//?
									g_dwTargetProcessId[ iMyId ] = g_dwTargetProcessId[ 2 ];

									// hp[2] will be cleared by the called
									SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) 2, STOPF_NO_LV_REFRESH );

									// wait until slot 2 is free again
									if( WaitForSingleObject( hSemaphore[ 2 ], 2000 ) != WAIT_OBJECT_0 )
									{
										MessageBox( hDlg, TEXT("2->0/1"), APP_NAME, MB_ICONSTOP | MB_OK );
										break;
									}
									// Don't release sema for iMyId; it'll be used by old target@2
									SendMessage( g_hWnd, WM_USER_HACK, (WPARAM) iMyId, (LPARAM) &lphp[ iMyId ] );
								}
							}
							// iMyId is not 2, but slot 2 is free.  This situation is not likely;
							// only possible if ever via subtle race condition
							else if( iMyId != 2 /*&& !g_bHack[ 2 ]*/ )
							{
								ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
								MessageBox( hDlg, TEXT("race"), APP_NAME, MB_ICONSTOP | MB_OK );
								break;
							}

							iMyId = 2;
							iSlider = GetSliderIni( ti[ index ].szPath, g_hWnd );
							g_Slider[ iMyId ] = iSlider;

							*lphp[ 2 ].lpTarget = ti[ index ];
							lphp[ 2 ].iMyId = 2;
							ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
							SetTargetPlus( g_hWnd, &hChildThread[ 3 ], &lphp[ 2 ],
											ti[ index ].szPath,
											ti[ index ].szExe );
						}
						else if( g < MAX_SLOTS ) // relimit
						{
							iSlider = g_Slider[ g ];
							SendMessage( g_hWnd, WM_USER_RESTART, (WPARAM) g, 0 );
						}
						else
						{
							for( iMyId = 0; iMyId < MAX_SLOTS; ++iMyId )
							{
								if( !g_bHack[ iMyId ]
									&& 
									WaitForSingleObject( hSemaphore[ iMyId ], 100 )
										== WAIT_OBJECT_0 ) break;
							}

							if( iMyId == MAX_SLOTS )
							{
								MessageBox( hDlg, TEXT("Semaphore Error"), APP_NAME, MB_OK | MB_ICONSTOP );
								break;
							}

							lphp[ iMyId ].iMyId = iMyId;
							*(lphp[ iMyId ].lpTarget) = ti[ index ];

							iSlider = GetSliderIni( ti[ index ].szPath, g_hWnd );
							g_Slider[ iMyId ] = iSlider;

							SendMessage( g_hWnd, WM_USER_HACK, (WPARAM) iMyId, (LPARAM) &lphp[ iMyId ] );
						}
					} // fLimitThis
					else // cancel@Question
					{
						// reset the flag @20120927
						ti[ index ].fWatch = false;
					}
					
					if( fLimitThis )
					{
						SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
						ActivateSlider( hDlg, iSlider );
					}
					else
						SetDlgItemFocus( hDlg, GetDlgItem( hDlg, IDC_TARGET_LIST ) );

					break;
				} // case IDOK, IDC_SLIDER_BUTTON
				
				case IDCANCEL:
				{
					EndDialog( hDlg, XLIST_CANCELED );
					break;
				}

				case IDC_WATCH:
				{
					if( g_bHack[ 3 ] ) // Unwatch
					{
						SendMessage( g_hWnd, WM_COMMAND, IDM_UNWATCH, 0 );
						SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE | URF_RESET_FOCUS );
					}
					else // Watch new
					{
						SendMessage( hDlg, WM_COMMAND, IDOK, (LPARAM) XLIST_WATCH_THIS );
					}
					break;
				}

				case IDC_LISTALL_SYS:
				{
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

					SendMessage( hDlg, WM_USER_REFRESH, pid, 0 );
					break;
				}

				case IDC_RELOAD:
				{
					// Don't reload if already reloading
					if( ! fReloading )
					{
						fReloading = true;

						SendMessage( hDlg, WM_USER_REFRESH, pid, 0 );

						fReloading = false;
					}
					break;
				}

				case IDC_FOE:
				{
					WriteIni_Time( ti[ index ].szExe );

					if( g_numOfEnemies < MAX_PROCESS_CNT )
						_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], MAX_PATH * 2, ti[ index ].szExe );

					for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
					{
						// formerly considered as Friend
						if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
						{
							/*
							if( g_numOfFriends >= 2 )
							{
								_tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );
								*g_lpszFriend[ g_numOfFriends - 1 ] = _T('\0');
							}
							*/

							// $FIX(2/5) see above
							Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );

							*g_lpszFriend[ g_numOfFriends-- ] = _T('\0');

							//break;
						}
					}

					SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
					break;
				}

				case IDC_FRIEND:
				{
					WriteIni_Time( ti[ index ].szExe );

					if( g_numOfFriends < MAX_PROCESS_CNT )
						_tcscpy_s( g_lpszFriend[ g_numOfFriends++ ], MAX_PATH *2, ti[ index ].szExe );

					for( ptrdiff_t i = 0; i < g_numOfEnemies; ++i )
					{
						// formerly considered as Foe
						if( Lstrcmpi( ti[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
						{
							// $FIX(3/5): see above.
							Tcscpy_s( g_lpszEnemy[ i ], MAX_PATH * 2, g_lpszEnemy[ g_numOfEnemies - 1 ] );

							*g_lpszEnemy[ g_numOfEnemies-- ] = _T('\0');

							//break;
						}
					}

					SendMessage( hDlg, WM_USER_REFRESH, pid, URF_ENSURE_VISIBLE );
					break;
				}

				case IDC_RESET_IFF:
				{
					if( ti[ index ].iIFF == IFF_FRIEND )
					{
						ti[ index ].iIFF = IFF_UNKNOWN;
						for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
						{
							if( Lstrcmpi( ti[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
							{
								// $FIX(4/5): see above.
								Tcscpy_s( g_lpszFriend[ i ], MAX_PATH * 2, g_lpszFriend[ g_numOfFriends - 1 ] );

								*g_lpszFriend[ g_numOfFriends-- ] = _T('\0');
								
								//break;
							}
						}
					}
					else if( ti[ index ].iIFF == IFF_FOE )
					{
						ti[ index ].iIFF = IFF_UNKNOWN;
						for( ptrdiff_t i = 0; i < g_numOfEnemies; ++i )
						{
							// formerly considered as Foe
							if( Lstrcmpi( ti[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
							{
								// $FIX(5/5): see above.
								Tcscpy_s( g_lpszEnemy[ i ], MAX_PATH * 2, g_lpszEnemy[ g_numOfEnemies - 1 ] );

								*g_lpszEnemy[ g_numOfEnemies-- ] = _T('\0');

								//break;
							}
						}
					}

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
						ti[ index ].szPath, pid );
					if( MessageBox( hDlg, msg, APP_NAME, 
							MB_OKCANCEL | MB_ICONWARNING | MB_DEFBUTTON2 ) == IDOK )
					{
						SendMessage( g_hWnd, WM_COMMAND, (WPARAM) IDM_STOP, (LPARAM) 0 );
						Sleep( 1000 );
						if( Unfreeze( hDlg, pid ) )
						{
							MessageBox( hDlg, TEXT( "Successful!" ),
										TEXT( "Unfreezing" ), MB_ICONINFORMATION | MB_OK );
						}
						else
						{
							MessageBox( hDlg, TEXT( "An error occurred.\r\nPlease retry." ),
										TEXT( "Unfreezing" ), MB_ICONEXCLAMATION | MB_OK );
						}
					}

					///*lphp->lpTarget = ti[ index ];
					///EndDialog( hDlg, XLIST_UNFREEZE );
					break;
				}

				/*case IDC_SAFETY:
					break;*/

				case IDC_NUKE:
					if( ti[ index ].iIFF != IFF_SYSTEM
						&& pid != GetCurrentProcessId() )
					{
						_nuke( hDlg, ti[ index ] );
					}
					break;

				case IDC_AUTOREFRESH:
					fPaused = ( Button_GetCheck( (HWND) lParam ) == BST_UNCHECKED );
					break;

				case IDC_HIDE:
				case IDC_SHOW:
				{
					HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
					SetDlgItemFocus( hDlg, hLV );

					ShowProcessWindow( hDlg, pid,
										CtrlID == IDC_HIDE ? BES_HIDE : BES_SHOW );

					if( ! fContextMenu && g_bSelNextOnHide && KEY_UP( VK_SHIFT ) )
					{
						ListView_SetCurrentSel( hLV, sel + 1, TRUE );
						LV_OnSelChange( hDlg, MetaIndex, ti, cItems, current_IFF );
					}
					break;
				}
				
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
						LV_OnSelChange( hDlg, MetaIndex, ti, cItems, current_IFF );
				}
				else if( notified.code == NM_CLICK || notified.code == NM_RCLICK )
				{
					NMITEMACTIVATE& nm =*(NMITEMACTIVATE*) lParam;
					
					// nm.iItem is -1 when sub-item is clicked (unless LVS_EX_FULLROWSELECT);
					// therefore we manually check the index...
					LVHITTESTINFO hit = {0};
					hit.pt = nm.ptAction;
					int itemIndex = ListView_SubItemHitTest( notified.hwndFrom, &hit );
					if( itemIndex == -1 ) break;

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
					return LV_OnCustomDraw( hDlg, notified.hwndFrom, lParam, hot );
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
			if( LOWORD( wParam ) < TB_ENDTRACK
				&& GetDlgCtrlID( (HWND) lParam ) == IDC_SLIDER )
			{
				HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
				LVITEM li;
				li.mask = LVIF_PARAM;
				li.iItem = (int) ListView_GetCurrentSel( hLV );
				li.iSubItem = 0;
				ListView_GetItem( hLV, &li );

				int k = 0;
				while( k < MAX_SLOTS && g_dwTargetProcessId[ k ] != (DWORD) li.lParam ) ++k;
				if( k == MAX_SLOTS ) break;

				LRESULT lSlider = SendMessage( (HWND) lParam, TBM_GETPOS, 0, 0 );
				if( lSlider < SLIDER_MIN || lSlider > SLIDER_MAX )
					lSlider = SLIDER_DEF;

				g_Slider[ k ] = (int) lSlider;

				TCHAR strPercent[ 32 ] = _T("");
				GetPercentageString( lSlider, strPercent, _countof(strPercent) );
				SetDlgItemText( hDlg, IDC_TEXT_PERCENT, strPercent );
			
				if( g_bHack[ k ]
					&& ( k < 2 || k == 2 && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE ) )
				{
					
					_stprintf_s( lphp->lpszStatus[ 0 + 4 * k ], cchStatus,
								_T( "Target #%d [ %s ]" ),
								k + 1,
								strPercent );

					RECT rcStatus;
					SetRect( &rcStatus, 20, 20 + 90 * k, 479, 40 + 90 * k );
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
				HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
				LVITEM li;
				li.mask = LVIF_PARAM;
				li.iItem = (int) ListView_GetCurrentSel( hLV );
				li.iSubItem = 0;
				ListView_GetItem( hLV, &li );

				int k = 0;
				while( k < MAX_SLOTS && g_dwTargetProcessId[ k ] != (DWORD) li.lParam ) ++k;
				if( k < MAX_SLOTS && g_bHack[ k ] )
				{
					SetBkMode( (HDC) wParam, OPAQUE );
					SetBkColor( (HDC) wParam, RGB( 0xff, 0, 0 ) );
					SetTextColor( (HDC) wParam, RGB( 0xff, 0xff, 0xff ) );
					return (INT_PTR)(HBRUSH) GetSysColorBrush( COLOR_3DFACE );
				}
			}
			return (INT_PTR)(HBRUSH) NULL;
			break;
		}

		case WM_PAINT:
		{
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

			GetWindowRect( GetDlgItem( hDlg, IDC_UNLIMIT_ALL ), &rc );
			MapWindowPoints( NULL, hDlg, (LPPOINT) &rc, 2 );
			const int x1 = rc.left + 1;
			const int x2 = rc.right;

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
			if( GetDlgCtrlID( (HWND) wParam ) == IDC_TARGET_LIST )
			{
				fContextMenu = true;
				LV_OnContextMenu( hDlg, (HWND) wParam, lParam, current_IFF );
				fContextMenu = false;
			}
			break;
		}

		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			/*if( wParam == 0 && lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, MAX_WINDOWTEXT );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}*/
			
			if( message == WM_USER_CAPTION )
				break;

			return FALSE;
			break;
		}
		
		case WM_NCUAHDRAWCAPTION:
		{
			/*if( lpSavedWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpSavedWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpSavedWindowText );
			}*/
			return FALSE;
			break;
		}

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

			// todo race condition: wait until YOU-CAN-READ
			Sleep(100);
			UpdateProcessSnap( NULL, NULL, 0, NULL );

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

			/*if( hListBoxBrush )
			{
				DeleteBrush( hListBoxBrush );
				hListBoxBrush = NULL;
			}*/

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

static void _nuke( HWND hDlg, const TARGETINFO& target )
{
	if( target.dwProcessId == GetCurrentProcessId()
		|| IsProcessBES( target.dwProcessId, NULL, 0 ) )
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


/*DWORD * ListProcessThreads_Alloc( DWORD dwOwnerPID, ptrdiff_t * pNumOfThreads ) //v1.6.x
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
}*/

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

DWORD * ListProcessThreads_Alloc( DWORD dwOwnerPID, ptrdiff_t& numOfThreads ) // v1.7.0
{
	numOfThreads = 0;

	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hThreadSnap == INVALID_HANDLE_VALUE )
	{
		return NULL;
	}

	ptrdiff_t maxNumOfThreads = 64; // 256 bytes
	DWORD * lpThreadIDs = (DWORD *) HeapAlloc( GetProcessHeap(), 0,
												sizeof(DWORD) * maxNumOfThreads );
	if( ! lpThreadIDs )
	{
		CloseHandle( hThreadSnap );
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

static PROCESS_THREAD_PAIR * AllocSortedPairs( ptrdiff_t& numOfPairs )
{
	numOfPairs = 0;

	ptrdiff_t maxNumOfPairs = 2048; // 16 KiB
	PROCESS_THREAD_PAIR * lpPairs = (PROCESS_THREAD_PAIR *) HeapAlloc( GetProcessHeap(), 0, 
											sizeof(PROCESS_THREAD_PAIR) * maxNumOfPairs );

	if( ! lpPairs )	return NULL;

	HANDLE hThreadSnap = CreateToolhelp32Snapshot( TH32CS_SNAPTHREAD, 0 );
	if( hThreadSnap == INVALID_HANDLE_VALUE )
	{
		HeapFree( GetProcessHeap(), 0, lpPairs );
		return NULL;
	}

	THREADENTRY32 te32;
	te32.dwSize = (DWORD) sizeof(THREADENTRY32); 
	for( BOOL b = Thread32First( hThreadSnap, &te32 ); b; b = Thread32Next( hThreadSnap, &te32 ) )
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
	
		lpPairs[ numOfPairs ].pid = te32.th32OwnerProcessID;
		lpPairs[ numOfPairs ].tid = te32.th32ThreadID;
		
		++numOfPairs;

	}

	CloseHandle( hThreadSnap );
	
	if( lpPairs )
	{
		qsort( lpPairs, (size_t) numOfPairs, sizeof(PROCESS_THREAD_PAIR), &pair_comp ); 
	}
	
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

BOOL IsProcessBES( DWORD dwProcessId, const PROCESS_THREAD_PAIR * pairs, ptrdiff_t numOfPairs )
{
	if( dwProcessId == 0 || dwProcessId == (DWORD) -1 ) // trivially false
		return FALSE;
	
	if( dwProcessId == GetCurrentProcessId() ) // this instance itself
		return TRUE;

	BOOL bThisIsBES = FALSE; // another instance of BES
	
	if( pairs )
	{
		for( ptrdiff_t i = 0; (!bThisIsBES) && i < numOfPairs; ++i )
			EnumThreadWindows( pairs[ i ].tid, &EnumThreadWndProc_DetectBES, (LPARAM) &bThisIsBES );
	}
	else
	{
		const ptrdiff_t maxNumOfThreads = 32; // BES doesn't have more than 32 threads####
		DWORD rgThreadIDs[ maxNumOfThreads ];
		ptrdiff_t numOfThreads = ListProcessThreads( dwProcessId, rgThreadIDs, maxNumOfThreads );
		for( ptrdiff_t i = 0; (!bThisIsBES) && i < numOfThreads; ++i )
			EnumThreadWindows( rgThreadIDs[ i ], &EnumThreadWndProc_DetectBES, (LPARAM) &bThisIsBES );
	}

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