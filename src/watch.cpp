#include "BattleEnc.h"

void WriteIni_Time( const TCHAR * pExe );

extern HWND g_hWnd;
extern HANDLE hSemaphore[ 4 ];
extern TCHAR g_szTarget[ 3 ][ CCHPATHEX ];

extern volatile DWORD g_dwTargetProcessId[ 4 ];
extern volatile int g_Slider[ 4 ];
extern volatile BOOL g_bHack[ 4 ];
BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );
BOOL SetDebugPrivilege( HANDLE hToken, BOOL bEnable, DWORD * pPrevAttributes );

static unsigned __stdcall LimitPlus( LPVOID lpVoid )
{
	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

	HWND hWnd = g_hWnd;

	if( ! lpVoid || ! hWnd )
		return NOT_WATCHING;

	HANDLE hToken = NULL;
	BOOL bPriv = FALSE;
	DWORD dwPrevAttributes = 0L;
	bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	LPHACK_PARAMS hp = (LPHACK_PARAMS) lpVoid - 2;
	TCHAR ** lpszStatus = hp[ 0 ].lpszStatus;

	WriteIni_Time( hp[ 2 ].lpTarget->szExe );

	EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
	EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_ENABLED );

	g_dwTargetProcessId[ 2 ] = WATCHING_IDLE;

	TCHAR lpszTargetPath[ CCHPATH ];
	TCHAR lpszTargetExe[ CCHPATH ];
	_tcscpy_s( lpszTargetPath, CCHPATH, hp[ 2 ].lpTarget->szPath );
	_tcscpy_s( lpszTargetExe, CCHPATH, hp[ 2 ].lpTarget->szExe );
	
	_stprintf_s( g_szTarget[ 2 ], CCHPATHEX, _T( "%s (Watching)" ), lpszTargetPath );

	ptrdiff_t i;
	for( i = 8; i < 12; i++ )
		lpszStatus[ i ][ 0 ] = _T('\0');

	_tcscpy_s( lpszStatus[ 13 ], cchStatus, _T( "Watching..." ) );

	TCHAR str[ CCHPATHEX ];
	_stprintf_s( str, _countof(str), _T( "Watching : %s" ), lpszTargetPath );
	WriteDebugLog( str );

#if 1//def _UNICODE  // ANSIFIX@20140307(1/5)
	_tcscpy_s( lpszStatus[ 14 ], cchStatus, lpszTargetPath );
#else
	strcpy_s( str, _countof(str), lpszTargetPath );
	if( strlen( str ) >= 19 )
	{
		str[ 15 ] = '\0';
		PathToExeEx( str, _countof(str) );
	}
	strcpy_s( lpszStatus[ 14 ], cchStatus, str );
#endif

	AdjustLength( lpszStatus[ 14 ] );

	g_Slider[ 2 ] = GetSliderIni( lpszTargetPath, hWnd );


	// UpdateStatus( hWnd );
	bool fInitialUpdate = true;

	ptrdiff_t slotid = 2;
	
	while( g_bHack[ 3 ] && g_bHack[ 3 ] != UNWATCH_NOW )
	{
		const bool fWatchMulti = !!( MF_CHECKED & GetMenuState( GetMenu( hWnd ), 
													IDM_WATCH_MULTI, MF_BYCOMMAND ) );

		bool fNewTarget = false;
		DWORD dwPID = 0L;

		// CASE 1: No PID was given
		if( hp[ 2 ].lpTarget->dwProcessId == TARGET_PID_NOT_SET )
		{
			g_dwTargetProcessId[ 2 ] = PathToProcessID( lpszTargetPath );
			fNewTarget = ( g_dwTargetProcessId[ 2 ] && g_dwTargetProcessId[ 2 ] != (DWORD) -1 );
		}
		// CASE 2: PID has been given; Slot 2 is used
		else if( g_dwTargetProcessId[ 2 ] && g_dwTargetProcessId[ 2 ] != (DWORD) -1 )
		{
			if( fWatchMulti && ( g_bHack[ 0 ] || g_bHack[ 1 ] || g_bHack[ 2 ] ) )
			{
				DWORD rgPIDToExclude[ 3 ];
				ptrdiff_t numOfPIDs = 0;
				for( i = 0; i < 3; ++i )
				{
					if( _tcsicmp( lpszTargetPath, hp[ i ].lpTarget->szPath ) == 0 )
						rgPIDToExclude[ numOfPIDs++ ] = hp[ i ].lpTarget->dwProcessId;
				}

				dwPID = PathToProcessID( lpszTargetPath, &rgPIDToExclude[ 0 ], numOfPIDs );

				if( dwPID && dwPID != (DWORD) -1 )
					fNewTarget = true;
			}
		}
		// CASE 3: PID has been given; Slot 2 is free
		else
		{
			g_dwTargetProcessId[ 2 ] = hp[ 2 ].lpTarget->dwProcessId; //###
			fNewTarget = true;
			_stprintf_s( str, _countof(str), _T( "Watching : ProcessID given : %08lX" ), 
						g_dwTargetProcessId[ 2 ] );
			WriteDebugLog( str );
		}

		if( fNewTarget )
		{
			if( dwPID ) // non-first instance in multi-watch
			{
				if( ! g_bHack[ 2 ] )
					slotid = 2;
				else if( ! g_bHack[ 1 ] )
					slotid = 1;
				else if( ! g_bHack[ 0 ] )
					slotid = 0;
				else
					fNewTarget = false;
			}
			else
			{
				slotid = 2;
			}
		}

		if( fNewTarget )
		{
			if( WaitForSingleObject( hSemaphore[ slotid ], 1000 ) == WAIT_OBJECT_0 )
			{
				if( dwPID ) // non-first instance in multi-watch
				{
					hp[ slotid ].lpTarget->dwProcessId = dwPID;
					g_dwTargetProcessId[ slotid ] = dwPID; // ###
				}
			}
			else
			{
				fNewTarget = false;

				if( ! dwPID ) // single-watch
				{
					g_bHack[ slotid ] = FALSE;
					MessageBox( hWnd, TEXT("Sema Error in LimitPlus()"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
					goto EXIT_THREAD;
				}
			}
		}

		if( fNewTarget )
		{
			for( i = 0; i < 3; ++i ) // if already-limited but non-watched, stop the old limiting
			{
				if( i == slotid )
					continue;

				if( g_dwTargetProcessId[ slotid ] == g_dwTargetProcessId[ i ] )
				{
					if( g_bHack[ i ] )
						SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, 0 );

					Sleep( 100 );
					for( ptrdiff_t j = (4*i); j < (4*i) + 4; ++j )
						lpszStatus[ j ][ 0 ] = _T('\0');

					g_dwTargetProcessId[ i ] = TARGET_PID_NOT_SET;
					_tcscpy_s( g_szTarget[ i ], CCHPATHEX, TARGET_UNDEF );
				}
			}
		}

		if( g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
		{
			if( _tcscmp( lpszStatus[ 15 ], _T( "Not found" ) ) != 0 )
			{
				_tcscpy_s( lpszStatus[ 15 ], cchStatus, _T( "Not found" ) );
				InvalidateRect( hWnd, NULL, FALSE );
			}
		}
		else if( fNewTarget )
		{
			_stprintf_s( str, _countof(str), _T( "Watching : Found! <%s>" ), lpszTargetPath );
			WriteDebugLog( str );

			SYSTEMTIME st;
			GetLocalTime( &st );
			_stprintf_s( lpszStatus[ 15 ], cchStatus, 
							_T( "%02d:%02d:%02d Found [PID %lu]" ),
							st.wHour,
							st.wMinute,
							st.wSecond,
							g_dwTargetProcessId[ slotid ] );
			
			hp[ slotid ].lpTarget->dwProcessId = g_dwTargetProcessId[ slotid ];
			_tcscpy_s( hp[ slotid ].lpTarget->szPath, CCHPATH, lpszTargetPath );
			_tcscpy_s( hp[ slotid ].lpTarget->szExe, CCHPATH, lpszTargetExe );
			hp[ slotid ].lpTarget->szText[ 0 ] = _T('\0');
			hp[ slotid ].lpTarget->fWatch = true;
			
			g_Slider[ slotid ] = GetSliderIni( lpszTargetPath, hWnd );

			SendMessage( hWnd, WM_USER_HACK, (WPARAM) slotid, (LPARAM) &hp[ slotid ] );

			// UpdateStatus not needed here; it'll be done on WM_USER_HACK
			// UpdateStatus( hWnd );
			fInitialUpdate = false;

			if( fWatchMulti ) // look for another instance now, without waiting
				continue;
		}

		if( fInitialUpdate )
		{
			UpdateStatus( hWnd );
			fInitialUpdate = false;
		}

		UINT kMax = GetPrivateProfileInt( _T("Options"), _T("WatchRT"), 80, GetIniPath() );
		if( 80 < kMax )
			kMax = 80;
		
		if( kMax < 20 )
			kMax = 20;
		
//		if( fWatchMulti && kMax == 80 )
//			kMax = 40;

		for( UINT k = 0; k < kMax; ++k ) // wait several seconds
		{
			Sleep( 100 );
			if( g_bHack[ 3 ] == FALSE || g_bHack[ 3 ] == UNWATCH_NOW )
				goto EXIT_THREAD;
		}
	}

EXIT_THREAD:
	WriteDebugLog( TEXT( "Watch: Exiting Thread..." ) );

	// if the watchee is not running, slot will be freed when 'unwatch' is posted
	// (Otherwise, slot should be still active)
	if( g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
	{
		g_dwTargetProcessId[ 2 ] = TARGET_PID_NOT_SET;
		hp[ 2 ].lpTarget->dwProcessId = TARGET_PID_NOT_SET;
		g_bHack[ 2 ] = FALSE;
		ReleaseSemaphore( hSemaphore[ 2 ], 1L, (LONG *) NULL );
	}

	ReleaseSemaphore( hSemaphore[ 3 ], 1L, (LONG *) NULL );

	_tcscpy_s( lpszStatus[ 13 ], cchStatus, _T( "* Not watching *" ) );
	_tcscpy_s( lpszStatus[ 14 ], cchStatus, lpszTargetPath );
	AdjustLength( lpszStatus[ 14 ] );
	lpszStatus[ 15 ][ 0 ] = _T('\0');
	InvalidateRect( hWnd, NULL, TRUE );

	if( ! g_bHack[ 2 ] )
		EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED ); // ?

	EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED ); // ?

	UpdateStatus( hWnd );

	g_bHack[ 3 ] = FALSE;

	if( hToken )
	{
		if( bPriv ) 
			SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}
	return NOT_WATCHING;
}


BOOL SelectWatch( HWND hWnd, HANDLE * phChildThread, LPHACK_PARAMS lphp )
{
	const TCHAR * strIniPath = GetIniPath();
	TCHAR szWatchDir[ MAX_PATH * 2 ] = _T("");
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "WatchDir" ),
		TEXT( "" ),
		szWatchDir,
		(DWORD) _countof(szWatchDir),
		strIniPath
	);
	
	if( szWatchDir[ 0 ] == _T('\0')
		|| GetFileAttributes( szWatchDir ) == INVALID_FILE_ATTRIBUTES )
	{
		if( GetShell32Version() < 5
			|| ! SUCCEEDED(	SHGetFolderPath( NULL, 
								CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, szWatchDir ) ) ) 
		{
			GetCurrentDirectory( (DWORD) _countof(szWatchDir), szWatchDir );
		}
	}
	
	TCHAR lpszTargetPath[ MAX_PATH * 2 ] = TEXT( "" );
	TCHAR lpszTargetExe[ MAX_PATH * 2 ] = TEXT( "" );
	OPENFILENAME ofn;
	ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = TEXT( "Application (*.exe)\0*.exe\0All (*.*)\0*.*\0\0" );
	ofn.lpstrInitialDir = szWatchDir;
	ofn.nFilterIndex = 1L;
	ofn.lpstrDefExt = TEXT( "exe" );
	ofn.lpstrFile = lpszTargetPath;
	ofn.nMaxFile = (DWORD) _countof(lpszTargetPath);
	ofn.lpstrFileTitle = lpszTargetExe;
	ofn.nMaxFileTitle = (DWORD) _countof(lpszTargetExe);
#ifdef _UNICODE
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
#else
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
#endif


	ofn.lpstrTitle = TEXT( "Limit & Watch / Watch & Limit" );

	if( ! GetOpenFileName( &ofn ) )
	{
		return FALSE;
	}

	if( 1 < ofn.nFileOffset && ofn.nFileOffset < _countof(szWatchDir) )
	{
		memcpy( szWatchDir, lpszTargetPath, ( ofn.nFileOffset - 1 ) * sizeof(TCHAR) );
		szWatchDir[ ofn.nFileOffset - 1 ] = _T('\0');
		WritePrivateProfileString(
			TEXT( "Options" ), 
			TEXT( "WatchDir" ),
			szWatchDir,
			strIniPath
		);
	}

	if( IsProcessBES( PathToProcessID( lpszTargetPath ) ) )
	{
		MessageBox( hWnd, lpszTargetPath, TEXT("BES Can't Target BES"), MB_OK | MB_ICONEXCLAMATION );
		return FALSE;
	}

	lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET;

#if 0//!defined( _UNICODE ) // ANSIFIX@20140307(2/5)
	_tcscpy_s( lpszTargetPath, _countof(lpszTargetPath), lpszTargetExe );
#endif

	return SetTargetPlus( hWnd, phChildThread, lphp, lpszTargetPath, lpszTargetExe );
}




BOOL SetTargetPlus( HWND hWnd, HANDLE * phChildThread, LPHACK_PARAMS lphp,
				   LPCTSTR lpszTargetPath, LPCTSTR lpszTargetExe )
{
	// we always use the slot 2 to start watching.
	if( g_bHack[ 2 ] || g_bHack[ 3 ] )
	{
		MessageBox( hWnd, TEXT("Busy"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
		return FALSE;
	}

	// sema2 will be acquired in LimitPlus, not here
	/*if( WaitForSingleObject( hSemaphore[ 2 ], 100UL ) != WAIT_OBJECT_0 )
	{
		MessageBox( hWnd, TEXT("Sema Error (2)"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
		return FALSE;
	}*/

	if( WaitForSingleObject( hSemaphore[ 3 ], 100UL ) != WAIT_OBJECT_0 )
	{
		//ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
		MessageBox( hWnd, TEXT("Sema Error (3)"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
		return FALSE;
	}

	g_bHack[ 2 ] = TRUE;
	g_bHack[ 3 ] = TRUE;

	if( lphp->lpTarget->dwProcessId == TARGET_PID_NOT_SET )
	{
		ZeroMemory( lphp->lpTarget, sizeof( TARGETINFO ) );
		_tcscpy_s( lphp->lpTarget->szExe, _countof(lphp->lpTarget->szExe), lpszTargetExe );
		_tcscpy_s( lphp->lpTarget->szPath, _countof(lphp->lpTarget->szPath), lpszTargetPath );
		lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET;
	}

	lphp->iMyId = 2;
	lphp->lpTarget->fWatch = true; // ?

	*phChildThread = CreateThread2( &LimitPlus,	lphp );

	if( *phChildThread == NULL )
	{
		lphp->lpTarget->fWatch = false; // ?
		//ReleaseSemaphore( hSemaphore[ 2 ], 1L, (LONG *) NULL );
		ReleaseSemaphore( hSemaphore[ 3 ], 1L, (LONG *) NULL );
		g_bHack[ 2 ] = FALSE;
		g_bHack[ 3 ] = FALSE;
		MessageBox( hWnd, TEXT( "CreateThread failed." ), APP_NAME, MB_OK | MB_ICONSTOP );
		return FALSE;
	}

	return TRUE;
}


BOOL Unwatch( HANDLE& hSema, HANDLE& hThread, volatile BOOL& bWatch )
{
	BOOL bSuccess = FALSE;

	bWatch = FALSE;
	if( WaitForSingleObject( hSema, 3000UL ) == WAIT_OBJECT_0 )
	{
		if( hThread )
		{
			WriteDebugLog( TEXT( "Closing hChildThread[ 3 ]" ) );
			CloseHandle( hThread );
			hThread = NULL;
		}
		ReleaseSemaphore( hSema, 1L, (LPLONG) NULL );

		bSuccess = TRUE;
	}
	else
	{
		WriteDebugLog( TEXT( "[!] Sema3 Error" ) );
	}

	return bSuccess;
}

