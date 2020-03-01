/* 
 *	Copyright (C) 2005-2017 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"
#include "Trackbar.h"

extern HWND g_hWnd;
extern HWND g_hSettingsDlg;
extern HANDLE hSemaphore[ MAX_SLOTS ];
extern TCHAR * ppszStatus[ 18 ];

//extern volatile DWORD g_dwTargetProcessId[ MAX_SLOTS ];
extern volatile int g_Slider[ MAX_SLOTS ];
extern volatile BOOL g_bHack[ MAX_SLOTS ];

volatile UINT g_uUnit;

DWORD * AllocThreadList( DWORD dwOwnerPID, ptrdiff_t& numOfThreads ); // v1.7.1
void CachedList_Refresh( DWORD msMaxWait );
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );

BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );
void WriteIni_Time( const TCHAR * pExe );
bool ProcessExists( DWORD dwPID );
void SetLongStatusText( const TCHAR * szText, TCHAR * pszStatus );
static int Hack_Worker( void * pv );



void tokendebug( HANDLE hProcess );

unsigned __stdcall Hack( void * pv )
{
#ifdef _DEBUG
	LARGE_INTEGER li = {0};
	LARGE_INTEGER freq = {0};
	QueryPerformanceCounter(&li);
	QueryPerformanceFrequency(&freq);
	TCHAR s[100];
	_sntprintf_s(s,_countof(s),_TRUNCATE,_T("0x%x\t<<Hack Thread[%d]>>\t%f\n"),
			GetCurrentThreadId(),
			((TARGETINFO*)pv)->slotid,
			(double) li.QuadPart / freq.QuadPart * 1000.0);
	OutputDebugString(s);
#endif


	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL );

	timeBeginPeriod( 1 );

	int ret = Hack_Worker( pv );

	timeEndPeriod( 1 );

	return (unsigned) ret;
}

static int Hack_Worker( void * pv )
{
	if( ! pv ) return 0xFFFF;

	TARGETINFO& ti = *(TARGETINFO*) pv;
	const int iMyId = ti.slotid;

	if( iMyId < 0 || MAX_SLOTS <= iMyId || iMyId == 3 )
	{
		ti.dwProcessId = 0;
		return 0xFFFE;
	}

	HANDLE hProcess = NULL;
	DWORD dwOldPriority = 0;
	BOOL bPrioritySet = FALSE;
	DWORD dwTargetProcessId = ti.dwProcessId;
	HANDLE hToken = NULL;
	BOOL bPrivilege = FALSE;
	DWORD dwPrevAttributes = 0;
	TCHAR szEnemyPath[ CCHPATH ] = _T("");
	_tcscpy_s( szEnemyPath, _countof(szEnemyPath), ti.disp_path ? ti.disp_path : ti.lpPath );
	//TCHAR szEnemyExe[ CCHPATH ] = _T("");
	//_tcscpy_s( szEnemyExe, _countof(szEnemyExe), ti.lpExe );
	RECT minirect = {0};
	RECT hackrect = {0};
	SYSTEMTIME st;
	TCHAR percent[ 16 ];
	bool fSuspended = false;

	ptrdiff_t i;

	ptrdiff_t iPrevThreadCount = 0;
	unsigned int errorflags = 0;
#define ERR_SUSPEND 1
#define ERR_RESUME  2
	int iOpenThreadRetry = 0;
	int iSuspendThreadRetry = 0;
	int iResumeThreadRetry = 0;

	TCHAR msg[ 1024 ] = _T("");

	if( ti._hSync )
	{
		if( ti.fSync )
		{
			SetEvent( ti._hSync );
			ti.fSync = false;
		}
		else
		{
			WaitForSingleObject( ti._hSync, 10000 );
		}
		
		ti._hSync = NULL; // don't close
	}
#ifdef _DEBUG
	LARGE_INTEGER li = {0};
	LARGE_INTEGER freq = {0};
	QueryPerformanceCounter(&li);
	QueryPerformanceFrequency(&freq);
	_sntprintf_s(msg,_countof(msg),_TRUNCATE,_T("0x%x\t<<Hack_Worker[%d]>> PID=%lu (%s)\t%f\n"),
			GetCurrentThreadId(),
			iMyId,
			dwTargetProcessId,
			szEnemyPath,
			(double) li.QuadPart / freq.QuadPart * 1000.0);
	OutputDebugString(msg);
#endif

	ptrdiff_t numOfPairs = 0;
	PROCESS_THREAD_PAIR * sorted_pairs = GetCachedPairs( 1000, numOfPairs );
	BOOL bBES = IsProcessBES( dwTargetProcessId, sorted_pairs, numOfPairs );
	if( sorted_pairs )
	{
		MemFree( sorted_pairs );
		sorted_pairs = NULL;
	}

	if( bBES )
	{
		if( g_bHack[ 3 ] == TRUE )
		{
			g_bHack[ 3 ] = UNWATCH_NOW;
		}
		ti.dwProcessId = TARGET_PID_NOT_SET;
		PostMessage( g_hWnd, WM_COMMAND, IDM_STOP, 0 );

		_tcscpy_s( ppszStatus[ 16 ], cchStatus, _T( "* Denied *" ) );
		_tcscpy_s( ppszStatus[ 17 ], cchStatus, _T("BES Can't Target BES"));
		return THREAD_NOT_OPENED;
	}

	WriteIni_Time( szEnemyPath );

	// TODO ###
	SetRect( &minirect, 20L, 20L + 90L * iMyId, 479L, 40L + 90L * iMyId );
	SetRect( &hackrect, 20L, 20L + 90L * iMyId, 479L, 100L + 90L * iMyId );



	if( iMyId < 3 )
	{
		for( i = 0 + iMyId * 4; i < 4 + iMyId * 4; ++i )
			ppszStatus[ i ][ 0 ] = _T('\0');
		
		SetLongStatusText( szEnemyPath, ppszStatus[ 2 + iMyId * 4 ] );
	}

	if( g_bHack[ iMyId ] )
	{
		bPrivilege = EnableDebugPrivilege( &hToken, &dwPrevAttributes );
		

		hProcess = OpenProcess(	STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFFF,
								FALSE, dwTargetProcessId );

		if( hProcess )
		{
			WriteDebugLog( _T("OpenProcess 0xFFFF OK") );
		}
		else
		{
			WriteDebugLog( _T("OpenProcess 0xFFFF failed") );
			hProcess = OpenProcess(	STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFF,
								FALSE, dwTargetProcessId );

			if( hProcess )
			{
				WriteDebugLog( _T("OpenProcess 0xFFF OK") );
			}
			else
			{
				WriteDebugLog( _T("OpenProcess 0xFFF failed") );
				hProcess = OpenProcess(	PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION,
								FALSE, dwTargetProcessId );
				if( hProcess )
				{
					WriteDebugLog( _T("OpenProcess Q S OK") );
				}
				else
				{
					WriteDebugLog( _T("OpenProcess Q S failed") );
					hProcess = OpenProcess(	PROCESS_QUERY_INFORMATION, FALSE, dwTargetProcessId );

					if( hProcess )
					{
						WriteDebugLog( _T("OpenProcess Q OK") );
					}
					else
					{
						WriteDebugLog( _T("OpenProcess Q failed") );
					}
				}
			}
		}
		
		if( hProcess )
		{
			WriteDebugLog( ti.lpPath );
			tokendebug( hProcess );

			if( ti.fRealTime )
			{
				dwOldPriority = GetPriorityClass( hProcess );

				_stprintf_s( msg, _countof(msg),
							_T("Target #%d: \"%s\"\r\n")
							_T("\tProcess ID = %lu : hProcess = 0x%p (Priority: %lu)"),
							iMyId < 3 ? iMyId + 1 : iMyId,
							szEnemyPath,
							dwTargetProcessId, hProcess, dwOldPriority );
				WriteDebugLog( msg );

				if( dwOldPriority )
				{
					bPrioritySet = SetPriorityClass( hProcess, IDLE_PRIORITY_CLASS );
					WriteDebugLog( bPrioritySet	? TEXT( "SetPriorityClass OK" )
												: TEXT( "SetPriorityClass failed" ) );
				}
			}
		}
		else // !hProcess
		{
			WriteDebugLog( TEXT( "OpenProcess failed." ) );

			if( iMyId < 3 )
			{
				GetLocalTime( &st );
				_stprintf_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus,
							_T("Process ID = %lu"), dwTargetProcessId );
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T("%02d:%02d:%02d Initial OpenThread failed"),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			}

			ti.dwProcessId = TARGET_PID_NOT_SET;
//			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL ) ) {
				_stprintf_s( msg, _countof(msg), _T("* ReleaseSemaphore #%d in Hack()"),
							iMyId < 3 ? iMyId + 1 : iMyId );
			} else {
				_stprintf_s( msg, _countof(msg),
							_T("[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu"),
							iMyId < 3 ? iMyId + 1 : iMyId,
							GetLastError() );
			}
			WriteDebugLog( msg );

			int iRetVal = ! ProcessExists( dwTargetProcessId ) //(PathToProcessID( szEnemyPath ) == (DWORD) -1)
								? TARGET_MISSING : THREAD_NOT_OPENED ;

			if( iRetVal == THREAD_NOT_OPENED && iMyId == 2 && g_bHack[ 3 ] == TRUE )
			{
				Sleep( 3000 );
				if( ! ProcessExists( dwTargetProcessId )/*PathToProcessID( szEnemyPath ) == (DWORD) -1*/ )
				{
					iRetVal = TARGET_MISSING;
				}
				else
				{
					g_bHack[ 3 ] = UNWATCH_NOW;
					g_bHack[ 2 ] = FALSE;
				}
			}
			PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, iRetVal );
			InvalidateRect( g_hWnd, NULL, TRUE );

			if( hToken )
			{
				if( bPrivilege ) AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );
				CloseHandle( hToken );
			}

			return iRetVal;
		}

		if( iMyId < 3 )
		{
			if( ti.wDelay ) {
				_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
						_T( "Target #%d [ Delay %d ms ]" ),
						iMyId + 1,
						(int) ti.wDelay );
			} else {
				_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
						_T( "Target #%d [ %s ]" ),
						iMyId < 3 ? iMyId + 1 : iMyId,
						GetPercentageString( g_Slider[ iMyId ], percent, _countof(percent) ) );
			}
		}
	}

	DWORD dwDelay = ti.wDelay; // Initial delay
	while( dwDelay && g_bHack[ iMyId ] )
	{
		DWORD dwSleep = ( dwDelay > 1000UL ) ? 1000UL : dwDelay;
		Sleep( dwSleep );
		dwDelay -= dwSleep;
	}
	
	for( DWORD dwOuterLoops = 0; g_bHack[ iMyId ]; ++dwOuterLoops )
	{
		//***
		ptrdiff_t numOfThreads = 0;
//		DWORD * pThreadIDs = ListProcessThreads_Alloc( dwTargetProcessId, numOfThreads );
		DWORD * pThreadIDs = AllocThreadList( dwTargetProcessId, numOfThreads );

		if( ! pThreadIDs ) break;

		if( numOfThreads == 0 )
		{
			if( iMyId < 3 )
			{
				GetLocalTime( &st );
				_stprintf_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus, _T("Process ID = %lu"),
							dwTargetProcessId );
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus, _T("%02d:%02d:%02d No threads"),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
			}
			
			ti.dwProcessId = TARGET_PID_NOT_SET;
//			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			if( bPrioritySet ) SetPriorityClass( hProcess, dwOldPriority );
			CloseHandle( hProcess );

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL ) )
			{
				_stprintf_s( msg, _countof(msg), 
							_T( "* TARGET_MISSING #%d : ReleaseSemaphore in Hack()" ),
							iMyId < 3 ? iMyId + 1 : iMyId );
			}
			else
			{
				_stprintf_s( msg, _countof(msg),
							_T( "[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu" ),
							iMyId < 3 ? iMyId + 1 : iMyId,
							GetLastError() );
			}
			WriteDebugLog( msg );

			PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, TARGET_MISSING );

			HeapFree( GetProcessHeap(), 0, pThreadIDs );
			if( hToken )
			{
				if( bPrivilege ) AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );
				CloseHandle( hToken );
			}

			return TARGET_MISSING;
		}

		if( dwOuterLoops == 0 )
		{
			iPrevThreadCount = numOfThreads;
			_stprintf_s( msg, _countof(msg), _T( "Target #%d : Thread Count = %3d" ),
							iMyId < 3 ? iMyId + 1 : iMyId,
							(int) numOfThreads );
			WriteDebugLog( msg );
		}
#ifdef _DEBUG
		else
#else
		else if( dwOuterLoops % 50 == 0 )
#endif
		{
			_stprintf_s( msg, _countof(msg),
						_T( "Target #%d : (INFO) Loops %5lu, Thread Count = %3d, PID %lu" ), 
						iMyId < 3 ? iMyId + 1 : iMyId,
						dwOuterLoops,
						(int) numOfThreads,
						dwTargetProcessId );
			WriteDebugLog( msg );
		}

		if( iMyId < 3 )
		{
			_stprintf_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus,
						_T( "Process ID = %lu [ %d Thread%s ]" ),
						dwTargetProcessId, (int) numOfThreads,
						( numOfThreads != 1 ) ? _T("s") : _T("") );
		}
		GetLocalTime( &st );

		if( numOfThreads != iPrevThreadCount || errorflags )
		{
			_stprintf_s( msg, _countof(msg),
						_T( "Target #%d : Thread Count = %3d (Prev = %d) flags=%u" ),
						iMyId < 3 ? iMyId + 1 : iMyId,
						(int) numOfThreads,
						(int) iPrevThreadCount,
						errorflags );
			WriteDebugLog( msg );
			
			iPrevThreadCount = numOfThreads;
			
			errorflags = 0;
			if( iMyId < 3 )
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T( "%02d:%02d:%02d Target Re-Locked On: OK" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond );
		}
		else
		{
			if( iMyId < 3 )
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T( "%02d:%02d:%02d %s: OK" ),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond,
							dwOuterLoops ? _T("Calm") : _T("Started") );
		}

		// NULL if unused (HEAP_ZERO_MEMORY)
		HANDLE * pThreadHandles = (HANDLE *) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
															sizeof(HANDLE) * numOfThreads );
		if( ! pThreadHandles )
		{
			HeapFree( GetProcessHeap(), 0, pThreadIDs );
			break;
		}

//#define A_M 1
#undef A_M
		ptrdiff_t numOfOpenedThreads = 0;
		for( i = 0; i < numOfThreads; ++i )
		{
			pThreadHandles[ i ] = OpenThread( STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0xFFFF,
											FALSE, pThreadIDs[ i ] );
			
			if( pThreadHandles[ i ] )
			{
				//if(i==0) WriteDebugLog(_T("0xFFFF OK"));
			}
			else
			{
				//if(i==0) WriteDebugLog(_T("0xFFFF failed"));
				
				pThreadHandles[ i ] = OpenThread( STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | 0x3FF,
											FALSE, pThreadIDs[ i ] );

				if( pThreadHandles[ i ] )
				{
					//if(i==0) WriteDebugLog(_T("0x3FF OK"));
				}
				else
				{
					//if(i==0) WriteDebugLog(_T("0x3FF failed"));
					pThreadHandles[ i ] = OpenThread( THREAD_SUSPEND_RESUME, FALSE, pThreadIDs[ i ] );
				}
			}

			if( pThreadHandles[ i ] )
			{
				++numOfOpenedThreads;
			}
			else
			{
				_stprintf_s( msg, _countof(msg),
							_T( "[!] Target #%d : OpenThread failed: Thread #%03d, ThreadId 0x%08lX" ),
							iMyId < 3 ? iMyId + 1 : iMyId,
							(int) i + 1,
							pThreadIDs[ i ] );
				WriteDebugLog( msg );
			}
		}

		if( numOfOpenedThreads == 0
			||
			numOfOpenedThreads != numOfThreads && iOpenThreadRetry > 10
			||
			iSuspendThreadRetry > 100
			||
			iResumeThreadRetry > 50 )
		{
			for( i = 0; i < numOfThreads; ++i )
				if( pThreadHandles[ i ] ) CloseHandle( pThreadHandles[ i ] );

			GetLocalTime( &st );
			if( iMyId < 3 )
			{
				_stprintf_s( ppszStatus[ 3 + iMyId * 4 ], cchStatus,
							_T("%02d:%02d:%02d %s Error"),
							(int) st.wHour, (int) st.wMinute, (int) st.wSecond,
							(iResumeThreadRetry > 50) ? _T("ResumeThread")
							: (iSuspendThreadRetry > 100) ? _T("SuspendThread")
							: _T("OpenThread") );

				WriteDebugLog( ppszStatus[ 3 + iMyId * 4 ] );
			}
			WriteDebugLog( TEXT( "### Giving up... ###" ) );

			ti.dwProcessId = TARGET_PID_NOT_SET;
//			g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;

			if( bPrioritySet ) SetPriorityClass( hProcess, dwOldPriority );
			CloseHandle( hProcess );

			UpdateStatus( g_hWnd ); // needed ?

			if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL ) )
			{
				_stprintf_s( msg, _countof(msg), 
							_T( "* THREAD_NOT_OPENED #%d : ReleaseSemaphore in Hack()" ),
							iMyId < 3 ? iMyId + 1 : iMyId );
				WriteDebugLog( msg );
			}

			int iRetVal = ! ProcessExists( dwTargetProcessId ) /*(PathToProcessID( szEnemyPath ) == (DWORD) -1)*/
								? TARGET_MISSING : THREAD_NOT_OPENED ;

			if( iRetVal == THREAD_NOT_OPENED && iMyId == 2 && g_bHack[ 3 ] == TRUE )
			{
				Sleep( 3000 );
				if( ! ProcessExists( dwTargetProcessId ) /*PathToProcessID( szEnemyPath ) == (DWORD) -1*/ )
				{
					iRetVal = TARGET_MISSING;
				}
				else
				{
					g_bHack[ 3 ] = UNWATCH_NOW;
					g_bHack[ 2 ] = FALSE;
				}
			}
			
			PostMessage( g_hWnd, WM_USER_STOP, (WPARAM) iMyId, iRetVal );

			InvalidateRect( g_hWnd, NULL, TRUE );

			HeapFree( GetProcessHeap(), 0, pThreadHandles );
			HeapFree( GetProcessHeap(), 0, pThreadIDs );

			if( hToken )
			{
				if( bPrivilege ) AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );
				CloseHandle( hToken );
			}

			return iRetVal;
		}
		else if( numOfOpenedThreads != numOfThreads )
		{
			for( i = 0; i < numOfThreads; ++i )
				if( pThreadHandles[ i ] ) CloseHandle( pThreadHandles[ i ] );

			++iOpenThreadRetry;
			
			_stprintf_s( msg, _countof(msg), 
						_T( "@ Couldn't open some threads: Retrying #%d..." ), iOpenThreadRetry );
			WriteDebugLog( msg );
		
			HeapFree( GetProcessHeap(), 0, pThreadHandles );
			HeapFree( GetProcessHeap(), 0, pThreadIDs );
			
			Sleep( 100 );
			CachedList_Refresh( 100 );
			continue;
		}

		iOpenThreadRetry = 0;

#if 0
//		const bool fActive1 = ( g_bHack[ 0 ] == TRUE );
//		const bool fActive2 = ( g_bHack[ 1 ] == TRUE );
//		const bool fActive3 = ( g_bHack[ 2 ] == TRUE && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE );

		if( IsActive() )
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
				
			/*if( fActive1 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #1" ) );
				
			if( fActive2 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #2" ) );
				
			if( fActive3 )
				_tcscat_s( strStatus, _countof(strStatus), _T( " #3" ) );*/

			for( ptrdiff_t e = 0; e < MAX_SLOTS; ++e )
			{
				if( e == 3 ) continue;
				if( g_bHack[ e ] && ( e != 2 || g_dwTargetProcessId[ 2 ] != WATCHING_IDLE ) )
				{
					TCHAR tmp[ 10 ] = _T("");
					_stprintf_s( tmp, _countof(tmp), _T(" #%d"), (int)( e < 3 ? e + 1 : e ) );
					_tcscat_s( strStatus, _countof(strStatus), tmp );
				}
			}
		
			_tcsncpy_s( ppszStatus[ 12 ], cchStatus, strStatus, _TRUNCATE );
		}
		else
		{
			ppszStatus[ 12 ][ 0 ] = _T('\0');
		}
#endif

		InvalidateRect( g_hWnd, &hackrect, FALSE );

		DWORD msElapsed = 0;
		DWORD sElapsed = 0;
		unsigned numOfReports = 0;

#ifdef _DEBUG
		DWORD dwInnerLoops = 0;
#endif

		while( msElapsed < 10 * 1024 ) // about 10 seconds
		//while( msElapsed < 4 * 1024 ) // about 4 seconds
		{
#ifdef _DEBUG
			++dwInnerLoops;
#endif
			DWORD TimeRed = 0;
			DWORD TimeGreen = 100;
			UINT msUnit = UNIT_DEF;

			const int slider = g_Slider[ iMyId ];
			if( 1 <= slider && slider <= 99 )
			{
				if( UNIT_MIN <= ti.wCycle && ti.wCycle <= UNIT_MAX )
				{
					msUnit = ti.wCycle;

					//TCHAR dbgstr[ 128 ] = _T("");
					//_stprintf_s( dbgstr, _countof(dbgstr), _T( "[DEBUG] msUnit = ti.wCycle=%d" ), ti.wCycle );
					//WriteDebugLog( dbgstr );
				}
				else
				{
					msUnit = g_uUnit;
					ti.wCycle = (WORD) 0;
				}
				if( !( UNIT_MIN <= msUnit && msUnit <= UNIT_MAX ) ) msUnit = UNIT_DEF;
				TimeRed = (DWORD)( msUnit / 100.0 * slider + 0.5 );
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

#ifdef A_M
			DWORD_PTR dwAM = 0, dwSys = 0;
			GetProcessAffinityMask( hProcess, &dwAM, &dwSys );
#endif
			if( ! fSuspended )
			{
#ifdef A_M
				fSuspended = true;
				if( ! SetProcessAffinityMask( hProcess, (DWORD_PTR) 0 ) )
				{
					_stprintf_s( msg, _countof(msg),
						_T( "Target #%d : Suspend failed: err=%lu" ),
								iMyId < 3 ? iMyId + 1 : iMyId,
								GetLastError() );
					WriteDebugLog( msg );
				}
#else
				for( i = 0; i < numOfThreads; ++i )
				{
					if( pThreadHandles[ i ] )
					{
						if( SuspendThread( pThreadHandles[ i ] ) == (DWORD) -1 )
						{
							errorflags |= ERR_SUSPEND;
							_stprintf_s( msg, _countof(msg),
								_T( "Target #%d : SuspendThread failed (Thread #%03d) : Retry=%d : err=%lu" ),
										iMyId < 3 ? iMyId + 1 : iMyId,
										(int) i + 1,
										iSuspendThreadRetry,
										GetLastError() );
							WriteDebugLog( msg );
						}
						else fSuspended = true;
					}
				}
#endif
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

			// Use a non-volatile flag: (! g_bHack[ iMyId ]) may change between #1 and #2,
			// and so if we use it, it is possible that the loop breaks at #2 while fSuspended,
			// without being resumed at #1, if TimeGreen is 0. [FIX @20140317]
			/*const bool fUnlimitNow = ! g_bHack[ iMyId ];*/

			// if !g_bHack[ iMyId ], the outer loop will break after this step;
			// in that case, threads must be resumed even if limited 100% ie TimeGreen==0.
			if( fSuspended /*&& ( TimeGreen || fUnlimitNow )*/ ) //#1
			{
#ifdef A_M
				if( ! SetProcessAffinityMask( hProcess, dwAM ) )
				{
						errorflags |= ERR_RESUME;
						_stprintf_s( msg, _countof(msg),
									_T("Target #%d : Resume failed: ")
									_T("err=%lu"),
									iMyId < 3 ? iMyId + 1 : iMyId,
									GetLastError() );
						WriteDebugLog( msg );
				}
#else
				for( i = 0; i < numOfThreads; ++i )
				{
					if( pThreadHandles[ i ] && ResumeThread( pThreadHandles[ i ] ) == (DWORD) -1 )
					{
						errorflags |= ERR_RESUME;
						_stprintf_s( msg, _countof(msg),
									_T("Target #%d : ResumeThread failed: Thread #%03d, ")
									_T("ThreadId 0x%08lX, iResumeThreadRetry=%d : err=%lu"),
									iMyId < 3 ? iMyId + 1 : iMyId,
									(int) i + 1,
									pThreadIDs[ i ],
									iResumeThreadRetry,
									GetLastError() );
						WriteDebugLog( msg );
					}
				}
#endif
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

			if( ! g_bHack[ iMyId ] ) //#2
			{
				break;
			}
			else if( errorflags )
			{
				Sleep( 100 );
				CachedList_Refresh( 100 );
				break;
			}
			
#if ALLOW100
			if( TimeGreen )	Sleep( TimeGreen );
#else
			Sleep( TimeGreen );
#endif
			msElapsed += TimeGreen;
		
#ifdef _UNICODE
# define BLINKING_CHAR L'\x25CF'
#else
# define BLINKING_CHAR '*'
#endif
			if( msElapsed >> 10 != sElapsed ) // report once in a second (1024 ms)
			{
				sElapsed = msElapsed >> 10;
				if( iMyId < 3 )
				{
					if( ti.wCycle ) {
						_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
								_T( "Target #%d [ %s @ %u ms ] %c" ),
								iMyId < 3 ? iMyId + 1 : iMyId,
								GetPercentageString( g_Slider[ iMyId ], percent, _countof(percent) ), // 20140317
								msUnit,
								( numOfReports & 1 ) ? _T(' ') : BLINKING_CHAR );
					} else {
						_stprintf_s( ppszStatus[ 0 + iMyId * 4 ], cchStatus,
								_T( "Target #%d [ %s ] %c" ),
								iMyId < 3 ? iMyId + 1 : iMyId,
								GetPercentageString( g_Slider[ iMyId ], percent, _countof(percent) ), // 20140317
								( numOfReports & 1 ) ? _T(' ') : BLINKING_CHAR );
					}
					InvalidateRect( g_hWnd, &minirect, 0 );
				}
				++numOfReports;

#if 0 //def _DEBUG
				_stprintf_s(msg,_countof(msg),_T("slider=%d: Red=%lu Green=%lu"),
					slider, TimeRed, TimeGreen );
				WriteDebugLog(msg);
#endif
			}
		}

		for( i = 0; i < numOfThreads; ++i )
			if( pThreadHandles[ i ] ) CloseHandle( pThreadHandles[ i ] );

		HeapFree( GetProcessHeap(), 0, pThreadHandles );
		HeapFree( GetProcessHeap(), 0, pThreadIDs );
	}

	if( iMyId < 3 )
		_tcscpy_s( ppszStatus[ 1 + iMyId * 4 ], cchStatus, _T( "* Unlimited *" ) );

	if( ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LONG *) NULL ) )
	{
		_stprintf_s( msg, _countof(msg),
					_T( "* Target #%d : ReleaseSemaphore in Hack()" ), iMyId < 3 ? iMyId + 1 : iMyId );
	}
	else
	{
		_stprintf_s( msg, _countof(msg),
					_T( "[!] Target #%d : ReleaseSemaphore failed in Hack(): ErrCode %lu" ),
					iMyId < 3 ? iMyId + 1 : iMyId,
					GetLastError() );
	}
	WriteDebugLog( msg );

	if( bPrioritySet )
	{
		BOOL bPriorityRestored = SetPriorityClass( hProcess, dwOldPriority );
		WriteDebugLog( bPriorityRestored ? _T("Set dwOldPriority: OK")
										: _T("Set dwOldPriority: Failed") );
	}
	if( hProcess )
		CloseHandle( hProcess );

	if( iMyId < 3 )
		ppszStatus[ 3 + iMyId * 4 ][ 0 ] = _T('\0');

	if( hToken )
	{
		if( bPrivilege ) AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );
		CloseHandle( hToken );
	}
	
	return NORMAL_TERMINATION;
}



TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString )
{
	*lpString = _T('\0');

	if( SLIDER_MIN <= lTrackbarPos && lTrackbarPos <= 99 )
	{
#ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %2d%%", (int) lTrackbarPos );
#else
		sprintf_s( lpString, cchString, "- %2d%%", (int) lTrackbarPos );
#endif
	}
#if ALLOW100
	else if( lTrackbarPos == SLIDER_MAX )
	{
# ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %2d%%", 100 );
# else
		sprintf_s( lpString, cchString, "- %2d%%", 100 );
# endif
	}
#endif
	else if( 100 <= lTrackbarPos && lTrackbarPos <= SLIDER_MAX )
	{
		double d = 99.0 + ( lTrackbarPos - 99 ) * 0.1;

#ifdef _UNICODE
		swprintf_s( lpString, cchString, L"\x2212 %4.1f%%", d );
#else
		sprintf_s( lpString, cchString, "- %4.1f%%", d );
#endif
	}

	return lpString;
}



static BOOL SetSliderText( HWND hDlg, int iControlId, LONG lPercentage )
{
	TCHAR strPercent[ 32 ];
	
	return SetDlgItemText( hDlg, iControlId, 
							GetPercentageString( lPercentage, strPercent, _countof(strPercent) ) );
}
static void EnableDlgItem( HWND hDlg, int iControlId, BOOL bEnable )
{
	HWND hwndCtrl = GetDlgItem( hDlg, iControlId );
	
	// If the control to be disabled has focus, re-focus IDOK now
	if( ! bEnable && GetFocus() == hwndCtrl )
		SendMessage( hDlg, WM_NEXTDLGCTL, (WPARAM) GetDlgItem( hDlg, IDOK ), TRUE );
	
	EnableWindow( hwndCtrl, bEnable );
}

//#define SliderMoved(wParam)     (LOWORD(wParam)!=TB_ENDTRACK)

static void _settings_init( HWND hDlg, TARGETINFO * ti )
{
	const bool fAllowMoreThan99 = 
				!!( MF_CHECKED & GetMenuState( GetMenu( g_hWnd ), 
					IDM_ALLOWMORETHAN99, MF_BYCOMMAND ) );
	const int iMax = fAllowMoreThan99 ? SLIDER_MAX : 99 ;

	TCHAR tmpstr[ 32 ];
	for( int i = 0; i < 3; i++ )
	{
		const TCHAR * path = ti[ i ].disp_path ? ti[ i ].disp_path
									: ti[ i ].lpPath ? ti[ i ].lpPath
									: TARGET_UNDEF ;
#if 0 //20160922
		if( i == 2 && g_bHack[ 3 ] )
		{
			TCHAR szText[ CCHPATH + 20 ];
			_sntprintf_s( szText, _countof(szText), _TRUNCATE, _T("%s (Watching)"), path );
			SetDlgItemText( hDlg, IDC_EDIT_TARGET1 + 2, szText );

		SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + 2, TEXT( "(Watching)" ) );
		EnableDlgItem( hDlg, IDC_BUTTON_STOP1 + 2, FALSE );
		EnableDlgItem( hDlg, IDC_SLIDER1 + 2, TRUE );
		EnableDlgItem( hDlg, IDC_TEXT_PERCENT1 + 2, TRUE );
		}
		else
#endif
		{
			SetDlgItemText( hDlg, IDC_EDIT_TARGET1 + i,	path );
		_stprintf_s( tmpstr, _countof(tmpstr), _T( "%s #&%d" ),
					g_bHack[ i ] ? _T( "Unlimit" ) : _T( "Limit" ), i + 1 );
		SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + i, tmpstr );
		EnableDlgItem( hDlg, IDC_BUTTON_STOP1 + i, 
						( ti[ i ].dwProcessId != TARGET_PID_NOT_SET ) );
		EnableDlgItem( hDlg, IDC_SLIDER1 + i, 
						( ti[ i ].dwProcessId != TARGET_PID_NOT_SET ) );
		EnableDlgItem( hDlg, IDC_TEXT_PERCENT1 + i, 
						( ti[ i ].dwProcessId != TARGET_PID_NOT_SET ) );
		}

		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETRANGE, TRUE, MAKELONG( SLIDER_MIN, iMax ) );
		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETPAGESIZE, 0, 5 );
		
		const int iSlider = g_Slider[ i ];
		SendDlgItemMessage( hDlg, IDC_SLIDER1 + i, TBM_SETPOS, TRUE, iSlider );
		SetSliderText( hDlg, IDC_TEXT_PERCENT1 + i , iSlider );
	}
#if 0
	if( /*g_dwTargetProcessId[ 2 ] == (DWORD) -1 ||*/ g_bHack[ 3 ] )
	{
		SetDlgItemText( hDlg, IDC_BUTTON_STOP1 + 2, TEXT( "(Watching)" ) );
		EnableWindow( GetDlgItem( hDlg, IDC_BUTTON_STOP1 + 2 ), FALSE );
		EnableWindow( GetDlgItem( hDlg, IDC_SLIDER1 + 2 ), TRUE );
		EnableWindow( GetDlgItem( hDlg, IDC_TEXT_PERCENT1 + 2 ), TRUE );
	}
#endif
	if( !( UNIT_MIN <= g_uUnit && g_uUnit <= UNIT_MAX ) ) g_uUnit = UNIT_DEF;
	SetDlgItemInt( hDlg, IDC_EDIT_UNIT, g_uUnit, FALSE );

	PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
}

INT_PTR CALLBACK Settings( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	static TARGETINFO * ti = NULL;
	static HFONT hfontNote = NULL;
	static HFONT hfontPercent = NULL;
	static HFONT hfontUnit = NULL;

	switch (message)
	{
		case WM_INITDIALOG:
		{
			TCHAR szWindowText[ MAX_WINDOWTEXT ] = _T("");
			if( ! lParam )
			{
				EndDialog( hDlg, FALSE );
				break;
			}
			g_hSettingsDlg = hDlg;

			ti = (TARGETINFO*) lParam;
			
			HDC hDC = GetDC( hDlg );
			hfontPercent = MyCreateFont( hDC, TEXT( "Verdana" ), 12, TRUE, FALSE );
			hfontUnit = MyCreateFont( hDC, TEXT( "Verdana" ), 11, FALSE, FALSE );
			ReleaseDC( hDlg, hDC );

#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					IS_JAPANESEo ? S_JPNo_1002 : S_JPN_1002,
					-1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_FRE_1002,
					-1, szWindowText, MAX_WINDOWTEXT );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE,
					S_SPA_1002,
					-1, szWindowText, MAX_WINDOWTEXT );
			}
			else
#endif
			{
				_tcscpy_s( szWindowText, MAX_WINDOWTEXT, _T( "Limiter control" ) );
			}
			SetWindowText( hDlg, szWindowText );

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

			_settings_init( hDlg, ti );
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
			const int id = GetDlgCtrlID( (HWND) lParam );
			const int k = id - IDC_SLIDER1;
			if( k < 0 || 2 < k || TB_ENDTRACK < LOWORD( wParam ) || ! ppszStatus ) break;

			if( LOWORD( wParam ) == TB_ENDTRACK )
			{
				SendNotifyIconData( g_hWnd, ti, NIM_MODIFY );
			}
			else
			{
				LRESULT lSlider = SendMessage( (HWND) lParam, TBM_GETPOS, 0, 0 );
				if( lSlider < SLIDER_MIN || lSlider > SLIDER_MAX )
					lSlider = SLIDER_DEF;

				g_Slider[ k ] = (int) lSlider;

				TCHAR strPercent[ 16 ] = _T("");
				GetPercentageString( lSlider, strPercent, _countof(strPercent) );
				SetDlgItemText( hDlg, IDC_TEXT_PERCENT1 + k, strPercent );
				
				if( g_bHack[ k ]
					/*&& ( k < 2 || k == 2 && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE )*/ )
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
					const bool fLimit = ! g_bHack[ k ]; // new state after toggled
					TCHAR strBtnText[ 32 ] = _T("");
					_stprintf_s( strBtnText, _countof(strBtnText),
								_T("%s #&%d"),
								fLimit ? _T("Unlimit") : _T("Limit"),
								k + 1 );
					
					if( fLimit ) // WM_USER_RESTART gets sema on its own
						SendMessage( g_hWnd, WM_USER_RESTART, (WPARAM) k, 0 );
					else
						SendMessage( g_hWnd, WM_USER_STOP, (WPARAM) k, STOPF_UNLIMIT );

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
/**
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
**/
		case WM_USER_REFRESH:
		{
			if( lParam == WM_KILLFOCUS
				&&
				( wParam < 3 && ti[ wParam ].dwProcessId || wParam == 2 && g_bHack[ 3 ] )
			)
			{
				LRESULT slider
					= SendDlgItemMessage( hDlg, IDC_SLIDER1 + (int) wParam, TBM_GETPOS, 0, 0 );
				const TCHAR * q = ti[ wParam ].disp_path;
				if( ! q ) q = ti[ wParam ].lpPath;
				SetSliderIni( q, slider );
			}
			else
			{
				_settings_init( hDlg, ti );
			}
			break;
		}
		
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
#if 0
				if( g_dwTargetProcessId[ i ] != TARGET_PID_NOT_SET )
				{
#ifdef _DEBUG
					TCHAR buf[ CCHPATHEX + 100 ];
					_stprintf_s( buf, _countof(buf), _T( "g_Slider[ %d ] = %d : %s" ),
									i, g_Slider[ i ], lphp[ i ].lpTarget->szPath/*g_szTarget[ i ]*/ );
					WriteDebugLog( buf );
#endif
					/*_tcscpy_s( buf, _countof(buf), g_szTarget[ i ] );
					
					if( i == 2 && g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
					{
						size_t len = _tcslen( buf );
						if( 11 < len
							&& _tcscmp( &buf[ len - 11 ], _T(" (Watching)") ) == 0 )
						{
							buf[ len - 11 ] = _T('\0');
						}
					}*/
					
					SetSliderIni( lphp[ i ].lpTarget->szExe/*buf*/, g_Slider[ i ] );
				}
#endif		
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

			/*if( lpszWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpszWindowText );
				lpszWindowText = NULL;
			}*/
			
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
	DWORD * pThreadIDs = ListProcessThreads_Alloc( dwProcessID, numOfThreads );
	
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
	DWORD dwPrevAttributes = 0;
	BOOL bPrivilege = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

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
		
			if( numOfThreads < 64 )
			{
				_stprintf_s( line, _countof(line),
						_T( "Thread #%3d (0x%08lX) opened\tPrevious suspend count = %lu\r\n" ),
						(int) i + 1, 
						pThreadIDs[ i ],
						dwPreviousSuspendCount );

				_tcscat_s( lpMessage, cchMessage, line );
			}
			else if( numOfThreads < 256 )
			{
				_stprintf_s( line, _countof(line),
						_T( "Thread #%3d OK, Prev count %lu; " ),
						(int) i + 1,
						dwPreviousSuspendCount );

				_tcscat_s( lpMessage, cchMessage, line );
			}

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
		if( bPrivilege ) AdjustDebugPrivilege( hToken, FALSE, &dwPrevAttributes );
		CloseHandle( hToken );
	}

	MessageBox( hWnd, lpMessage, TEXT( "Unfreezing : Status" ), MB_OK );
	HeapFree( GetProcessHeap(), 0L, lpMessage );
	HeapFree( GetProcessHeap(), 0L, pThreadIDs );
	
	return bSuccess;
}


#define SURROGATE_HI( wc ) ( L'\xD800' <= wc && wc <= L'\xDBFF' ) // leading
#define SURROGATE_LO( wc ) ( L'\xDC00' <= wc && wc <= L'\xDFFF' ) // trailing
/*
0123456789012345678901234567890123456789012345678 (cchOriginal=48)
dddddddddd~D      SsssssssssssssssssssssssssssssZ (cchToMove=31) D=dst, S=src
C:\Program Files\mozilla.org\Mozilla\mozilla.exe
*/
#if 0
static void AdjustLength( TCHAR * szPath, size_t cchOriginal )
{
	//const size_t cchOriginal = _tcslen( szPath );
	//if( 45 < cchOriginal )
	{
		TCHAR * dst = strPath + ( SURROGATE_LO( szPath[ 10 ] ) ? 11 : 10 );
#ifdef _UNICODE
		*dst++ = L'\x2026'; // 'HORIZONTAL ELLIPSIS' (U+2026)
#else
		*dst++ = '.';
		*dst++ = '.';
		*dst++ = '.';
#endif

		// Including +1 for term NUL
		size_t cchToMove = (size_t)( SURROGATE_LO( szPath[ cchOriginal - 30 ] ) ? 32 : 31 );
		const TCHAR * src = szPath + cchOriginal - ( cchToMove - 1 ); // -1 to include NUL
		memmove( dst, src, cchToMove * sizeof(TCHAR) );
	}
}
#endif

void SetLongStatusText( const TCHAR * szText, TCHAR * pszStatus )
{
	size_t cchOriginal = _tcslen( szText );
	if( cchOriginal <= 45 ) _tcscpy_s( pszStatus, cchStatus, szText );
	else
	{
		const TCHAR * tail = &szText[ cchOriginal - 30 ];
#ifdef _UNICODE
		int icchHead = ( SURROGATE_LO( szText[ 10 ] ) ) ? 11 : 10 ;
		if( SURROGATE_LO( *tail ) ) --tail;
		swprintf_s( pszStatus, cchStatus, L"%.*s\x2026%s", icchHead, szText, tail );
#else
		sprintf_s( pszStatus, cchStatus, "%.10s...%s", szText, tail );
#endif
	}
}


typedef BOOL (WINAPI *GetTokenInformation_t)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);
extern GetTokenInformation_t g_pfnGetTokenInformation;
static DWORD GetPrivilegeAttributes( HANDLE hToken, LUID * pLuid )
{
	DWORD dwAttributes = 0L; // ret val

	DWORD cbPrivs = 0;
	// GetTokenInformation returns FALSE in this case with ERROR_INSUFFICIENT_BUFFER (122);
	// MS sample assumes the same:
	// e.g. http://msdn.microsoft.com/en-us/library/windows/desktop/aa446670%28v=vs.85%29.aspx
	// Since this behavior is not well documented, I'll just check cbPrivs!=0
#ifdef Like_Microsoft_LogonSID_sample_code
	if( g_pfnGetTokenInformation
		&& ! g_pfnGetTokenInformation( hToken, TokenPrivileges, NULL, 0L, &cbPrivs )
		&& GetLastError() == ERROR_INSUFFICIENT_BUFFER )
#else
	if( g_pfnGetTokenInformation
		&& (*g_pfnGetTokenInformation)( hToken, TokenPrivileges, NULL, 0L, &cbPrivs ), cbPrivs )
#endif
	{
		TOKEN_PRIVILEGES * lpPrivs
			= (TOKEN_PRIVILEGES*) HeapAlloc( GetProcessHeap(), 0, cbPrivs );
		if( lpPrivs )
		{
			DWORD cbWritten;
			lpPrivs->PrivilegeCount = 0L;
			(*g_pfnGetTokenInformation)( hToken, TokenPrivileges, lpPrivs, cbPrivs, &cbWritten );
			
			for( DWORD n = 0L; n < lpPrivs->PrivilegeCount; ++n )
			{
				if( lpPrivs->Privileges[ n ].Luid.LowPart == pLuid->LowPart
					&& lpPrivs->Privileges[ n ].Luid.HighPart == pLuid->HighPart )
				{
					dwAttributes = lpPrivs->Privileges[ n ].Attributes;
					break;
				}
			}

			HeapFree( GetProcessHeap(), 0, lpPrivs );
		}
	}
	
	return dwAttributes;
}

typedef BOOL (WINAPI *LookupPrivilegeValue_t)(LPCTSTR,LPCTSTR,PLUID);
typedef BOOL (WINAPI *AdjustTokenPrivileges_t)(HANDLE,BOOL,PTOKEN_PRIVILEGES,
																	 DWORD,PTOKEN_PRIVILEGES,PDWORD);
extern LookupPrivilegeValue_t g_pfnLookupPrivilegeValue;
extern AdjustTokenPrivileges_t g_pfnAdjustTokenPrivileges;
extern bool g_fLowerPriv;

DWORD AdjustDebugPrivilege( HANDLE hToken, BOOL bEnable, DWORD * pPrevAttributes ) // WAS: SetDebugPrivilege
{
	if( g_fLowerPriv )
		return ERROR_SERVICE_DISABLED;

	
	WriteDebugLog( bEnable? _T( "AdjustDebugPrivilege: Enabling..." ) : _T( "AdjustDebugPrivilege: Disabling..." ) );


//BOOL bRet = FALSE;
	DWORD dwErr = 0L;
	DWORD dwPrevAttributes = 0L;

	LUID Luid;
    if( g_pfnLookupPrivilegeValue
		&& g_pfnAdjustTokenPrivileges
		&& (*g_pfnLookupPrivilegeValue)( NULL, SE_DEBUG_NAME, &Luid ) )
	{
		dwPrevAttributes = GetPrivilegeAttributes( hToken, &Luid );
		DWORD dwNewAttributes = 0L;
		if( bEnable )
		{
			dwNewAttributes = (dwPrevAttributes | SE_PRIVILEGE_ENABLED);
		}
		else
		{
			if( pPrevAttributes )
				dwNewAttributes = *pPrevAttributes;
			else
				dwNewAttributes = (dwPrevAttributes & ~((DWORD)SE_PRIVILEGE_ENABLED));
		}

		TOKEN_PRIVILEGES tp;
		tp.PrivilegeCount = 1L;
		tp.Privileges[ 0 ].Luid = Luid;
		tp.Privileges[ 0 ].Attributes = dwNewAttributes;

		TOKEN_PRIVILEGES tpPrev = {0};
		tpPrev.PrivilegeCount = 1;
		DWORD BufferLength = sizeof(tpPrev);
		DWORD ReturnLength = 0;
		BOOL b0 = 0;
		UINT situation = 0; // test

		SetLastError( 0 );
		b0 = (*g_pfnAdjustTokenPrivileges)( hToken, FALSE, &tp, BufferLength, &tpPrev, &ReturnLength );
		dwErr = GetLastError();

		// In theory, this could happen
		if( BufferLength < ReturnLength )
		{
			situation |= 1;
			
			BufferLength = ReturnLength;
			TOKEN_PRIVILEGES * lpTP = (TOKEN_PRIVILEGES *)
									HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, BufferLength );
			if( lpTP )
			{
				lpTP->PrivilegeCount
					= (DWORD)( (BufferLength - sizeof(DWORD)) / sizeof(LUID_AND_ATTRIBUTES) );
				SetLastError( 0 );
				b0 = (*g_pfnAdjustTokenPrivileges)( hToken, FALSE, &tp, 0, NULL, NULL );
				dwErr = GetLastError();
			
				tpPrev.PrivilegeCount = lpTP->PrivilegeCount;
				tpPrev.Privileges[ 0 ] = lpTP->Privileges[ 0 ];
				MemFree( lpTP );
			}
			else
			{
				situation |= 2;

				BufferLength = 0;
				SetLastError( 0 );
				b0 = (*g_pfnAdjustTokenPrivileges)( hToken, FALSE, &tp, BufferLength, NULL, NULL );
				dwErr = GetLastError();
			}
		}

		// ret.val. of Adjust itself is not reliable; we need to do this...
		//bRet = ( dwErr == ERROR_SUCCESS );

///#ifdef _DEBUG
		TCHAR s[200];
		_stprintf_s(s,200,
			_T("0x%x [Enable=%d] Attr %lX to %lX: prev=%lX ReturnLength=%lu/%lu")
			_T(": PrivilegeCount=%lu err=%lu b0=%d situation=%u"),
			GetCurrentThreadId(),
			bEnable,
			dwPrevAttributes,
			dwNewAttributes,
				tpPrev.Privileges[0].Attributes,
				ReturnLength,
				BufferLength,
			tpPrev.PrivilegeCount,
			dwErr,
			b0,
			situation);
		WriteDebugLog(s);
///#endif
	}

	if( pPrevAttributes )
		*pPrevAttributes = dwPrevAttributes;

	if( dwErr == ERROR_SUCCESS )
		WriteDebugLog( _T( "Success!" ) );

	return dwErr;
}

typedef BOOL (WINAPI *OpenThreadToken_t)(HANDLE,DWORD,BOOL,PHANDLE);
typedef BOOL (WINAPI *ImpersonateSelf_t)(SECURITY_IMPERSONATION_LEVEL);
extern OpenThreadToken_t g_pfnOpenThreadToken;
extern ImpersonateSelf_t g_pfnImpersonateSelf;


BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes )
{
	if( ! phToken || ! g_pfnOpenThreadToken || ! g_pfnImpersonateSelf )
		return FALSE;

	BOOL bPrivilege = FALSE; // ret val
	TCHAR tmpstr[ 256 ];

	SetLastError( 0 );
	BOOL bToken = (*g_pfnOpenThreadToken)( GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, TRUE, phToken );
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
			_stprintf_s( tmpstr, _countof(tmpstr), _T("OpenThreadToken failed, Error Code %lu"), GetLastError() );
			WriteDebugLog( tmpstr );
		}


		
		if( (*g_pfnImpersonateSelf)( SecurityImpersonation ) ) //++
		{
			WriteDebugLog( _T( "ImpersonateSelf OK" ) );
			bToken = (*g_pfnOpenThreadToken)( GetCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, phToken );
		}
		else
		{
			WriteDebugLog( _T( "ImpersonateSelf failed" ) );
		}

	}

	if( bToken )
	{
		WriteDebugLog( _T( "OpenThreadToken OK" ) );

		DWORD dwErrorCode = AdjustDebugPrivilege( *phToken, TRUE, pPrevAttributes );

		if( dwErrorCode == ERROR_SUCCESS )
		{
			WriteDebugLog( _T( "AdjustDebugPrivilege: OK" ) );
			bPrivilege = TRUE;
		}
		else
		{
			_stprintf_s( tmpstr, _countof(tmpstr), _T("AdjustDebugPrivilege failed: Error code %lu"), dwErrorCode );
			WriteDebugLog( tmpstr );
			WriteDebugLog( _T( "Warning: did not get the debug privilege." ) );
		}
	}
	else
	{
		*phToken = NULL;
	}

	return bPrivilege;
}

