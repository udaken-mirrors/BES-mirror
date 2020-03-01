#include "BattleEnc.h"
#include "Trackbar.h"

extern HWND g_hWnd;
extern HWND g_hSettingsDlg;
extern HANDLE hSemaphore[ 4 ];
extern TCHAR g_szTarget[ 3 ][ CCHPATHEX ];

extern volatile DWORD g_dwTargetProcessId[ 4 ];
extern volatile int g_Slider[ 4 ];
extern volatile BOOL g_bHack[ 4 ];

volatile UINT g_uUnit;

BOOL IsMenuItemEnabled( HWND hWnd, const UINT uMenuId );
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );

static int Hack_Worker( LPVOID lpVoid );

BOOL SetDebugPrivilege( HANDLE hToken, BOOL bEnable, DWORD * pPrevAttributes );
BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );

unsigned __stdcall Hack( LPVOID lpVoid )
{
	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

	timeBeginPeriod( 1 );

	int ret = Hack_Worker( lpVoid );

	timeEndPeriod( 1 );

	return (unsigned) ret;
}

void WriteIni_Time( const TCHAR * pExe );
static int Hack_Worker( LPVOID lpVoid )
{
	TCHAR msg[ 4096 ];
	LPHACK_PARAMS lphp = (LPHACK_PARAMS) lpVoid;
	if( ! lphp || ! lphp->lpTarget )
		return NORMAL_TERMINATION;

	const LPTARGETINFO lpTarget = lphp->lpTarget;
	WriteIni_Time( lpTarget->szExe );

	const int iMyId = lphp->iMyId;

	if( iMyId < 0 || 4 <= iMyId )
		return NORMAL_TERMINATION;

	LPTSTR * ppszStatus = lphp->lpszStatus;
	DWORD dwTargetProcessId = lpTarget->dwProcessId;

	ptrdiff_t iPrevThreadCount = 0;
	unsigned int errorflags = 0;
#define ERR_SUSPEND 1
#define ERR_RESUME  2

//	UINT menuState = GetMenuState( GetMenu( g_hWnd ), IDM_DEBUGPRIVILEGE, MF_BYCOMMAND );
//	const bool fDebugPrivilege = ( menuState != (UINT) -1 && menuState & MF_CHECKED );

	RECT minirect; SetRect( &minirect, 20L, 20L + 90L * iMyId, 479L, 40L + 90L * iMyId );
	RECT hackrect; SetRect( &hackrect, 20L, 20L + 90L * iMyId, 479L, 100L + 90L * iMyId );

	TCHAR lpszEnemyPath[ MAX_PATH * 2 ] = _T("");
	TCHAR lpszEnemyExe[ MAX_PATH * 2 ] = _T("");
	_tcscpy_s( lpszEnemyPath, _countof(lpszEnemyPath), lpTarget->szPath );
	_tcscpy_s( lpszEnemyExe, _countof(lpszEnemyExe), lpTarget->szExe );

	HANDLE hProcess = NULL;
	DWORD dwOldPriority = NORMAL_PRIORITY_CLASS;
	int iOpenThreadRetry = 0;
	int iSuspendThreadRetry = 0;
	int iResumeThreadRetry = 0;
	SYSTEMTIME st;

	ptrdiff_t i;
	for( i = 0 + iMyId * 4; i < 4 + iMyId * 4; ++i )
	{
		ppszStatus[ i ][ 0 ] = _T('\0');
	}

	_tcscpy_s( ppszStatus[ 2 + iMyId * 4 ], cchStatus, lpszEnemyPath );

#if !defined( _UNICODE )
	// for watching...
	if( iMyId == 2 && strlen( lpszEnemyPath ) >= 19 )
	{
		ppszStatus[ 2 + iMyId * 4 ][ 15 ] = '\0';
		PathToExeEx( ppszStatus[ 2 + iMyId * 4 ], MAX_PATH * 2 );
	}
#endif

	AdjustLength( ppszStatus[ 2 + iMyId * 4 ] );

#ifdef _UNICODE
	const TCHAR mark = _T('\x25CF');
#else
	const TCHAR mark = _T('*');
#endif

	HANDLE hToken = NULL;
	BOOL bPriv = FALSE;
	DWORD dwPrevAttributes = 0L;
	
	if( g_bHack[ iMyId ] )
	{
//		if( fDebugPrivilege )
			bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );
		
		hProcess = OpenProcess(	PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION,
								FALSE, dwTargetProcessId );
		
		if( hProcess )
		{
//TerminateProcess(hProcess, 0x3);
			dwOldPriority = GetPriorityClass( hProcess );
			_stprintf_s( msg, _countof(msg),
						_T( "Target #%d: \"%s\"\r\n\tProcess ID = 0x%08lX : hProcess = 0x%p (Priority: %lu)" ),
						iMyId + 1, lpszEnemyPath, dwTargetProcessId, hProcess, dwOldPriority );
			WriteDebugLog( msg );
			if( lpTarget->fRealTime )
			{
				WriteDebugLog( SetPriorityClass( hProcess, IDLE_PRIORITY_CLASS )
								? TEXT( "SetPriorityClass OK" )
								: TEXT( "SetPriorityClass failed" ) );
			}
		}
		else // !hProcess
		{
			WriteDebugLog( TEXT( "OpenProcess failed." ) );

			if( g_bHack[ 3 ] )
			{
				g_bHack[ 3 ] = UNWATCH_NOW;
				g_bHack[ 2 ] = FALSE;
			}

			GetLocalTime( &st );
			_tcscpy_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus, _T( "Process ID = n/a" ) );
			_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus, _T( "%02d:%02d:%02d Initial OpenThread failed" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LPLONG) NULL ) )
			{
				_stprintf_s( msg, _countof(msg),
								_T( "* ReleaseSemaphore #%d in Hack()" ),
								iMyId + 1 );
				WriteDebugLog( msg );
			}
			else
			{
				_stprintf_s( msg, _countof(msg),
							_T( "[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu" ),
							iMyId + 1, GetLastError() );
				WriteDebugLog( msg );
			}
			lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET;

			if( PathToProcessID( lpszEnemyPath ) == (DWORD) -1 )
			{
				PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, TARGET_MISSING );
				InvalidateRect( g_hWnd, NULL, TRUE );

				if( hToken )
				{
					if( bPriv ) 
						SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

					CloseHandle( hToken );
				}

				return TARGET_MISSING;
			}
			else
			{
				PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, THREAD_NOT_OPENED );
				InvalidateRect( g_hWnd, NULL, TRUE );

				if( hToken )
				{
					if( bPriv ) 
						SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

					CloseHandle( hToken );
				}

				return THREAD_NOT_OPENED;
			}
		}

		TCHAR buf[ 100 ];
		_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
					_T( "Target #%d [ %s ]" ),
					iMyId + 1,
					GetPercentageString( g_Slider[ iMyId ], buf, _countof(buf) ) );
	}

	bool fSuspended = false;
	
	for( DWORD dwOuterLoops = 0L;
			g_bHack[ iMyId ];
			dwOuterLoops < 0xFFFFffff ? ++dwOuterLoops : dwOuterLoops = 1L )
	{
		ptrdiff_t numOfThreads = 0;
		DWORD * pThreadIDs = ListProcessThreads_Alloc( dwTargetProcessId, &numOfThreads );

		if( ! pThreadIDs )
			break;

		// ----------------------- @@@
		if( numOfThreads == 0 )
		{
			GetLocalTime( &st );
			_tcscpy_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus, _T( "Process ID = n/a" ) );
			_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus, _T("%02d:%02d:%02d No threads"),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			
			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			SetPriorityClass( hProcess, dwOldPriority );
			CloseHandle( hProcess );

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LPLONG) NULL ) )
			{
				_stprintf_s( msg, _countof(msg), 
							_T( "* TARGET_MISSING #%d : ReleaseSemaphore in Hack()" ), iMyId + 1 );
				WriteDebugLog( msg );
			}
			else
			{
				_stprintf_s( msg, _countof(msg),
							_T( "[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu" ),
							iMyId + 1, GetLastError() );
				WriteDebugLog( msg );
			}

			WriteDebugLog( TEXT("PostMessage Doing...") );

			PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, TARGET_MISSING );

			WriteDebugLog( TEXT("PostMessage Done!") );
			lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET;

			HeapFree( GetProcessHeap(), 0L, pThreadIDs );

			if( hToken )
			{
				if( bPriv ) 
					SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

				CloseHandle( hToken );
			}

			return TARGET_MISSING;
		}

		if( dwOuterLoops == 0L )
		{
			iPrevThreadCount = numOfThreads;
			_stprintf_s( msg, _countof(msg), _T( "Target #%d : iThreadCount = %3d" ),
							iMyId + 1, (int) numOfThreads );
			WriteDebugLog( msg );
		}
		else if( dwOuterLoops % 50UL == 0UL )
		{
			_stprintf_s( msg, _countof(msg),
						_T( "Target #%d : Loops %5lu, ThreadCnt %3d (Prev %3d)" ), 
						iMyId + 1, dwOuterLoops, (int) numOfThreads, (int) iPrevThreadCount );
			WriteDebugLog( msg );
		}

		_stprintf_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus,
					_T( "Process ID = 0x%04X %04X [ %d Thread%s ]" ),
					HIWORD( dwTargetProcessId ), LOWORD( dwTargetProcessId ), (int) numOfThreads,
					( numOfThreads != 1 ) ? _T("s") : _T("") );
		GetLocalTime( &st );
		if( numOfThreads != iPrevThreadCount || errorflags )
		{
			_stprintf_s( msg, _countof(msg),
						_T( "Target #%d : iThreadCount = %3d (Prev = %d)" ),
						iMyId + 1, (int) numOfThreads, (int) iPrevThreadCount );
			WriteDebugLog( msg );
			iPrevThreadCount = numOfThreads;
			errorflags = 0;
			_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
						_T( "%02d:%02d:%02d Target Re-Locked On: OK" ),
						(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
		}
		else
		{
			_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus, _T( "%02d:%02d:%02d %s: OK" ),
						(int) st.wHour, (int) st.wMinute, (int) st.wSecond,
						dwOuterLoops == 0L ? _T( "Started" ) : _T( "Calm" ) );
		}

		HANDLE * pThreadHandles = (HANDLE *) HeapAlloc( GetProcessHeap(), 
														HEAP_ZERO_MEMORY, 
														(SIZE_T)( sizeof(HANDLE) * numOfThreads ) );

		if( ! pThreadHandles )
		{
			HeapFree( GetProcessHeap(), 0L, pThreadIDs );
			break;
		}

		ptrdiff_t numOfOpenedThreads = 0;
		for( i = 0; i < numOfThreads; ++i )
		{
			pThreadHandles[ i ] = OpenThread( THREAD_SUSPEND_RESUME, FALSE, pThreadIDs[ i ] );
			if( ! pThreadHandles[ i ] )
			{
				_stprintf_s( msg, _countof(msg),
							_T( "[!] Target #%d : OpenThread failed: Thread #%03d, ThreadId 0x%08lX" ),
							iMyId + 1, (int) i + 1, pThreadIDs[ i ] );
				WriteDebugLog( msg );
			}
			else
			{
				++numOfOpenedThreads;
			}
		}

		// ###----------------
		if( numOfOpenedThreads == 0
			||
			numOfOpenedThreads != numOfThreads && iOpenThreadRetry > 10
			||
			iSuspendThreadRetry > 100
			||
			iResumeThreadRetry > 50			
		)
		{
			for( i = 0; i < numOfThreads; ++i ) //@20120513
			{
				if( pThreadHandles[ i ] )
					CloseHandle( pThreadHandles[ i ] );
			}

			GetLocalTime( &st );
			if( iResumeThreadRetry > 50 )
			{
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T( "%02d:%02d:%02d ResumeThread Error" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
				// to do
				// 'unfreeze'
			}
			else if( iSuspendThreadRetry > 100 )
			{
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T( "%02d:%02d:%02d SuspendThread Error" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			}
			else
			{
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T( "%02d:%02d:%02d OpenThread Error" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			}

			WriteDebugLog( ppszStatus[ 3 + iMyId * 4 ] );
			WriteDebugLog( TEXT( "### Giving up... ###" ) );

			lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET; //?
			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			SetPriorityClass( hProcess, dwOldPriority );
			CloseHandle( hProcess );

			// This is needed as a workaround ? (Possibly this works as a workaround) 1.0 beta6
			UpdateStatus( g_hWnd );

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL ) )
			{
				_stprintf_s( msg, _countof(msg), 
							_T( "* THREAD_NOT_OPENED #%d : ReleaseSemaphore in Hack()" ),
							iMyId + 1 );
				WriteDebugLog( msg );
			}

			BOOL bMissing = ( PathToProcessID( lpszEnemyPath ) == (DWORD) -1 );

			if( ! bMissing && g_bHack[ 3 ] )
			{
				g_bHack[ 3 ] = UNWATCH_NOW;
				g_bHack[ 2 ] = FALSE;
			}
			
			WriteDebugLog( TEXT("PostMessage Doing...") );

			PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId,
							bMissing ? TARGET_MISSING : THREAD_NOT_OPENED );

			WriteDebugLog( TEXT("PostMessage Done!") );

			lphp->lpTarget->dwProcessId = TARGET_PID_NOT_SET;

			InvalidateRect( g_hWnd, NULL, TRUE );

			HeapFree( GetProcessHeap(), 0L, pThreadHandles );
			HeapFree( GetProcessHeap(), 0L, pThreadIDs );

			if( hToken )
			{
				if( bPriv ) 
					SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

				CloseHandle( hToken );
			}

			return bMissing ? TARGET_MISSING : THREAD_NOT_OPENED ;
		}
		else if( numOfOpenedThreads != numOfThreads )
		{
			for( i = 0; i < numOfThreads; ++i )
			{
				if( pThreadHandles[ i ] )
					CloseHandle( pThreadHandles[ i ] );
			}

			++iOpenThreadRetry;
			
			_stprintf_s( msg, _countof(msg), 
						_T( "@ Couldn't open some threads: Retrying #%d..." ), iOpenThreadRetry );
			WriteDebugLog( msg );
			Sleep( 100UL );
		
			HeapFree( GetProcessHeap(), 0L, pThreadHandles );
			HeapFree( GetProcessHeap(), 0L, pThreadIDs );
			continue;
		}

		iOpenThreadRetry = 0;
		// non-volatile snapshot of bHack
		const bool fActive1 = ( g_bHack[ 0 ] == TRUE );
		const bool fActive2 = ( g_bHack[ 1 ] == TRUE );
		const bool fActive3 = ( g_bHack[ 2 ] == TRUE && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE );

		if( fActive1 || fActive2 || fActive3 )
		{
			TCHAR strStatus[ 1024 ] = TEXT( " Limiting CPU load:" );

#ifdef _UNICODE
			if( IS_JAPANESE )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_2000, -1, strStatus, (int) _countof(strStatus) );
			else if( IS_FINNISH )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_2000, -1, strStatus, (int) _countof(strStatus) );
			else if( IS_SPANISH )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_2000, -1, strStatus, (int) _countof(strStatus) );
			else if( IS_CHINESE_T )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000T, -1, strStatus, (int) _countof(strStatus) );
			else if( IS_CHINESE_S )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000S, -1, strStatus, (int) _countof(strStatus) );
			else if( IS_FRENCH )
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_2000, -1, strStatus, (int) _countof(strStatus) );
#endif
				
			if( fActive1 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #1" ) );
				
			if( fActive2 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #2" ) );
				
			if( fActive3 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #3" ) );
		
			_tcsncpy_s( ppszStatus[ 12 ], cchStatus, strStatus, 256 );
		}
		else
		{
			ppszStatus[ 12 ][ 0 ] = _T('\0');
		}

		InvalidateRect( g_hWnd, &hackrect, FALSE );

		DWORD msElapsed = 0L;
		DWORD sElapsed = 0L;
		UINT numOfReports = 0;
#ifdef _DEBUG
		DWORD dwInnerLoops = 0L;
#endif

#ifdef _DEBUG
		while( msElapsed < 3 * 1024 )
#else
		while( msElapsed < 10 * 1024 ) // about 10 seconds
#endif
		{
#ifdef _DEBUG
			++dwInnerLoops;
#endif
			DWORD TimeRed = 0L;
			DWORD TimeGreen = 100L;
			UINT msUnit = UNIT_DEF;

			const int slider = g_Slider[ iMyId ];
			if( 1 <= slider && slider <= 99 )
			{
				msUnit = g_uUnit;
				if( !( UNIT_MIN <= msUnit && msUnit <= UNIT_MAX ) ) msUnit = UNIT_DEF;
				TimeRed = (DWORD)( (double) msUnit / 100.0 * (double) slider + 0.5 );
				TimeGreen = ( msUnit - TimeRed );
				TimeRed = __max( 1L, TimeRed );
				TimeGreen = __max( 1L, TimeGreen );

				// Situation like Unit=2;Red=1;Green=2;ActualUnit=3 is ok, because it satisfies
				// the condition that we should awake at least once in 2 ms.
				// Situation like Unit=2;Red=2;Green=1;ActualUnit=3 is not ok.
				if( msUnit == TimeRed && 2UL <= TimeRed )
				{
					--TimeRed;
				}

#ifdef _DEBUG
				if(dwInnerLoops==1u){
					WCHAR s[100];
					swprintf_s(s,100,L"slider=%d: Red=%lu Green=%lu\n", slider, TimeRed, TimeGreen );
					OutputDebugStringW(s);
				}

#endif
			}
#if SLIDER_MIN == 0
			else if( slider == 0 )
			{
				TimeGreen = msUnit;
				TimeRed = 0L;
			}
#endif
			else
			{
				if( slider == 100 ) TimeRed = 110L; // 99.1% 110 / 111
				else if( slider == 101 ) TimeRed = 124L; // 99.2% 124 / 125
				else if( slider == 102 ) TimeRed = 124L; // 99.3% 142 / 143
				else if( slider == 103 ) TimeRed = 166L; // 99.4% 166 / 167
				else if( slider == 104 ) TimeRed = 199L; // 99.5% 199 / 200
				else if( slider == 105 ) TimeRed = 249L; // 99.6% 249 / 250
				else if( slider == 106 ) TimeRed = 332L; // 99.7% 332 / 333
				else if( slider == 107 ) TimeRed = 499L; // 99.8% 499 / 500
				else if( slider == 108 ) TimeRed = 999L; // 99.9% 999 / 1000
#if ALLOW100
				else if( slider == 109 ) TimeRed = 1000L;
#endif

#if ALLOW100
				TimeGreen = ( slider != 109 ) ? 1ul : 0ul ;
#else
				TimeGreen = 1L;
#endif
			}

			if( ! fSuspended )
			{
				for( i = 0; i < numOfThreads; ++i )
				{
					if( pThreadHandles[ i ] && SuspendThread( pThreadHandles[ i ] ) == (DWORD) -1 )
					{
						errorflags |= ERR_SUSPEND;
						_stprintf_s( msg, _countof(msg),
									_T( "Target #%d : SuspendThread failed (Thread #%03d) : Retry=%d" ),
									iMyId + 1, (int) i + 1, iSuspendThreadRetry );
						WriteDebugLog( msg );
					}
				}
				fSuspended = true;
			}

			if( errorflags & ERR_SUSPEND )
			{
				iSuspendThreadRetry++;
			}
			else
			{
				iSuspendThreadRetry = 0;
			}

#if SLIDER_MIN == 0
			if( TimeRed != 0 ) Sleep( TimeRed );
#else
			Sleep( TimeRed );
#endif
			msElapsed += TimeRed;
			
			// if !g_bHack[ iMyId ], the outer loop will break after this step;
			// in that case, threads must be resumed even if limited 100% ie TimeGreen==0.
			if( fSuspended && ( TimeGreen || ! g_bHack[ iMyId ] ) )
			{
				for( i = 0; i < numOfThreads; ++i )
				{
					if( pThreadHandles[ i ] && ResumeThread( pThreadHandles[ i ] ) == (DWORD) -1 )
					{
						errorflags |= ERR_RESUME;
						_stprintf_s( msg, _countof(msg),
									_T( "Target #%d : ResumeThread failed: Thread #%03d, ThreadId 0x%08lX, iResumeThreadRetry=%d" ),
									iMyId + 1, (int) i + 1, pThreadIDs[ i ], iResumeThreadRetry );
						WriteDebugLog( msg );
					}
				}
				fSuspended = false;
			}

			if( errorflags & ERR_RESUME )
			{
				iResumeThreadRetry++;
			}
			else
			{
				iResumeThreadRetry = 0;
			}

			if( ! g_bHack[ iMyId ] || errorflags )
			{
				break;
			}
			
#if ALLOW100
			if( TimeGreen )	Sleep( TimeGreen );
#else
			Sleep( TimeGreen );
#endif
			msElapsed += TimeGreen;
		
			if( msElapsed >> 10 != sElapsed ) // report once in a second (1024 ms)
			{
				TCHAR buf[ 100 ];
#if 0//def _DEBUG
				_stprintf_s( buf, _countof(buf),
							_T("Out#=%lu (msElapsed=%lu In#=%lu) unit=%u red=%lu green=%lu\r\n"),
							dwOuterLoops, msElapsed, dwInnerLoops,
							msUnit, TimeRed, TimeGreen );
				OutputDebugString( buf );
#endif
				sElapsed = msElapsed >> 10;

				_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
							_T( "Target #%d [ %s ] %c" ), iMyId + 1,
							GetPercentageString( slider, buf, _countof(buf) ),
							( numOfReports & 1 ) ? _T(' ') : mark );
				InvalidateRect( g_hWnd, &minirect, 0 );
				++numOfReports;
			}
		}

		for( i = 0; i < numOfThreads; ++i )
		{
			if( pThreadHandles[ i ] )
				CloseHandle( pThreadHandles[ i ] );
		}

		HeapFree( GetProcessHeap(), 0L, pThreadHandles );
		HeapFree( GetProcessHeap(), 0L, pThreadIDs );
	}

	_tcscpy_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus, _T( "* Unlimited *" ) );

	if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LONG *) NULL ) )
	{
		_stprintf_s( msg, _countof(msg),
					_T( "* Target #%d : ReleaseSemaphore in Hack()" ), iMyId + 1 );
		WriteDebugLog( msg );
	}
	else
	{
		_stprintf_s( msg, _countof(msg),
					_T( "[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu" ),
					iMyId + 1, GetLastError() );
		WriteDebugLog( msg );
	}

	const BOOL bPriRestored = SetPriorityClass( hProcess, dwOldPriority );
	WriteDebugLog( bPriRestored ? _T( "Set dwOldPriority: OK" ) : _T( "Set dwOldPriority: Failed" ) );
	CloseHandle( hProcess );

	ppszStatus[ 3 + iMyId * 4 ][ 0 ] = _T('\0');

	if( hToken )
	{
		if( bPriv ) 
			SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}
	
	return NORMAL_TERMINATION;
}



TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString )
{
	if( 1 <= lTrackbarPos && lTrackbarPos <= 99 )
	{
#ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %2ld%%", lTrackbarPos );
#else
		sprintf_s( lpString, cchString, "- %2ld%%", lTrackbarPos );
#endif
	}
#if ALLOW100
	else if( lTrackbarPos == SLIDER_MAX )
	{
# ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %2ld%%", 100L );
# else
		sprintf_s( lpString, cchString, "- %2ld%%", 100L );
# endif
	}
#endif
	else
	{
		double d = 0.0;
		if( 100 <= lTrackbarPos && lTrackbarPos <= SLIDER_MAX )
		{
			d = 99.0 + ( lTrackbarPos - 99L ) * 0.1;
		}
#ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %4.1f%%", d );
#else
		sprintf_s( lpString, cchString, "- %4.1f%%", d );
#endif
	}

	return lpString;
}



static inline BOOL SetSliderText( HWND hDlg, int iControlId, LONG lPercentage )
{
	TCHAR strPercent[ 32 ];
	
	return SetDlgItemText( hDlg, iControlId, 
							GetPercentageString( lPercentage, strPercent, _countof(strPercent) ) );
}


#define SliderMoved(wParam)     (LOWORD(wParam)!=TB_ENDTRACK)

static void _settings_init( HWND hDlg )
{
	const bool fAllowMoreThan99 = 
				!!( MF_CHECKED & GetMenuState( GetMenu( g_hWnd ), 
					IDM_ALLOWMORETHAN99, MF_BYCOMMAND ) );
	const int iMax = fAllowMoreThan99 ? SLIDER_MAX : 99 ;

	TCHAR tmpstr[ 32 ];
	for( int i = 0; i < 3; i++ )
	{
		SetDlgItemText( hDlg, IDC_EDIT_TARGET1 + i, g_szTarget[ i ] );
		_stprintf_s( tmpstr, _countof(tmpstr), _T( "%s #&%d" ),
					g_bHack[ i ] ? _T( "Unlimit" ) : _T( "Limit" ), i + 1 );
		SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + i, tmpstr );
		EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_STOP1 + i ), 
						( g_dwTargetProcessId[ i ] != TARGET_PID_NOT_SET ) );
		EnableWindow( GetDlgItem( hDlg, IDC_SLIDER1 + i ), 
						( g_dwTargetProcessId[ i ] != TARGET_PID_NOT_SET ) );
		EnableWindow( GetDlgItem( hDlg, IDC_TEXT_PERCENT1 + i ), 
						( g_dwTargetProcessId[ i ] != TARGET_PID_NOT_SET ) );

		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETRANGE, TRUE, MAKELONG( SLIDER_MIN, iMax ) );
		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETPAGESIZE, 0, 5 );
		
		const int iSlider = g_Slider[ i ];
		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETPOS, TRUE, iSlider );
		SetSliderText( hDlg, IDC_TEXT_PERCENT1 + i , iSlider );
	}

	if( g_dwTargetProcessId[ 2 ] == (DWORD) -1 || g_bHack[ 3 ] )
	{
		SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + 2, TEXT( "(Watching)" ) );
		EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_STOP1 + 2 ), FALSE );
		EnableWindow( GetDlgItem( hDlg, IDC_TEXT_PERCENT1 + 2 ), TRUE );
	}

	if( !( UNIT_MIN <= g_uUnit && g_uUnit <= UNIT_MAX ) ) g_uUnit = UNIT_DEF;
	SetDlgItemInt( hDlg, IDC_EDIT_UNIT, g_uUnit, FALSE );

	PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
}

INT_PTR CALLBACK Settings( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static LPTSTR * ppszStatus = NULL;
	static TCHAR * lpszWindowText = NULL;
	static HFONT hfontNote = NULL;
	static HFONT hfontPercent = NULL;
	static HFONT hfontUnit = NULL;

#if defined(_MSC_VER) && 1400 <= _MSC_VER
	//__assume( message == WM_INITDIALOG || hWnd != NULL );
#endif

	switch (message)
	{
		case WM_INITDIALOG:
		{
			lpszWindowText = (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_WINDOWTEXT * sizeof(TCHAR) );
			if( ! lParam || ! lpszWindowText )
			{
				EndDialog( hDlg, FALSE );
				break;
			}
			g_hSettingsDlg = hDlg;

			ppszStatus = (TCHAR**) lParam;
			
			HDC hDC = GetDC( hDlg );
			hfontPercent = MyCreateFont( hDC, TEXT( "Verdana" ), 12, TRUE, FALSE );
			hfontUnit = MyCreateFont( hDC, TEXT( "Verdana" ), 11, FALSE, FALSE );
			ReleaseDC( hDlg, hDC );

#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					IS_JAPANESEo ? S_JPNo_1002 : S_JPN_1002,
					-1, lpszWindowText, MAX_WINDOWTEXT - 1 );
				SetWindowText( hDlg, lpszWindowText );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1002,
					-1, lpszWindowText, MAX_WINDOWTEXT - 1 );
				SetWindowText( hDlg, lpszWindowText );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1002,
					-1, lpszWindowText, MAX_WINDOWTEXT - 1 );
				SetWindowText( hDlg, lpszWindowText );
			}
			else
#endif
			{
				_tcscpy_s( lpszWindowText, MAX_WINDOWTEXT, _T( "Limiter control" ) );
				SetWindowText( hDlg, lpszWindowText );
			}

			for( int i = 0; i < 3; i++ )
			{
				for( long lPos = 10L; lPos <= 90L; lPos += 10L )
				{
					SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETTIC, 0U, lPos );
				}

				PrevTrackbarProc[ i ] = SetSubclass( GetDlgItem( hDlg, IDC_SLIDER1 + i ),
														MyTrackbarProc[ i ] );

				SendDlgItemMessage( hDlg, IDC_TEXT_PERCENT1 + i, WM_SETFONT, 
									(WPARAM) hfontPercent, MAKELPARAM( FALSE, 0 ) );
			}
			SendDlgItemMessage( hDlg, IDC_EDIT_UNIT, WM_SETFONT, (WPARAM) hfontUnit, 0 );

			LOGFONT lf = {0};
			lf.lfCharSet = ANSI_CHARSET;
			_tcscpy_s( lf.lfFaceName, _countof(lf.lfFaceName), _T("Verdana") );
			lf.lfHeight = 17;
			lf.lfWeight = FW_NORMAL;
			lf.lfItalic = TRUE;
			hfontNote = CreateFontIndirect( &lf );
			SendDlgItemMessage( hDlg, IDC_EDIT_NOTE, WM_SETFONT, (WPARAM) hfontNote, 0 );
#ifdef _UNICODE
			SetDlgItemText( hDlg, IDC_EDIT_NOTE,
				L"NOTE:  You will specify a negative percentage here.  For example \x2212" L"25%"
				L" means that the target will only get 75% of the CPU time it would"
				L" normally get.  By going to \x2212" L"30%, \x2212" L"35%, \x2212" L"40%,\x2026 you will"
				L" THROTTLE MORE, making the target SLOWER."
			);
#else
			SetDlgItemText( hDlg, IDC_EDIT_NOTE,
				TEXT("NOTE:  You will specify a negative percentage here.  For example -25%" )
				TEXT(" means that the target will only get 75% of the CPU time it would" )
				TEXT(" normally get.  By going to -30%, -35%, -40%,... you will" )
				TEXT(" THROTTLE MORE, making the target SLOWER." )
			);
#endif
			_settings_init( hDlg );
			break;
		}

		case WM_CTLCOLOREDIT:
			if( GetDlgCtrlID( (HWND) lParam ) == IDC_EDIT_UNIT )
			{
				const INT unit = (INT) GetDlgItemInt( hDlg, IDC_EDIT_UNIT, NULL, TRUE );
				HDC hDC = (HDC) wParam;
				SetBkMode( hDC, OPAQUE );
				SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
				SetTextColor( hDC, ( UNIT_MIN <= unit && unit <= UNIT_MAX )
									? RGB( 0, 0, 0xff ) : RGB( 0xff, 0, 0 ) );
				return (INT_PTR)(HBRUSH) GetSysColorBrush( COLOR_WINDOW );
			}
			return (INT_PTR)(HBRUSH) NULL;
			break;
		
		case WM_CTLCOLORSTATIC:
		{
			if( (HWND) lParam == GetDlgItem( hDlg, IDC_EDIT_NOTE ) )
			{
				HDC hDC = (HDC) wParam;
				SetBkMode( hDC, TRANSPARENT );
				SetBkColor( hDC, GetSysColor( COLOR_3DFACE ) );
				SetTextColor( hDC, RGB( 0x00, 0x66, 0x33 ) );
				return (INT_PTR)(HBRUSH) GetSysColorBrush( COLOR_3DFACE );
			}

			for( int i = 0; i < 3; i++ )
			{
				HWND hEdit = GetDlgItem( hDlg, IDC_TEXT_PERCENT1 + i );
				if( (HWND) lParam == hEdit )
				{
					HDC hDC = (HDC) wParam;
					SetBkMode( hDC, TRANSPARENT );
					SetBkColor( hDC, GetSysColor( COLOR_3DFACE ) );
					
					int pos = (int) SendDlgItemMessage( hDlg, IDC_SLIDER1 + i,
														TBM_GETPOS, 0U, 0 );
					
					if( 1 <= pos && pos <= 99 )
					{
						int x = (int)(( pos - 1 ) * 255.0 / 98.0);

						SetTextColor( hDC, RGB( (x), 0, (255-x/2) ) );
					}
					else
					{
						SetTextColor( hDC, RGB( 0xff, 0, 0 ) );
					}

					return (INT_PTR) (HBRUSH) GetSysColorBrush( COLOR_3DFACE );
				}
			}
			return (INT_PTR)(HBRUSH) NULL;
		}
		
		case WM_HSCROLL:
		{
			if( ! SliderMoved( wParam ) || ! ppszStatus )
				break;

			const int id = GetDlgCtrlID( (HWND) lParam );
			const int k = id - IDC_SLIDER1;
			if( 0 <= k && k <= 2 )
			{
				LRESULT lSlider = SendMessage( (HWND) lParam, TBM_GETPOS, 0, 0 );
				if( lSlider < SLIDER_MIN || lSlider > SLIDER_MAX )
					lSlider = SLIDER_DEF;

				g_Slider[ k ] = (int) lSlider;

				TCHAR strPercent[ 32 ];
				GetPercentageString( lSlider, strPercent, _countof(strPercent) );
				SetDlgItemText( hDlg, IDC_TEXT_PERCENT1 + k, strPercent );
				
				if( g_bHack[ k ]
					&& ( k < 2 || k == 2 && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE ) )
				{
					_stprintf_s( ppszStatus[ 0 + 4 * k ], cchStatus,
								_T( "Target #%d [ %s ]" ),
								k + 1,
								strPercent );

					RECT rcStatus;
					SetRect( &rcStatus, 20, 20 + 90 * k, 479, 40 + 90 * k );
					InvalidateRect( g_hWnd, &rcStatus, FALSE );
				}
			}
			break;
		}

		case WM_COMMAND:
		{
			const int id = (int) LOWORD( wParam );
			switch( id )
			{
				case IDOK:
					EndDialog( hDlg, TRUE );
					break;

				case IDCANCEL:
					EndDialog( hDlg, FALSE );
					break;
				
				case IDC_BUTTON_STOP1:
				case IDC_BUTTON_STOP2:
				case IDC_BUTTON_STOP3:
				{
					const int k = id - IDC_BUTTON_STOP1;
					const bool fActive = ! g_bHack[ k ]; // new state when toggled
					TCHAR strBtnText[ 32 ];
					_stprintf_s( strBtnText, _countof(strBtnText),
								_T("%s #&%d"),
								fActive ? _T("Unlimit") : _T("Limit"),
								k + 1 );

					SendMessage( g_hWnd, fActive ? WM_USER_RESTART : WM_USER_STOP, (WPARAM) k, 0 );
					SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + k, strBtnText );
					break;
				}

				case IDC_EDIT_UNIT:
				{
					if( HIWORD( wParam ) == EN_CHANGE )
					{
						INT newUnit = (INT) GetDlgItemInt( hDlg, IDC_EDIT_UNIT, NULL, TRUE );
						if( newUnit == 0 )
							newUnit = UNIT_DEF;
						else
							newUnit = __max( UNIT_MIN, __min( newUnit, UNIT_MAX ) );

						g_uUnit = (UINT) newUnit;
					}
					break;
				}
				
				default:
					break;
			}
			break; // <-- fixed @ 1.1 beta3
		}

		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			if( wParam == 0 && lpszWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpszWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0U, (LPARAM) lpszWindowText );
			}

			if( message == WM_USER_CAPTION )
				break;

			return FALSE;
			break;
		}

		case WM_NCUAHDRAWCAPTION:
		{
			if( lpszWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpszWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0U, (LPARAM) lpszWindowText );
			}

			return FALSE;
			break;
		}

		case WM_USER_REFRESH:
			_settings_init( hDlg );
			break;
		
		case WM_DESTROY:
		{
			g_hSettingsDlg = NULL;
			
			if( hfontUnit )
			{
				SendDlgItemMessage( hDlg, IDC_EDIT_UNIT, WM_SETFONT, 
								(WPARAM)(HFONT) NULL, MAKELPARAM( FALSE, 0 ) );
				DeleteFont( hfontUnit );
				hfontUnit = NULL;
			}
			
			for( int i = 0; i < 3; ++i )
			{
				SendDlgItemMessage( hDlg, IDC_TEXT_PERCENT1 + i, WM_SETFONT,
									(WPARAM)(HFONT) NULL, MAKELPARAM( FALSE, 0 ) );

				UnsetSubclass( hDlg, IDC_SLIDER1 + i, PrevTrackbarProc[ i ] );

				if( g_dwTargetProcessId[ i ] != TARGET_PID_NOT_SET )
				{
					TCHAR buf[ CCHPATHEX + 100 ];
#ifdef _DEBUG
					_stprintf_s( buf, _countof(buf), _T( "g_Slider[ %d ] = %d : %s" ),
									i, g_Slider[ i ], g_szTarget[ i ] );
					WriteDebugLog( buf );
#endif
					_tcscpy_s( buf, _countof(buf), g_szTarget[ i ] );
					
					if( i == 2 && g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
					{
						size_t len = _tcslen( buf );
						if( 11 < len
							&& _tcscmp( &buf[ len - 11 ], _T(" (Watching)") ) == 0 )
						{
							buf[ len - 11 ] = _T('\0');
						}
					}
					
					SetSliderIni( buf, g_Slider[ i ] );
				}
			}
			
			if( hfontNote )
			{
				SendDlgItemMessage( hDlg, IDC_EDIT_NOTE, WM_SETFONT,
									(WPARAM)(HFONT) NULL, MAKELPARAM( FALSE, 0 ) );

				DeleteFont( hfontNote );
				hfontNote = NULL;
			}
			
			if( hfontPercent )
			{
				DeleteFont( hfontPercent );
				hfontPercent = NULL;
			}

			if( lpszWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpszWindowText );
				lpszWindowText = NULL;
			}
			
			break;
		}

		default:
			return FALSE;

	}
    return TRUE;
}

BOOL Unfreeze( HWND hWnd, DWORD dwProcessID )
{
	ptrdiff_t numOfThreads = 0;
	DWORD * pThreadIDs = ListProcessThreads_Alloc( dwProcessID, &numOfThreads );
	
	if( ! pThreadIDs )
		return FALSE;

	TCHAR line[ 128 ];
	const size_t cchMessage = ( (size_t) numOfThreads + 1 ) * 128;

	TCHAR * lpMessage = (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
											cchMessage * sizeof(TCHAR) );
	if( ! lpMessage )
	{
		HeapFree( GetProcessHeap(), 0L, pThreadIDs );
		return FALSE;
	}

	BOOL bSuccess = TRUE;
	_stprintf_s( lpMessage, cchMessage, _T( "%d thread%s:\r\n" ),
					(int) numOfThreads,
					numOfThreads != 1 ? _T("s") : _T("") );

	HANDLE hToken = NULL;
	DWORD dwPrevAttributes = 0L;
	BOOL bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	for( ptrdiff_t i = 0; i < numOfThreads; ++i )
	{
		HANDLE hThread = OpenThread( THREAD_SUSPEND_RESUME, FALSE, pThreadIDs[ i ] );
		if( hThread )
		{
			const DWORD dwPreviousSuspendCount = SuspendThread( hThread );
			if( dwPreviousSuspendCount == (DWORD) -1 )
			{
				_stprintf_s( line, _countof(line), 
							_T( "Thread #%3d (0x%08lX) opened. ERROR while checking suspend count.\r\n" ),
							(int) i + 1, pThreadIDs[ i ] );
				_tcscat_s( lpMessage, cchMessage, line );
				CloseHandle( hThread );
				continue;
			}
			
			_stprintf_s( line, _countof(line),
						_T( "Thread #%3d (0x%08lX) opened.\tPrevious suspend count = %lu\r\n" ),
						(int) i + 1, pThreadIDs[ i ], dwPreviousSuspendCount );
			_tcscat_s( lpMessage, cchMessage, line );

			const DWORD dwCurrentSuspendCount = dwPreviousSuspendCount + 1L;
			for( DWORD n = 0L; n < dwCurrentSuspendCount; ++n )
			{
				ResumeThread( hThread );
			}
			CloseHandle( hThread );
		}
		else
		{
			bSuccess = FALSE;

			_stprintf_s( line, _countof(line),
						_T( "ERROR: Thread #%d (0x%08lX) NOT opened.\r\n" ), 
						(int) i + 1, pThreadIDs[ i ] );
			_tcscat_s( lpMessage, cchMessage, line );
		}
	}
#ifdef _DEBUG
	OutputDebugString(lpMessage);
#endif

	if( hToken )
	{
		if( bPriv ) 
			SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}

	MessageBox( hWnd, lpMessage, TEXT( "Unfreezing : Status" ), MB_OK );
	HeapFree( GetProcessHeap(), 0L, lpMessage );
	HeapFree( GetProcessHeap(), 0L, pThreadIDs );
	
	return bSuccess;
}



/*
0123456789012345678901234567890123456789012345678 (cchOriginal=48)
dddddddddd~D      SsssssssssssssssssssssssssssssZ (cchToMove=31) D=dst, S=src
C:\Program Files\mozilla.org\Mozilla\mozilla.exe
*/

VOID AdjustLength( LPTSTR strPath )
{
	const size_t cchOriginal = _tcslen( strPath );
	if( 45 < cchOriginal )
	{
		TCHAR * dst = strPath + ( SURROGATE_LO( strPath[ 10 ] ) ? 11 : 10 );
#ifdef _UNICODE
		*dst++ = L'\x2026'; // 'HORIZONTAL ELLIPSIS' (U+2026)
#else
		*dst++ = '.';
		*dst++ = '.';
		*dst++ = '.';
#endif

		// Including +1 for term NUL
		const size_t cchToMove = (size_t)( SURROGATE_LO( strPath[ cchOriginal - 30 ] ) ? 32 : 31 );
		const TCHAR * src = strPath + cchOriginal - ( cchToMove - 1 ); // -1 to include NUL
		memmove( dst, src, cchToMove * sizeof(TCHAR) );
	}
}

static DWORD GetPrivilegeAttributes( HANDLE hToken, LUID * pLuid )
{
	HMODULE hModule = SafeLoadLibrary( _T("Advapi32.dll") );
	if( ! hModule ) return 0L;

	DWORD dwAttributes = 0L; // ret val

typedef BOOL (WINAPI *GetTokenInformation_t)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);

	GetTokenInformation_t Info = (GetTokenInformation_t)
								(void*) GetProcAddress( hModule, "GetTokenInformation" );

	DWORD cbPrivs = 0L;
	// GetTokenInformation returns FALSE in this case with ERROR_INSUFFICIENT_BUFFER (122);
	// MS sample assumes the same:
	// e.g. http://msdn.microsoft.com/en-us/library/windows/desktop/aa446670%28v=vs.85%29.aspx
	// Since this behavior is not well documented, I'll just check cbPrivs!=0
#ifdef Like_Microsoft_LogonSID_sample_code
	if( Info
		&& ! (*Info)( hToken, TokenPrivileges, NULL, 0L, &cbPrivs )
		&& GetLastError() == ERROR_INSUFFICIENT_BUFFER )
#else
	if( Info && (*Info)( hToken, TokenPrivileges, NULL, 0L, &cbPrivs ), cbPrivs )
#endif
	{
		TOKEN_PRIVILEGES * lpPrivs = (TOKEN_PRIVILEGES*) HeapAlloc( GetProcessHeap(),
																	0L,
																	cbPrivs );
		if( lpPrivs )
		{
			DWORD cbWritten;
			lpPrivs->PrivilegeCount = 0L;
			Info( hToken, TokenPrivileges, lpPrivs, cbPrivs, &cbWritten );
			for( DWORD n = 0L; n < lpPrivs->PrivilegeCount; ++n )
			{
				if( lpPrivs->Privileges[ n ].Luid.LowPart == pLuid->LowPart
					&& lpPrivs->Privileges[ n ].Luid.HighPart == pLuid->HighPart )
				{
					dwAttributes = lpPrivs->Privileges[ n ].Attributes;
					break;
				}
			}
			HeapFree( GetProcessHeap(), 0L, lpPrivs );
		}
	}
	
	FreeLibrary( hModule );

	return dwAttributes;
}

BOOL SetDebugPrivilege( HANDLE hToken, BOOL bEnable, DWORD * pPrevAttributes )
{
	WriteDebugLog( bEnable? _T( "SetDebugPrivilege TRUE" ) : _T( "SetDebugPrivilege FALSE" ) );

//	if( pPrevAttributes )
//		*pPrevAttributes = 0L;//--20131015
	
	HMODULE hModule = SafeLoadLibrary( _T("Advapi32.dll") );
	if( ! hModule ) return FALSE;

	BOOL bRet = FALSE;
	DWORD dwPrevAttributes = 0L;

typedef BOOL (WINAPI *LookupPrivilegeValue_t)(LPCTSTR,LPCTSTR,PLUID);
	LookupPrivilegeValue_t Lookup = (LookupPrivilegeValue_t)(void*)
									GetProcAddress( hModule, "LookupPrivilegeValue" WorA_ );

typedef BOOL (WINAPI *AdjustTokenPrivileges_t)(HANDLE,BOOL,PTOKEN_PRIVILEGES,
																	 DWORD,PTOKEN_PRIVILEGES,PDWORD);
	AdjustTokenPrivileges_t Adjust = (AdjustTokenPrivileges_t)(void*)
									GetProcAddress( hModule, "AdjustTokenPrivileges" );

	LUID Luid;
    if( Lookup && Adjust && (*Lookup)( NULL, SE_DEBUG_NAME, &Luid ) )
	{
		dwPrevAttributes = GetPrivilegeAttributes( hToken, &Luid );
		DWORD dwNewAttributes = 0L;
		if( bEnable )
		{
			dwNewAttributes = (dwPrevAttributes | SE_PRIVILEGE_ENABLED);
		}
		else
		{
			if( pPrevAttributes ) // todo: this is not needed at all
				dwNewAttributes = *pPrevAttributes;
			else
				dwNewAttributes = (dwPrevAttributes & ~((DWORD)SE_PRIVILEGE_ENABLED));
		}

		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1L;
		tp.Privileges[ 0 ].Luid = Luid;
		tp.Privileges[ 0 ].Attributes = dwNewAttributes;

		TOKEN_PRIVILEGES tpPrev = {0};
		DWORD cbPrev;
		
		SetLastError( 0L );
		(*Adjust)( hToken, FALSE, &tp, (DWORD) sizeof(tpPrev), &tpPrev, &cbPrev );
		
		// ret.val. of Adjust itself is not reliable; one needs to do this.
		bRet = ( GetLastError() == ERROR_SUCCESS );

#ifdef _DEBUG
		WCHAR s[100];
		swprintf_s(s,100,L"TID%08lX [Enable=%d] Attr %08lX [prev=%08lX] to %08lX (result=%lu err=%lu bRet=%d) TEST=%d\n",
			GetCurrentThreadId(),
			bEnable,
			dwPrevAttributes,
			tpPrev.Privileges[0].Attributes,
			dwNewAttributes,
			tpPrev.PrivilegeCount,
			GetLastError(),
			bRet,
			pPrevAttributes&&(dwPrevAttributes & ~((DWORD)SE_PRIVILEGE_ENABLED))==*pPrevAttributes);
		OutputDebugStringW(s);
#endif
	}

	if( pPrevAttributes )
		*pPrevAttributes = dwPrevAttributes;

	if( bRet )
		WriteDebugLog( _T( "Success!" ) );

	FreeLibrary( hModule );

	return bRet;
}

BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes )
{
	if( ! phToken ) return FALSE;

	HMODULE hModule = SafeLoadLibrary( _T("Advapi32.dll") );
	if( ! hModule ) return FALSE;

	BOOL bPriv = FALSE; // ret val

typedef BOOL (WINAPI *OpenThreadToken_t)(HANDLE,DWORD,BOOL,PHANDLE);
	OpenThreadToken_t OpenTT = (OpenThreadToken_t)(void*)
									GetProcAddress( hModule, "OpenThreadToken" );
typedef BOOL (WINAPI *ImpersonateSelf_t)(SECURITY_IMPERSONATION_LEVEL);
	ImpersonateSelf_t Impersonate = (ImpersonateSelf_t)(void*)
									GetProcAddress( hModule, "ImpersonateSelf" );

	SetLastError( 0L );
	BOOL bToken = OpenTT && (*OpenTT)( GetCurrentThread(), 
									TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, TRUE, phToken );
	if( bToken )
	{
		WriteDebugLog( _T( "OpenThreadToken OK" ) );
	}
	else
	{
		if( ERROR_NO_TOKEN == GetLastError() )
		{
			WriteDebugLog( _T( "ERROR_NO_TOKEN (No prob.)" ) );
		}
		else
		{
			TCHAR tmpstr[ 100 ];
			_stprintf_s( tmpstr, _countof(tmpstr),
						_T("OpenThreadToken failed, Error Code %lu"), GetLastError() );
			WriteDebugLog( tmpstr );
		}
		
      //if(           Impersonate && Impersonate( SecurityImpersonation ) ) //--20131015
		if( OpenTT && Impersonate && Impersonate( SecurityImpersonation ) ) //++
		{
			bToken = (*OpenTT)( GetCurrentThread(), 
								TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, phToken );
		}
		else
		{
			WriteDebugLog( _T( "ImpersonateSelf failed" ) );
		}
	}

	if( bToken )
	{
		WriteDebugLog( _T( "OpenThreadToken OK" ) );
		
		bPriv = SetDebugPrivilege( *phToken, TRUE, pPrevAttributes );
		WriteDebugLog( bPriv ? _T( "SetPrivilege OK" ) : _T( "SetPrivilege failed" ) );
	}
	else
	{
		*phToken = NULL;
	}

	FreeLibrary( hModule );
	return bPriv;
}

