/* 
 *	Copyright (C) 2005-2017 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

// Multi-path watch: "--add" or "-a". Up to 4 paths for now (2014-03-31)
// "-J" or "--job-list" supported (2015-12-03)
// Up to 8 paths (2017-11-04)
PATHINFO g_arPathInfo[ MAX_WATCH ];
ptrdiff_t g_nPathInfo = 0;


BOOL LimitPID( HWND hWnd, TARGETINFO * pTarget, const DWORD pid, int iSlider, BOOL bUnlimit, const int * aiParams ); // v1.7.5

extern HANDLE hReconSema;

extern HWND g_hWnd;
extern HANDLE hSemaphore[ MAX_SLOTS ];
extern HANDLE hChildThread[ MAX_SLOTS ];
extern TCHAR * ppszStatus[ 18 ];
extern bool g_fRealTime;

extern volatile int g_Slider[ MAX_SLOTS ];
extern volatile BOOL g_bHack[ MAX_SLOTS ];

void WriteIni_Time( const TCHAR * pExe );
BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );

void CachedList_Refresh( DWORD msMaxWait );
BOOL HasFreeSlot( void );
bool PathEqual( const PATHINFO& pathInfo, const TCHAR * lpPath, size_t cch, bool fSafe );
void ResetTi( TARGETINFO& ti );
void SetLongStatusText( const TCHAR * szText, TCHAR * pszStatus );
DWORD ParsePID( const TCHAR * strTarget ); // v1.7.5

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
	ptrdiff_t& numOfPIDs );


static void Found_Clear( FOUNDPID *& arFound, ptrdiff_t& nFound )
{
	if( arFound )
	{
		for( ptrdiff_t n = 0; n < nFound; ++n )
			if( arFound[ n ].lpPath ) TcharFree( arFound[ n ].lpPath );
		MemFree( arFound );
		arFound = NULL;
		nFound = 0;
	}
}

static BOOL GetSlots( int * arSlots, ptrdiff_t numOfSlots, TARGETINFO * ti )
{
	ptrdiff_t n = 0;
	ptrdiff_t slotid = 2;
	for( ; 0 <= slotid && n < numOfSlots; --slotid )
	{
		if( ! g_bHack[ slotid ]	&& ! ti[ slotid ].fUnlimited
			&& WaitForSingleObject( hSemaphore[ slotid ], 100 ) == WAIT_OBJECT_0 )
		{
			arSlots[ n++ ] = slotid;
			ResetTi( ti[ slotid ] );
			if( hChildThread[ slotid ] )
			{
				CloseHandle( hChildThread[ slotid ] );
				hChildThread[ slotid ] = NULL;
			}
		}
	}
				
	for( slotid = 4; slotid < MAX_SLOTS && n < numOfSlots; ++slotid )
	{
		if( ! g_bHack[ slotid ]	&& ! ti[ slotid ].fUnlimited
			&& WaitForSingleObject( hSemaphore[ slotid ], 100 ) == WAIT_OBJECT_0 )
		{
			arSlots[ n++ ] = slotid;
			ResetTi( ti[ slotid ] );
			if( hChildThread[ slotid ] )
			{
				CloseHandle( hChildThread[ slotid ] );
				hChildThread[ slotid ] = NULL;
			}
		}
	}

	if( n == numOfSlots )
		return TRUE;

	for( ptrdiff_t s = 0; s < n; ++s )
	{
		ReleaseSemaphore( hSemaphore[ arSlots[ s ] ], 1L, NULL );
	}

	return FALSE;
}


static unsigned __stdcall LimitPlus( void * pv )
{
	if( ! pv ) return 0xFFFF;

	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

	HANDLE hToken = NULL;
	BOOL bPriv = FALSE;
	DWORD dwPrevAttributes = 0L;
	bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	if( g_bHack[ 2 ] ) return 0; // #

	HWND hWnd = g_hWnd;

	TARGETINFO * ti = (TARGETINFO *) pv;

	if( ! ti[ 2 ].lpPath ) return 0xFFFF;


#ifdef _DEBUG
TCHAR dbgstr[256];_sntprintf_s(dbgstr,_countof(dbgstr),_TRUNCATE,_T("\t<LimitPlus> slot=%u PID=%d (%s)\n\n"),ti[2].slotid,
						 ti[2].dwProcessId,ti[2].lpPath);
OutputDebugString(dbgstr);
#endif




	// star Slider: LOBYTE of the next TCHAR after the term. NUL
	//int starSlider = (int)( ti[ 2 ].lpPath[ cchPath + 1 ] & 0xFF );

	WriteIni_Time( ti[ 2 ].disp_path ? ti[ 2 ].disp_path : ti[ 2 ].lpPath );

	//EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
	EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_ENABLED );

	TCHAR szTargetPath[ CCHPATH ] = _T("");
	_tcscpy_s( szTargetPath, CCHPATH, ti[ 2 ].lpPath ); // TODO

	ptrdiff_t i;
	for( i = 8; i < 18; ++i )
		ppszStatus[ i ][ 0 ] = _T('\0');

	//_tcscpy_s( ppszStatus[ 13 ], cchStatus, _T( "Watching... : hit [L] for more info." ) );

	TCHAR str[ CCHPATHEX ];
	_stprintf_s( str, _countof(str), _T( "Watching : %s" ), szTargetPath );
	WriteDebugLog( str );

	SetLongStatusText( szTargetPath, ppszStatus[ 14 ] );

	PATHINFO pathInfo[ MAX_WATCH ];
	ZeroMemory( pathInfo, sizeof(PATHINFO) * MAX_WATCH );

	/*
	// Reset (clear) the fUnlimited flag
	// - This is not needed. HandleJobList and --add do this itself.  Otherwise, WM_USER_RESTART is sent. @20161108
	for( i = 0; i < MAX_SLOTS; ++i )
	{
		if( ti[ i ].fUnlimited )
		{
			const TCHAR * q = ti[ i ].disp_path ? ti[ i ].disp_path : ti[ i ].lpPath ;
			if( PathEqual( pathInfo[ 0 ], q, _tcslen( q ), false ) ) // this is wrong!  pathInfo[ 0 ] is empty
				ti[ i ].fUnlimited = false;
		}
	}
	*/

	timeBeginPeriod( 1 );

	ptrdiff_t nInfo = 0;
	ptrdiff_t prev_nInfo = 0;
	bool fInitialUpdate = true;
	bool fBES = false;
	//ptrdiff_t slotid = 2;
	//TCHAR * lpPath = NULL;
	FOUNDPID * arFound = NULL;
	int * arSlots = NULL;
	ptrdiff_t nFound = 0;
	while( g_bHack[ 3 ] && g_bHack[ 3 ] != UNWATCH_NOW )
	{
		if( WaitForSingleObject( hReconSema, 3000 ) != WAIT_OBJECT_0 ) break;
		nInfo = g_nPathInfo;
		if( 1 <= nInfo && nInfo <= MAX_WATCH )
		{
			ptrdiff_t n = 0;
			for( ; n < nInfo; ++n )
			{
				pathInfo[ n ].cchPath = g_arPathInfo[ n ].cchPath;
				pathInfo[ n ].cchHead = g_arPathInfo[ n ].cchHead;
				pathInfo[ n ].cchTail = g_arPathInfo[ n ].cchTail;
				pathInfo[ n ].slider = g_arPathInfo[ n ].slider;

				pathInfo[ n ].wCycle = g_arPathInfo[ n ].wCycle;
				pathInfo[ n ].wDelay = g_arPathInfo[ n ].wDelay;

				size_t cchMem = g_arPathInfo[ n ].cchPath + 1;
				if( pathInfo[ n ].pszPath ) TcharFree( pathInfo[ n ].pszPath );
				pathInfo[ n ].pszPath = TcharAlloc( cchMem );
				if( pathInfo[ n ].pszPath && g_arPathInfo[ n ].pszPath )
					_tcscpy_s( pathInfo[ n ].pszPath, cchMem, g_arPathInfo[ n ].pszPath );
				else break;
			}
			nInfo = n;
		}
		ReleaseSemaphore( hReconSema, 1L, NULL );

		if( ! nInfo || MAX_WATCH < nInfo )
			break;
		
		ptrdiff_t yy;
		if( 2 <= nInfo )
		{
			for( yy = 0; yy < 3 && yy < nInfo; ++yy )
				SetLongStatusText( pathInfo[ yy ].pszPath, ppszStatus[ 14 + yy ] );
		}

		if( nInfo != prev_nInfo )
		{
			_stprintf_s( ppszStatus[ 13 ], cchStatus, _T( "Watching %d target%s..." ), (int) nInfo, ( nInfo == 1 ? _T("") : _T("s") ) );
			if( nInfo >= 4 ) _tcscpy_s( ppszStatus[ 17 ], cchStatus, _T( "Hit [L] for the full list." ) );

			InvalidateRect( hWnd, NULL, FALSE );
		}
		prev_nInfo = nInfo;

		//const bool fWatchMulti = ( 2 <= nInfo )
		//							|| ( MF_CHECKED & GetMenuState( GetMenu( hWnd ), 
		//											IDM_WATCH_MULTI, MF_BYCOMMAND ) );
		//DWORD dwNewTarget = 0;
		//DWORD dwPID = 0L;
		DWORD rgPIDToExclude[ MAX_SLOTS ];
		ptrdiff_t nExcluded = 0;
		for( i = 0; i < MAX_SLOTS; ++i ) // Exclude a slot that is active, or manually unlimited
		{
			if( i != 3 && ( g_bHack[ i ] || ti[ i ].fUnlimited ) && ti[ i ].dwProcessId	)
			{
				rgPIDToExclude[ nExcluded++ ] = ti[ i ].dwProcessId;
			}
		}

		//ptrdiff_t found_index = 0;

#if 0
		// CASE 1: Slot 2 is free / no PID given
		if( ! ti[ 2 ].dwProcessId )
		{
			// note: everyone (ParseArgs, xList, and SelectWatch) uses longPathNames
			CachedList_Refresh( 1000 );
			dwNewTarget = PathToProcessID_Cached( pathInfo, nInfo, rgPIDToExclude, nExcluded, &lpPath, &found_index );
			if( dwNewTarget == (DWORD) -1 ) dwNewTarget = 0;
		}
		// CASE 2: Slot 2 is already taken
		else if( g_bHack[ 2 ] || ti[ 2 ].fUnlimited )
		{
			if( fWatchMulti && HasFreeSlot() )
			{
				CachedList_Refresh( 1000 );
				dwPID = PathToProcessID_Cached( pathInfo, nInfo, rgPIDToExclude, nExcluded, &lpPath, &found_index );

				if( dwPID != (DWORD) -1 && dwPID )
					dwNewTarget = dwPID;
				else dwPID = 0;
			}
		}
#endif
		//if( 2 <= nInfo || ! ti[ 2 ].dwProcessId )

		Found_Clear( arFound, nFound );
		arFound = FindPIDs( pathInfo, nInfo, rgPIDToExclude, nExcluded, nFound );
		if( ! arFound ) break;

		if( nFound )
		{
			CachedList_Refresh( 1000 );

			ptrdiff_t numOfPairs = 0;
			PROCESS_THREAD_PAIR * sorted_pairs = GetCachedPairs( 1000, numOfPairs );
			ptrdiff_t fx = 0;
			for( ; fx < nFound; ++fx )
			{
				if( IsProcessBES( arFound[ fx ].pid, sorted_pairs, numOfPairs ) )
				{
					fBES = true;
					break;
				}
			}

			if( sorted_pairs )
			{
				MemFree( sorted_pairs );
				sorted_pairs = NULL;
			}

			if( fBES )
				break;

			if( arSlots ) MemFree( arSlots );
			arSlots = (int *) MemAlloc( sizeof(int), (size_t) nFound );
			if( ! arSlots ) break;

			if( ! GetSlots( arSlots, nFound, ti ) )
			{
				MessageBox(hWnd,_T("Couldn't get slots"),APP_NAME,MB_ICONSTOP);
				break;
			}

			bool fRealTime = g_fRealTime;

			for( fx = 0; fx < nFound; ++fx )
			{
				int slotid = arSlots[ fx ];
				arFound[ fx ].slot = slotid;

				ResetTi( ti[ slotid ] );
				ti[ slotid ].dwProcessId = arFound[ fx ].pid;
			
				ptrdiff_t path_info_index = arFound[ fx ].index;
				size_t cchMem_Path = pathInfo[ path_info_index ].cchPath + 1;
				ti[ slotid ].lpPath = TcharAlloc( cchMem_Path );
				if( ! ti[ slotid ].lpPath ) break;
				const TCHAR * path0 = pathInfo[ path_info_index ].pszPath;
				_tcscpy_s( ti[ slotid ].lpPath, cchMem_Path, path0 );
				if( arFound[ fx ].lpPath && ! StrEqualNoCase( arFound[ fx ].lpPath, path0 ) )
				{
					// old .disp_path has been already freed in ResetTi (in GetSlots)
					ti[ slotid ].disp_path = arFound[ fx ].lpPath;
					arFound[ fx ].lpPath = NULL; // not to be freed here
				}

				const TCHAR * q = ti[ slotid ].disp_path ? ti[ slotid ].disp_path : ti[ slotid ].lpPath ;
				//const TCHAR * pBkSlash = _tcsrchr( q, _T('\\') );
				//const TCHAR * pExe = ( pBkSlash && pBkSlash[ 1 ] ) ? pBkSlash + 1 : q ; 
				//size_t cchMem_Exe = _tcslen( pExe ) + 1;
				//ti[ slotid ].lpExe = TcharAlloc( cchMem_Exe );
				//if( ! ti[ slotid ].lpExe ) break;
				//_tcscpy_s( ti[ slotid ].lpExe, cchMem_Exe, pExe );

				ti[ slotid ].wCycle = pathInfo[ path_info_index ].wCycle;
				ti[ slotid ].wDelay = pathInfo[ path_info_index ].wDelay;

				ti[ slotid ].fWatch = true;
				ti[ slotid ].fUnlimited = false;
				ti[ slotid ].fRealTime = fRealTime;
			
				int iSlider = pathInfo[ path_info_index ].slider;

				if( SLIDER_MIN <= iSlider && iSlider <= SLIDER_MAX )
				{
					if( ti[ slotid ].disp_path )
						SetSliderIni( ti[ slotid ].disp_path, iSlider );
				}
				else
				{
					iSlider = GetSliderIni( q, hWnd );
				}

				g_Slider[ slotid ] = iSlider;
			}

			if( fx != nFound )
			{
				MessageBox(hWnd,_T("Error2"),APP_NAME,MB_ICONSTOP);
				
				for( fx = 0; fx <= nFound; ++fx )
				{
					int slotid = arSlots[ fx ];
					ResetTi( ti[ slotid ] );
					ReleaseSemaphore( hSemaphore[ slotid ], 1L, NULL );
				}
				break;
			}

			HANDLE hSync
				= CreateEvent(
					NULL,  // default security descriptor
					TRUE,  // manual-reset: once signaled, remains signaled until manually reset
					FALSE, // initially non-signaled
					NULL   // created without a name
				);

			// a higher slots goes first; the "base" slot goes last and does fSync job
			for( fx = nFound - 1; 0 <= fx; --fx )
			{
				ptrdiff_t slotid = arSlots[ fx ];
				g_bHack[ slotid ] = TRUE;
				ti[ slotid ]._hSync = hSync;
				ti[ slotid ].fSync = ( fx == 0 );
				hChildThread[ slotid ] = CreateThread2( &Hack, (void *) &ti[ slotid ] );
				if( ! hChildThread[ slotid ] ) break;
			}

			if( 0 <= fx )
			{
				if( hSync ) SetEvent( hSync );
				for( fx = 0; fx < nFound; ++fx )
				{
					ptrdiff_t slotid = arSlots[ fx ];
					g_bHack[ slotid ] = FALSE;
					if( hChildThread[ slotid ] )
					{
						CloseHandle( hChildThread[ slotid ] );
						hChildThread[ slotid ] = NULL;
					}
					else
					{
						ReleaseSemaphore( hSemaphore[ slotid ], 1L, NULL );
						ResetTi( ti[ slotid ] );
					}
				}

				MessageBox( hWnd, TEXT("CreateThread failed!"), APP_NAME, MB_OK|MB_ICONHAND );
				if( hSync )
				{
					Sleep( 500 );
					CloseHandle( hSync );
					hSync = NULL;
				}
				break;
			}

			SendMessage( hWnd, WM_USER_STOP, JUST_UPDATE_STATUS, STOPF_LESS_FLICKER );
			if( hSync )
			{
				Sleep( 500 );
				CloseHandle( hSync );
				hSync = NULL;
			}
		}
#if 0
		else // CASE 3: ProcessID given; Slot 2 is free
		{
			dwNewTarget = ti[ 2 ].dwProcessId;
			_stprintf_s( str, _countof(str), _T("Watching: PID %lu given"), dwNewTarget );
			WriteDebugLog( str );
		}

		if( dwNewTarget )
		{
			if( IsProcessBES( dwNewTarget ) )
			{
				fBES = true;
				break;
			}

			if( dwPID ) // non-first instance in multi-watch
			{
				for( slotid = 2; 0 <= slotid; --slotid )
					if( ! ti[ slotid ].fUnlimited && ! g_bHack[ slotid ] ) break;
				
				if( slotid == -1 )
				{
					for( slotid = 4; slotid < MAX_SLOTS; ++slotid )
						if( ! g_bHack[ slotid ] && ! ti[ slotid ].fUnlimited ) break;
				}
				
				if( slotid == MAX_SLOTS )
					dwNewTarget = 0;
			}
			else
			{
				slotid = 2;
			}
		}

		if( dwNewTarget )
		{
			if( WaitForSingleObject( hSemaphore[ slotid ], 1000 ) == WAIT_OBJECT_0 )
			{
				ti[ slotid ].dwProcessId = dwNewTarget;
			}
			else
			{
				dwNewTarget = 0;

				if( ! dwPID ) // single-watch
				{
					g_bHack[ slotid ] = FALSE; // ?
					MessageBox( hWnd, TEXT("Sema Error in LimitPlus()"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
					goto EXIT_THREAD;
				}
			}
		}

		if( dwNewTarget )
		{
			//##TODO##??
			// if already-limited (active OR fUnlimited), stop the old limiting; this is needed especially when
			// slot 2 is used, then freed, and now about to be reused
			for( i = 0; i < MAX_SLOTS; ++i )
			{
				if( i == 3 || i == slotid )
					continue;

				if( ti[ i ].dwProcessId == dwNewTarget )
				{
					if( g_bHack[ i ] )
					{
						SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, 0 );
						if( i < 3 )
						{
							for( ptrdiff_t j = (4*i); j < (4*i) + 4; ++j )
								ppszStatus[ j ][ 0 ] = _T('\0');
						}
					}
					ResetTi( ti[ i ] );
				}
			}
		}

		if( ! dwNewTarget )
		{
			if( ! ti[ 2 ].dwProcessId && _tcscmp( ppszStatus[ 13 + yy ], _T( "Nothing new" ) ) != 0 )
			{
				_tcscpy_s( ppszStatus[ 13 + yy ], cchStatus, _T( "Nothing new" ) );
				InvalidateRect( hWnd, NULL, FALSE );
			}
		}
		else
		{
			_sntprintf_s( str, _countof(str), _TRUNCATE, _T( "Watching : New target found! <%s>" ), szTargetPath );
			WriteDebugLog( str );

			SYSTEMTIME st;
			GetLocalTime( &st );
			_stprintf_s( ppszStatus[ 13 + yy ], cchStatus, 
							_T( "%02d:%02d:%02d Found [PID %lu]" ),
							(int) st.wHour,
							(int) st.wMinute,
							(int) st.wSecond,
							ti[ slotid ].dwProcessId );
			
			ResetTi( ti[ slotid ] );
			ti[ slotid ].dwProcessId = dwNewTarget;
			
			size_t cchMem_Path = _tcslen( szTargetPath ) + 1;
			ti[ slotid ].lpPath = TcharAlloc( cchMem_Path );
			if( ! ti[ slotid ].lpPath )
			{
				ReleaseSemaphore( hSemaphore[ slotid ], 1L, NULL );
				break;
			}
			_tcscpy_s( ti[ slotid ].lpPath, cchMem_Path, szTargetPath );

			if( lpPath && ! StrEqualNoCase( lpPath, szTargetPath ) )
			{
				ti[ slotid ].disp_path = lpPath; // old .disp_path is already freed in ResetTi
				lpPath = NULL; // not to be freed here
			}

			const TCHAR * q = ti[ slotid ].disp_path ? ti[ slotid ].disp_path : ti[ slotid ].lpPath ;
			const TCHAR * pBkSlash = _tcsrchr( q, _T('\\') );
			const TCHAR * pExe = ( pBkSlash && pBkSlash[ 1 ] ) ? pBkSlash + 1 : q ; 
			size_t cchMem_Exe = _tcslen( pExe ) + 1;
			ti[ slotid ].lpExe = TcharAlloc( cchMem_Exe );
			if( ! ti[ slotid ].lpExe )
			{
				ReleaseSemaphore( hSemaphore[ slotid ], 1L, NULL );
				break;
			}
			_tcscpy_s( ti[ slotid ].lpExe, cchMem_Exe, pExe );

			ti[ slotid ].fWatch = true;
			ti[ slotid ].fUnlimited = false;
			ti[ slotid ].fRealTime = false;
			
			if( found_index < 4 )
			{
				int iSlider = pathInfo[ found_index ].slider; //(int)( pathInfo[ found_index ].pszPath[ s ] & 0xff );
				if( SLIDER_MIN <= iSlider && iSlider <= SLIDER_MAX )
				{
					g_Slider[ slotid ] = iSlider;
					SetSliderIni( ti[ slotid ].lpExe, iSlider );
				}
			}

			SendMessage( hWnd, WM_USER_HACK, (WPARAM) slotid, (LPARAM) &ti[ slotid ] );

			// UpdateStatus not needed here; it'll be done on WM_USER_HACK
			// UpdateStatus( hWnd );
			fInitialUpdate = false;

			if( fWatchMulti ) // look for another instance now, without waiting
			{
				// But don't start another thread too soon even if there is another target,
				// to avoid possible race conditions
				Sleep( 50 );
				continue;
			}
		}
#endif


//###########Todo

		if( fInitialUpdate )
		{
			UpdateStatus( hWnd );
			fInitialUpdate = false;
		}
#if 0x1
		UINT kMax = 80;
		HMENU hMenu = GetMenu( hWnd );
		if( GetMenuState( hMenu, IDM_WATCH_RT2, MF_BYCOMMAND ) & MF_CHECKED )
			kMax = 20;
		else if( GetMenuState( hMenu, IDM_WATCH_RT4, MF_BYCOMMAND ) & MF_CHECKED )
			kMax = 40;
#else
		UINT kMax = GetPrivateProfileInt( _T("Options"), _T("WatchRT"), 80, GetIniPath() );
		if( 80 < kMax )
			kMax = 80;
		
		if( kMax < 20 )
			kMax = 20;
#endif

#ifdef _DEBUG
		TCHAR s[100];swprintf_s(s,100,L"kMax=%u\n",kMax);OutputDebugString(s);
#endif
		for( UINT k = 0; k < kMax; ++k ) // wait for a few seconds
		{
			Sleep( 100 );
			if( g_bHack[ 3 ] == FALSE || g_bHack[ 3 ] == UNWATCH_NOW )
				goto EXIT_THREAD;
		}


		/*if( lpPath )
		{
			HeapFree( GetProcessHeap(), 0, lpPath );
			lpPath = NULL;
		}*/
	}

EXIT_THREAD:
	WriteDebugLog( TEXT( "Watch: Exiting Thread..." ) );

	timeEndPeriod( 1 );

	if( arSlots )
	{
		MemFree( arSlots );
		arSlots = NULL;
	}

	Found_Clear( arFound, nFound );

//	if( lpPath )
//	{
//		HeapFree( GetProcessHeap(), 0, lpPath );
//		lpPath = NULL;
//	}


	ptrdiff_t n = 0;
	for( ; n < MAX_WATCH; ++n )
	{
		if( pathInfo[ n ].pszPath )
		{
			TcharFree( pathInfo[ n ].pszPath );
			pathInfo[ n ].pszPath = NULL;
		}
	}

	if( WaitForSingleObject( hReconSema, 3000 ) == WAIT_OBJECT_0 )
	{
		for( n = 0; n < MAX_WATCH; ++n )
		{
			if( g_arPathInfo[ n ].pszPath )
			{
				TcharFree( g_arPathInfo[ n ].pszPath );
				g_arPathInfo[ n ].pszPath = NULL;
			}
		}
		g_nPathInfo = 0;
		ReleaseSemaphore( hReconSema, 1L, NULL );
#ifdef _DEBUG
		OutputDebugString(_T("\tg_arPathInfo cleaned\n"));
#endif
	}

	// If watching idly, Slot 2 is now freed here, as 'unwatch' has been posted;
	// otherwise, Slot 2 should be active now.
	//if( g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
	if( ! g_bHack[ 2 ] && ! ti[ 2 ].fUnlimited )
	{
		ti[ 2 ].dwProcessId = 0;

#ifdef _DEBUG
		DWORD wait = WaitForSingleObject( hSemaphore[ 2 ], 0 );
		if(!wait)ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
		OutputDebugString(wait?_T("\n\nSema2 is used\n\n"):_T("\n\nSema2 is free\n\n"));
#endif
	}

	ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );

	_tcscpy_s( ppszStatus[ 13 ], cchStatus, _T( "* Not watching *" ) );
	//SetLongStatusText( szTargetPath, ppszStatus[ 14 ] );

	ppszStatus[ 14 ][ 0 ] = _T('\0');
	ppszStatus[ 15 ][ 0 ] = _T('\0');
	ppszStatus[ 16 ][ 0 ] = _T('\0');
	ppszStatus[ 17 ][ 0 ] = _T('\0');

	if( fBES )
	{
		SendMessage( hWnd, WM_COMMAND, IDM_STOP, 0 );
		_tcscpy_s( ppszStatus[ 16 ], cchStatus, _T( "* Denied *" ) );
		_tcscpy_s( ppszStatus[ 17 ], cchStatus, _T("BES Can't Target BES"));
	}

	InvalidateRect( hWnd, NULL, TRUE );

	//if( ! g_bHack[ 2 ] )
	//	EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED ); // ?

	EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED ); // ?

	g_bHack[ 3 ] = FALSE;

	UpdateStatus( hWnd );

	if( hToken )
	{
		if( bPriv ) 
			AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}
	return NOT_WATCHING;
}


BOOL SelectWatch( HWND hWnd, TARGETINFO * pti )
{
	const TCHAR * strIniPath = GetIniPath();
	TCHAR szInitialDir[ CCHPATH ] = _T("");
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "WatchDir" ),
		NULL,
		szInitialDir,
		(DWORD) _countof(szInitialDir),
		strIniPath
	);
	
	if( szInitialDir[ 0 ] == _T('\0')
		|| GetFileAttributes( szInitialDir ) == INVALID_FILE_ATTRIBUTES )
	{
		if( ! SUCCEEDED( SHGetFolderPath( NULL, 
								CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, szInitialDir ) ) ) 
		{
			GetCurrentDirectory( (DWORD) _countof(szInitialDir), szInitialDir );
		}
	}
	
	TCHAR szTargetPath[ CCHPATH ] = TEXT( "" );
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = TEXT( "Application (*.exe)\0*.exe\0All Files\0*.*\0\0" );
	ofn.lpstrInitialDir = szInitialDir;
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = TEXT( "exe" );
	ofn.lpstrFile = szTargetPath;
	ofn.nMaxFile = (DWORD) _countof(szTargetPath);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
#ifdef _UNICODE
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
#else
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
#endif
	ofn.lpstrTitle = TEXT( "Watch" );

	if( ! GetOpenFileName( &ofn ) )
	{
		return FALSE;
	}

	if( 1 < ofn.nFileOffset && ofn.nFileOffset < _countof(szInitialDir) )
	{
		_tcsncpy_s( szInitialDir, _countof(szInitialDir), szTargetPath, (rsize_t) ofn.nFileOffset - 1 );
		WritePrivateProfileString(
			TEXT( "Options" ), 
			TEXT( "WatchDir" ),
			szInitialDir,
			strIniPath
		);
	}

#if 0 // ANSIFIX(2/5)
	_tcscpy_s( lpszTargetPath, _countof(lpszTargetPath), lpszTargetExe );
#endif

	TCHAR szTargetLongPath[ CCHPATH ];
	DWORD dwcch = GetLongPathName( szTargetPath, szTargetLongPath, (DWORD) _countof(szTargetLongPath ) );
	if( ! dwcch || _countof(szTargetLongPath) <= dwcch )
		_tcscpy_s( szTargetLongPath, _countof(szTargetLongPath), szTargetPath );

	return SetTargetPlus( hWnd, pti, szTargetLongPath, GetSliderIni( szTargetLongPath, g_hWnd ) );
}

BOOL SetTargetPlus( HWND hWnd, TARGETINFO * pti, const TCHAR * pszTargetPath, int iSlider, const int * aiParams )
{
#ifdef _DEBUG
	//pti[2].dwProcessId=0;
#endif
	if( ! pszTargetPath ) return FALSE;

	SetSliderIni( pszTargetPath, iSlider );

	if( g_bHack[ 3 ] )
	{
		size_t cchPath = _tcslen( pszTargetPath );
		size_t cchMem = cchPath + 256;
		TCHAR * lpMem = TcharAlloc( cchMem );
		if( ! lpMem ) return FALSE;

		_stprintf_s( lpMem, cchMem, _T("bes.exe -J \"%s\" %d;%d;%d"), pszTargetPath, iSlider,
										aiParams ? aiParams[ 0 ] : 0,
										aiParams ? aiParams[ 1 ] : 0 );

		bool fAllowMoreThan99 = IsMenuChecked( hWnd, IDM_ALLOWMORETHAN99 );
		BOOL bRet = HandleJobList( hWnd, lpMem, fAllowMoreThan99, pti );
		TcharFree( lpMem );
		//MessageBox( hWnd, TEXT("Busy3"), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
		return bRet;
	}

	DWORD dwPID_LimitThisFirst = 0;

	// We always use Slot 2 to start watching. TODO: This should be changed
	// This is possible only when this func is called from SelectWatch
	if( g_bHack[ 2 ] )
	{
		ptrdiff_t free_slot = 0;
		bool fRelocate = !( pti[ 2 ].disp_path && StrEqualNoCase( pti[ 2 ].disp_path, pszTargetPath )
							||
							pti[ 2 ].lpPath && StrEqualNoCase( pti[ 2 ].lpPath, pszTargetPath ) );

		if( fRelocate )
		{
			// Get a ticket for the guy at Slot 2; they must move to a new place
			for( free_slot = 0; free_slot < MAX_SLOTS; ++free_slot )
			{
				if( free_slot == 2 || free_slot == 3 ) continue;
				
				if( ! g_bHack[ free_slot ]
					&& ! pti[ free_slot ].fUnlimited
					&& WaitForSingleObject( hSemaphore[ free_slot ], 100 ) == WAIT_OBJECT_0 ) break;
			}

			if( free_slot == MAX_SLOTS )
			{
				MessageBox( hWnd, TEXT("No free slot"), APP_NAME, MB_OK | MB_ICONSTOP );
				return FALSE;
			}

			// copy the info of the target, which is now at slot 2
			if( ! TiCopyFrom( pti[ free_slot ], pti[ 2 ] ) )
			{
				ReleaseSemaphore( hSemaphore[ free_slot ], 1L, NULL );
				return FALSE;
			}

			g_Slider[ free_slot ] = g_Slider[ 2 ];
		}
		else
		{
			dwPID_LimitThisFirst = pti[ 2 ].dwProcessId;
		}
		SendMessage( hWnd, WM_USER_STOP, (WPARAM) 2, STOPF_NO_LV_REFRESH );

		// Wait until slot 2 is free again...
		// NOTE: Between the above SendMessage and this Wait, sema2 may be taken by
		// another thread.  Basically, we can prevent this problem by using PostMessage;
		// however, since this function is executed on the main thread, doing so would
		// block the main thread in the following Wait.  For now, let's say it's ok
		// even if another thread gets the sema (which will just cause the Sema2a error).
		if( WaitForSingleObject( hSemaphore[ 2 ], 3000 ) != WAIT_OBJECT_0 )
		{
			MessageBox( hWnd, TEXT("Sema2a"), APP_NAME, MB_ICONSTOP | MB_OK );
			ReleaseSemaphore( hSemaphore[ free_slot ], 1L, NULL );
			return FALSE;
		}

		if( fRelocate )
		{
			// Don't release hSemaphore[ free_slot ]; it'll be used by the ex-slot2 target
			SendMessage( g_hWnd, WM_USER_HACK, (WPARAM) free_slot, (LPARAM) &pti[ free_slot ] );
		}

		ResetTi( pti[ 2 ] );
	}
	else
	{
		if( WaitForSingleObject( hSemaphore[ 2 ], 3000 ) != WAIT_OBJECT_0 )
		{
			MessageBox( hWnd, TEXT("Sema2b"), APP_NAME, MB_ICONSTOP | MB_OK );
			return FALSE;
		}
	}

	if( WaitForSingleObject( hSemaphore[ 3 ], 100 ) != WAIT_OBJECT_0 )
	{
		ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
		MessageBox( hWnd, TEXT("Sema3"), APP_NAME, MB_OK | MB_ICONSTOP );
		return FALSE;
	}


#ifdef _DEBUG
if(pti[2].slotid!=2) MessageBox(NULL,_T("slotid!=2"),NULL,MB_ICONSTOP|MB_TOPMOST);
pti[2].slotid=2;
#endif

	size_t cchMem_Path;
	if( pszTargetPath != NULL )
	{
		ResetTi( pti[ 2 ] );
		pti[ 2 ].dwProcessId = dwPID_LimitThisFirst;

//		const TCHAR * pBkSlash = _tcsrchr( pszTargetPath, _T('\\') );
//		const TCHAR * pExe = ( pBkSlash && pBkSlash[ 1 ] ) ? pBkSlash + 1 : pszTargetPath ;
//		size_t cchMem_Exe = _tcslen( pExe ) + 1;
//		pti[ 2 ].lpExe = TcharAlloc( cchMem_Exe );
//		if( ! pti[ 2 ].lpExe )
//		{
//			ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
//			ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );
//			return FALSE;
//		}
//		_tcscpy_s( pti[ 2 ].lpExe, cchMem_Exe, pExe );

		cchMem_Path = _tcslen( pszTargetPath ) + 1; // +2 for star slider
		pti[ 2 ].lpPath = TcharAlloc( cchMem_Path );
		
		if( pti[ 2 ].lpPath )
			_tcscpy_s( pti[ 2 ].lpPath, cchMem_Path, pszTargetPath );
	}

	if( ! pti[ 2 ].lpPath )
	{
		ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
		ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );
		return FALSE;
	}

	if( WaitForSingleObject( hReconSema, 3000 ) == WAIT_OBJECT_0 )
	{
		if( g_nPathInfo == 0 ) // TODO
		{
			if( g_arPathInfo[ 0 ].pszPath ) TcharFree( g_arPathInfo[ 0 ].pszPath );
			ZeroMemory( &g_arPathInfo[ 0 ], sizeof(PATHINFO) );

			const TCHAR * pStar = _tcschr( pszTargetPath, _T('*') );
			if( pStar )
			{
				g_arPathInfo[ 0 ].cchHead = (size_t)( pStar - pszTargetPath );
				g_arPathInfo[ 0 ].cchTail = _tcslen( pStar + 1 );
			}
			g_arPathInfo[ 0 ].cchPath = _tcslen( pszTargetPath );
			g_arPathInfo[ 0 ].slider = iSlider;

			size_t cchMem = g_arPathInfo[ 0 ].cchPath + 1;
			g_arPathInfo[ 0 ].pszPath = TcharAlloc( cchMem );
			if( g_arPathInfo[ 0 ].pszPath )
				_tcscpy_s( g_arPathInfo[ 0 ].pszPath, cchMem, pszTargetPath );

			if( aiParams != NULL )
			{
				if( aiParams[ 0 ] != -1 ) g_arPathInfo[ 0 ].wCycle = (WORD) aiParams[ 0 ]; // update if param(s) are defined & valid
				if( aiParams[ 1 ] != -1 ) g_arPathInfo[ 0 ].wDelay = (WORD) aiParams[ 1 ];
			}

			g_nPathInfo = 1;
		}
		ReleaseSemaphore( hReconSema, 1L, NULL );

		if( aiParams != NULL && g_nPathInfo == 1 )
		{
			TCHAR dbgstr[ 128 ] = _T("");
			_stprintf_s( dbgstr, _countof(dbgstr), _T("cycle=%d, delay=%d @ SetTargetPlus"), aiParams[0], aiParams[1] );
			WriteDebugLog( dbgstr );
		}
	}
	else
	{
		ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
		ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );
		return FALSE;
	}

	pti[ 2 ].fWatch = true; // ?

//	g_bHack[ 2 ] = TRUE; // 1.7.0.11@20140323 Don't set this flag for consistency.
	g_bHack[ 3 ] = TRUE;
	ReleaseSemaphore( hSemaphore[ 2 ], 1L, NULL );
	hChildThread[ 3 ] = CreateThread2( &LimitPlus, pti );

	if( ! hChildThread[ 3 ] )
	{
		pti[ 2 ].fWatch = false; // ?
		ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );
//		g_bHack[ 2 ] = FALSE;
		g_bHack[ 3 ] = FALSE;
		
		for( ptrdiff_t n = 0; n < g_nPathInfo; ++n )
		{
			if( g_arPathInfo[ n ].pszPath != NULL )
			{
				TcharFree( g_arPathInfo[ n ].pszPath );
				g_arPathInfo[ n ].pszPath = NULL;
			}
		}
		g_nPathInfo = 0;
		
		MessageBox( hWnd, TEXT( "CreateThread failed" ), APP_NAME, MB_OK | MB_ICONSTOP );
		return FALSE;
	}

	return TRUE;
}


//BOOL Unwatch( HANDLE& hSema, HANDLE& hThread, volatile BOOL& bWatch )
BOOL Unwatch( TARGETINFO * rgTargetInfo )
{
	BOOL bSuccess = FALSE;

	for( ptrdiff_t i = 0; i < MAX_SLOTS; ++i )
	{
		if( i == 3 ) continue;
		rgTargetInfo[ i ].fUnlimited = false; // ?? TODO
		rgTargetInfo[ i ].fWatch = false;
	}

	g_bHack[ 3 ] = UNWATCH_NOW;
	if( WaitForSingleObject( hSemaphore[ 3 ], 3000 ) == WAIT_OBJECT_0 )
	{
		if( hChildThread[ 3 ] )
		{
			WriteDebugLog( TEXT( "Closing hChildThread[ 3 ]" ) );
			CloseHandle( hChildThread[ 3 ] );
			hChildThread[ 3 ] = NULL;
		}
		ReleaseSemaphore( hSemaphore[ 3 ], 1L, NULL );

		bSuccess = TRUE;
	}
	else
	{
		WriteDebugLog( TEXT( "[!] Sema3 Error" ) );
	}

	return bSuccess;
}

BOOL HandleJobList( HWND hWnd, const TCHAR * lpszCommandLine, bool fAllowMoreThan99, TARGETINFO * ti )
{
	TCHAR * arTargetPaths[ MAX_WATCH ];
	int arSlider[ MAX_WATCH ];
	int arCycle[ MAX_WATCH ];
	int arDelay[ MAX_WATCH ];
	ptrdiff_t job_index;
	ptrdiff_t first_job = 0;

	// Not technically needed; just zero-init to play it safe
	for( job_index = 0; job_index < MAX_WATCH; ++job_index )
	{
		arTargetPaths[ job_index ] = NULL;
		arSlider[ job_index ] = 0;
		arCycle[ job_index ] = 0;
		arDelay[ job_index ] = 0;
	}
	
	const ptrdiff_t numOfPaths = ParseJobList( lpszCommandLine, arTargetPaths, 
										&arSlider[ 0 ],
										&arCycle[ 0 ],
										&arDelay[ 0 ],
										MAX_WATCH, fAllowMoreThan99 );

	if( ! numOfPaths ) return FALSE;

	// v1.7.5
	ptrdiff_t numOfWatches = numOfPaths;
	for( job_index = 0; job_index < numOfPaths; ++job_index )
	{
		if( arTargetPaths[ job_index ] == NULL ) break; // this never happens

		const DWORD pid = ParsePID( arTargetPaths[ job_index ] );
		if( pid )
		{
			int aiParams[ 2 ];
			aiParams[ 0 ] = arCycle[ job_index ];
			aiParams[ 1 ] = arDelay[ job_index ];
			LimitPID( hWnd, &ti[ 0 ], pid, arSlider[ job_index ], FALSE, &aiParams[ 0 ] );

			TcharFree( arTargetPaths[ job_index ] ); // this "path" is done
			arTargetPaths[ job_index ] = NULL;
			--numOfWatches;
		}
	}

	if( numOfWatches > 0 )
	{
		PATHINFO arNewTargets[ MAX_WATCH ];
		ZeroMemory( &arNewTargets[ 0 ], sizeof(PATHINFO) * MAX_WATCH );
		
		// Load the paths onto arNewTargets
		for( job_index = 0; job_index < numOfPaths; ++job_index )
		{
			if( arTargetPaths[ job_index ] == NULL ) continue; // path was "pid:xxxx"

			const TCHAR * pStar = _tcschr( arTargetPaths[ job_index ], _T('*') );
			if( pStar )
			{
				arNewTargets[ job_index ].cchHead = (size_t)( pStar - arTargetPaths[ job_index ] );
				arNewTargets[ job_index ].cchTail = _tcslen( pStar + 1 );
			}
			arNewTargets[ job_index ].cchPath = _tcslen( arTargetPaths[ job_index ] );
			arNewTargets[ job_index ].slider = arSlider[ job_index ];
			if( arCycle[ job_index ] != -1 ) arNewTargets[ job_index ].wCycle = (WORD) arCycle[ job_index ];
			if( arDelay[ job_index ] != -1 ) arNewTargets[ job_index ].wDelay = (WORD) arDelay[ job_index ];
			
			// Just a local copy of a pointer, arTargetPaths[ job_index ], which will be freed at the end of this function
			arNewTargets[ job_index ].pszPath = arTargetPaths[ job_index ];

			if( arSlider[ job_index ] != -1 && ! pStar )
				SetSliderIni( arTargetPaths[ job_index ], arSlider[ job_index ] );
		}

		// (1) Update Sliders and/or ReLimit if needed
		for( ptrdiff_t sx = 0; sx < MAX_SLOTS; ++sx )
		{
			if( sx != 3 && ( g_bHack[ sx ] || ti[ sx ].fUnlimited ) ) // Something is going on in this slot
			{
				const TCHAR * q = ti[ sx ].disp_path ? ti[ sx ].disp_path : ti[ sx ].lpPath ;
				
				for( job_index = 0; job_index < numOfPaths; ++job_index )
				{
					if( arTargetPaths[ job_index ] == NULL ) continue; // path was "pid:xxxx"

					// If this 'something' is related to one of my watch-jobs, I'll have to update it and restart it
					if( PathEqual( arNewTargets[ job_index ], q, _tcslen( q ), false ) )
					{
						if( arSlider[ job_index ] != -1 ) g_Slider[ sx ] = arSlider[ job_index ];
						ti[ sx ].fUnlimited = false;
						if( arCycle[ job_index ] != -1 ) ti[ sx ].wCycle = (WORD) arCycle[ job_index ];
						if( arDelay[ job_index ] != -1 ) ti[ sx ].wDelay = (WORD) arDelay[ job_index ];
					}
				}
			}
		}

		// (2) Update the g_arPathInfo table (esp. if the target is new)
		if( WaitForSingleObject( hReconSema, 1000 ) == WAIT_OBJECT_0 )
		{
			for( job_index = 0; job_index < numOfPaths; ++job_index )
			{
				if( arTargetPaths[ job_index ] == NULL ) continue; // path was "pid:xxxx"

				ptrdiff_t ig = 0; // index of g_arPathInfo
				
				// Determine the right value of ig
				for( ; ig < g_nPathInfo && ig < MAX_WATCH; ++ig )
				{
					if( StrEqualNoCase( arNewTargets[ job_index ].pszPath, g_arPathInfo[ ig ].pszPath ) )
					{
						//TcharFree( g_arPathInfo[ ig ].pszPath ); // ig < MAX_WATCH
						//g_arPathInfo[ ig ].pszPath = NULL;
						break;
					}
				}

				if( ig < MAX_WATCH )
				{
					// Duplicate the path string: this is needed to avoid a problem between [#1] and [#2] below.
					size_t cchNewMem = arNewTargets[ job_index ].cchPath + 1;
					TCHAR * lpNewMem = TcharAlloc( cchNewMem );

					if( lpNewMem )
					{
						// A safe (unshared) memory block; the same string as arTargetPaths[ job_index ] will be stored here
						_tcscpy_s( lpNewMem, cchNewMem, arNewTargets[ job_index ].pszPath );

						if( ig < g_nPathInfo ) // This target is already in the table
						{
							// [#1] Free the old memory block; the pointer is unshared, and NOT one of arTargetPaths, so freeing it is ok
							if( g_arPathInfo[ ig ].pszPath ) TcharFree( g_arPathInfo[ ig ].pszPath );
							g_arPathInfo[ ig ].pszPath = NULL;
						}
						else // This target is new; not previously in the table
						{
							++g_nPathInfo;
						}

						// Don't update Cycle/Delay but use the old values, if the new value is undefined/invalid
						if( arCycle[ job_index ] == -1 ) arNewTargets[ job_index ].wCycle = g_arPathInfo[ ig ].wCycle;
						if( arDelay[ job_index ] == -1 ) arNewTargets[ job_index ].wDelay = g_arPathInfo[ ig ].wDelay;

						memcpy( &g_arPathInfo[ ig ], &arNewTargets[ job_index ], sizeof(PATHINFO) );

						// At this point, g_arPathInfo[ ig ].pszPath is shared with arTargetPaths[ job_index ]; we'll fix this problem
						// by using a new, unshared pointer
						g_arPathInfo[ ig ].pszPath = lpNewMem;

						if( ig == 0 ) first_job = job_index; // ? TODO: test
					}
				}
			}
			
			ReleaseSemaphore( hReconSema, 1L, NULL );
		}

		if( ! g_bHack[ 3 ] ) // Currently the Watch thread is not running - TODO: does this mean g_nPathInfo was 0 before?
		{
			ti[ 2 ].dwProcessId = TARGET_PID_NOT_SET; // (0) just in case

			// [#2] We use arTargetPaths[ first_job ] here; not shared in g_arPathInfo, so this pointer is not suddenly freed
			// NOTE: It's ok even if the target is an instance of BES.
			// Watch and Hack both can handle such a situation properly.
			int aiParams[ 2 ];
			aiParams[ 0 ] = arCycle[ first_job ];
			aiParams[ 1 ] = arDelay[ first_job ];
			SetTargetPlus( hWnd, ti, arTargetPaths[ first_job ], arSlider[ first_job ], &aiParams[ 0 ] );
		}
	}

	// Free memory blocks allocated in ParseJobList if not yet freed
	for( job_index = 0; job_index < MAX_WATCH; ++job_index )
	{
		if( arTargetPaths[ job_index ] != NULL ) 
		{
			TcharFree( arTargetPaths[ job_index ] );
			arTargetPaths[ job_index ] = NULL;
		}
	}

	return TRUE;
}