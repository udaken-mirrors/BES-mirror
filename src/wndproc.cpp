/* 
 *	Copyright (C) 2005-2019 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

volatile BOOL g_bHack[ MAX_SLOTS ];
//volatile DWORD g_dwTargetProcessId[ MAX_SLOTS ];
HANDLE hSemaphore[ MAX_SLOTS ];
volatile int g_Slider[ MAX_SLOTS ];

TCHAR * ppszStatus[ 18 ];
HANDLE hChildThread[ MAX_SLOTS ];
HFONT g_hFont = NULL;

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hSettingsDlg;
extern HWND g_hListDlg;
extern HWND g_hAboutDlg;
extern BOOL g_bLogging;
extern bool g_fRealTime;
extern bool g_fHide;
extern bool g_fLowerPriv;
extern PATHINFO g_arPathInfo[ MAX_WATCH ];
extern ptrdiff_t g_nPathInfo;
extern HANDLE hReconSema;


HBITMAP LoadSkin( HWND hWnd, HDC hMemDC, SIZE& PicSize );
BOOL ChangeSkin( HWND hWnd, HDC hMemDC, SIZE& PicSize, HBITMAP& hOrigBmp );
BOOL DrawSkin( HDC hdc, HDC hMemDC, const SIZE& SkinSize );
HFONT GetFontForURL( HDC hdc, float font_size );
void ShowCommandLineHelp( HWND hWnd );
void ShowWatchList( HWND hWnd );
void NotifyIcon_OnContextMenu( HWND hWnd );
BOOL HasFreeSlot( void );
void DeleteNotifyIcon( HWND hWnd );

CLEAN_POINTER DWORD * PathToProcessIDs( const PATHINFO& pathInfo, ptrdiff_t& numOfPIDs );
bool PathEqual( const PATHINFO& pathInfo, const TCHAR * lpPath, size_t cch, bool fSafe );

BOOL LimitPID( HWND hWnd, TARGETINFO * pTarget, const DWORD pid, int iSlider, BOOL bUnlimit, const int * aiParams ); // v1.7.5
DWORD ParsePID( const TCHAR * strTarget ); // v1.7.5
static HFONT _create_font( LPCTSTR lpszFace, int iSize, BOOL bBold, BOOL bItalic );

static void CheckMenuItemB( HMENU hMenu, UINT uMenuId, bool fCheck );
static bool CheckMenuItemI( HMENU hMenu, UINT uMenuId, const TCHAR * strKey, const TCHAR * strIniPath );

static bool toggle_opt_menu_item( HWND hWnd, UINT uMenuId, const TCHAR * pIniOptionName, int nIfEnabled );
static void SetRealTimeMode( HWND hWnd, bool fRealTime );
static void EnableLoggingIni( BOOL bEnabled );
int UrlHitTest( LPARAM lParam, const RECT& urlrect );
#define MainWindowUrlHit UrlHitTest





LRESULT CALLBACK WndProc( HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	static RECT urlrect = { 420L, 390L, 620L, 420L };
	static HCURSOR hCursor[ 3 ];
	static int iCursor = 0;
	static int iMouseDown = 0;
	//static HACK_PARAMS hp[ MAX_SLOTS ];
	static TARGETINFO ti[ MAX_SLOTS ];
	static HFONT hFontItalic;
	static HWND hButton[ 4 ];
	static HWND hToolTip[ 4 ];
	static TCHAR s_szStatus[ 18 ][ cchStatus ];
	static TCHAR s_szLimitingStatus[ 256 ];
	static TCHAR strToolTips[ 4 ][ 64 ];

	static HDC hMemDC = NULL;
	static HBITMAP hOrigBmp = NULL;
	static SIZE SkinSize = { 0L, 0L };

	static UINT uMsgTaskbarCreated = 0;
	
	switch ( uMessage ) 
	{
		case WM_CREATE:
		{
			g_hWnd = hWnd;


#if 0 //(2014+04+12)
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

extern     GetProcessImageFileName_t
g_pfnGetProcessImageFileName    ;
extern     GetLogicalDriveStrings_t
g_pfnGetLogicalDriveStrings     ;
extern     QueryDosDevice_t
g_pfnQueryDosDevice             ;
extern     GetTokenInformation_t
g_pfnGetTokenInformation        ;
extern     LookupPrivilegeValue_t
g_pfnLookupPrivilegeValue       ;
extern     AdjustTokenPrivileges_t
g_pfnAdjustTokenPrivileges      ;
extern     OpenThreadToken_t
g_pfnOpenThreadToken            ;
extern     ImpersonateSelf_t
g_pfnImpersonateSelf            ;
extern     GetProcessTimes_t
g_pfnGetProcessTimes            ;

TCHAR s[100];
_stprintf_s(s,100,_T("<g_pfnGetProcessImageFileName> %p"),g_pfnGetProcessImageFileName);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnGetLogicalDriveStrings> %p"),g_pfnGetLogicalDriveStrings);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnQueryDosDevice> %p"),g_pfnQueryDosDevice);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnGetProcessTimes> %p"),g_pfnGetProcessTimes);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnGetTokenInformation> %p"),g_pfnGetTokenInformation);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnLookupPrivilegeValue> %p"),g_pfnLookupPrivilegeValue);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnAdjustTokenPrivileges> %p"),g_pfnAdjustTokenPrivileges);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnOpenThreadToken> %p"),g_pfnOpenThreadToken);
WriteDebugLog(s);
_stprintf_s(s,100,_T("<g_pfnImpersonateSelf> %p"),g_pfnImpersonateSelf);
WriteDebugLog(s);

#endif




			TCHAR lpszWindowText[ MAX_WINDOWTEXT ] = _T("");
			if( g_fRealTime )
			{
				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T("%s - Idle (Real-time mode)"), APP_LONGNAME );
				WriteDebugLog( TEXT( "Real-time mode: Yes" ) );
			}
			else
			{
				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T("%s - Idle"), APP_LONGNAME );
				WriteDebugLog( TEXT( "Real-time mode: No" ) );
			}
			SetWindowText( hWnd, lpszWindowText );

			HMENU hMenu = GetMenu( hWnd );
			CheckMenuItemB( hMenu, IDM_REALTIME, g_fRealTime );

#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				InitMenuJpn( hWnd );
				InitToolTipsJpn( strToolTips );
			}
			else if( IS_SPANISH )
			{
				InitMenuSpa( hWnd );
				InitToolTipsSpa( strToolTips );
			}
			else if( IS_FINNISH )
			{
				InitMenuFin( hWnd );
				InitToolTipsFin( strToolTips );
			}
			else if( IS_CHINESE_T )
			{
				InitMenuChiT( hWnd );
				InitToolTipsChiT( strToolTips );
			}
			else if( IS_CHINESE_S )
			{
				InitMenuChiS( hWnd );
				InitToolTipsChiS( strToolTips );
			}
			else if( IS_FRENCH )
			{
				InitMenuFre( hWnd );
				InitToolTipsFre( strToolTips );
			}
			else
			{
				InitMenuEng( hWnd );
				InitToolTipsEng( strToolTips );
			}
#else
			InitToolTipsEng( strToolTips );

			HMENU hOptMenu = GetSubMenu( hMenu, 2 );
			
			// "Language" sub-submenu
			EnableMenuItem( hOptMenu, 4U, MF_BYPOSITION | MF_GRAYED );
#endif
			CheckLanguageMenuRadio( hWnd );

			CheckMenuItemB( hMenu, IDM_LOGGING, !! g_bLogging );

			const TCHAR * strIniPath = GetIniPath();
			bool fAllowMoreThan99 = CheckMenuItemI( hMenu, IDM_ALLOWMORETHAN99, _T("AllowMoreThan99"), strIniPath );

			bool fListAll = ( GetPrivateProfileInt( TEXT( "Options" ), TEXT( "ListAll" ), 0, strIniPath ) == 2 );
			CheckMenuItemB( hMenu, IDM_ALWAYS_LISTALL, fListAll );

			CheckMenuItemI( hMenu, IDM_DISABLE_F1, _T("DisableF1"), strIniPath );
			CheckMenuItemB( hMenu, IDM_LOWER_PRIVILEGE, g_fLowerPriv );


			const UINT wrt = GetPrivateProfileInt( _T("Options"), _T("WatchRT"), 80, strIniPath );
			CheckMenuRadioItem( GetMenu( hWnd ), IDM_WATCH_RT8, IDM_WATCH_RT2,
								(UINT)( wrt == 20 ? IDM_WATCH_RT2
										: wrt == 40 ? IDM_WATCH_RT4
										: IDM_WATCH_RT8 ),
								MF_BYCOMMAND );

			g_hFont = _create_font( TEXT( "Tahoma" ), 17, TRUE, FALSE );

			HDC hDC = GetDC( hWnd );
			double k = 96.0 / GetDeviceCaps( hDC, LOGPIXELSY ); // 20140412
			hFontItalic = MyCreateFont( hDC, TEXT( "Georgia" ), (int)( 13 * k + 0.5 ), FALSE, TRUE );
			hMemDC = CreateCompatibleDC( hDC );

			HFONT hFontURL = GetFontForURL( hDC, 10.0 );
			HFONT hOldFont = SelectFont( hDC, hFontURL );
			SIZE size;
			GetTextExtentPoint32( hDC, APP_HOME_URL, 28, &size );
			SelectFont( hDC, hOldFont );
			DeleteFont( hFontURL );
			ReleaseDC( hWnd, hDC );

			RECT rc;
			GetClientRect( hWnd, &rc );
			urlrect.left = rc.right - 20 - size.cx;
			urlrect.top = rc.bottom - 20 - size.cy;
			urlrect.right = rc.right - 20;
			urlrect.bottom = rc.bottom - 20;

			if( hMemDC )
				hOrigBmp = LoadSkin( hWnd, hMemDC, SkinSize );
			else
				EnableMenuItem( hMenu, IDM_SKIN, MF_BYCOMMAND | MF_GRAYED );

			hButton[ 0 ] = CreateWindow( TEXT( "BUTTON" ), TEXT( "&Target..." ), 
				WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
				480,30,130,75,
				hWnd, (HMENU)IDM_LIST, g_hInst, NULL );
			hButton[ 1 ] = CreateWindow( TEXT( "BUTTON" ), TEXT( "&Unlimit all" ), 
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				480,125,130,50,
				hWnd, (HMENU)IDM_STOP, g_hInst, NULL );
			hButton[ 2 ] = CreateWindow( TEXT( "BUTTON" ), TEXT( "&Control..." ), 
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				480,195,130,75,
				hWnd, (HMENU)IDM_SETTINGS, g_hInst, NULL );
			hButton[ 3 ] = CreateWindow( TEXT( "BUTTON" ), TEXT( "E&xit" ), 
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				480,330,130,50,
				hWnd, (HMENU)IDM_EXIT, g_hInst, NULL );

			EnableWindow( hButton[ 1 ], FALSE );
			EnableWindow( hButton[ 2 ], FALSE );
			EnableMenuItem( hMenu, IDM_SETTINGS, MF_BYCOMMAND | MF_GRAYED );
			EnableMenuItem( hMenu, IDM_STOP, MF_BYCOMMAND | MF_GRAYED );
			EnableMenuItem( hMenu, IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
			
			ZeroMemory( ti, sizeof(TARGETINFO) * MAX_SLOTS ); // todo: wasteful
			ZeroMemory( hChildThread, sizeof(HANDLE) * MAX_SLOTS );

			ptrdiff_t i = 0;
			for( ; i < MAX_SLOTS; ++i )
			{
				g_bHack[ i ] = FALSE;
				ti[ i ].slotid = (WORD) i;
				hSemaphore[ i ] = CreateSemaphore( NULL, 1L, 1L, NULL ); // todo : wasteful
			}

			for( i = 0; i < 18; ++i )
				ppszStatus[ i ] = &(s_szStatus[ i ][ 0 ]);

			for( i = 0; i < 4; ++i )
			{
				hToolTip[ i ] = CreateTooltip( g_hInst, hButton[ i ], strToolTips[ i ] );
				SendMessage( hButton[ i ], WM_SETFONT, (WPARAM) hFontItalic, 0L );
			}
			_tcscpy_s( s_szStatus[ 0 ], cchStatus, APP_LONGNAME );
			_tcscpy_s( s_szStatus[ 1 ], cchStatus, _T( "Hit [A] for more info." ) );

			uMsgTaskbarCreated = RegisterWindowMessage( TEXT( "TaskbarCreated" ) );

			TCHAR strTargetPath[ CCHPATH ] = _T("");
			TCHAR strTargetExe[ CCHPATH ] = _T("");
			TCHAR strOptions[ CCHPATH ] = _T("");

			const TCHAR * lpszCommandLine = GetCommandLine();

			int aiParams[ 2 ] = { 0, 0 };
			int mode = DEF_MODE;
			const int iSlider = ParseArgs( lpszCommandLine, CCHPATH,
											strTargetPath, strTargetExe, strOptions,
											fAllowMoreThan99, &aiParams[ 0 ], &mode );

			const DWORD pid = ParsePID( strTargetPath ); // v1.7.5

			TCHAR dbgstr[ 128 ] = _T("");
			_stprintf_s( dbgstr, _countof(dbgstr), 
				_T("cycle=%d, delay=%d, mode=%d, pid=%lu @ WM_CREATE"), aiParams[0], aiParams[1], mode, pid );
			WriteDebugLog( dbgstr );

			const BOOL bUnlimit = IsOptionSet( strOptions, _T("--unlimit"), _T("-u") );

			if( pid )
			{
				LimitPID( hWnd, &ti[ 0 ], pid, iSlider, bUnlimit, &aiParams[ 0 ] );
			}
			else if( iSlider == IGNORE_ARGV )
			{
				; // Do nothing
			}
			else if( IsOptionSet( strOptions, _T("--job-list"), _T("-J") ) )
			{
				HandleJobList( hWnd, lpszCommandLine, fAllowMoreThan99, ti );
			}
			else if( 2 <= _tcslen( strTargetPath ) && ! bUnlimit )
			{
				// It's ok even if strTargetPath is an instance of BES.
				// Watch and Hack both can handle such a situation properly.
				ti[ 2 ].dwProcessId = TARGET_PID_NOT_SET; // (0) just in case
				SetTargetPlus( hWnd, ti, strTargetPath, iSlider, &aiParams[ 0 ], mode );
			}

			SendNotifyIconData( hWnd, ti, NIM_ADD );
			
			hCursor[ 0 ] = LoadCursor( (HINSTANCE) NULL, IDC_ARROW );
			hCursor[ 2 ] = hCursor[ 1 ] = LoadCursor( (HINSTANCE) NULL, IDC_HAND );
			SetCursor( hCursor[ 0 ] );

			SetParent( hWnd, NULL );
			break;
		}

		case WM_INITMENUPOPUP: // TODO: this does not necessary
		{
			if( LOWORD(lParam) != 2 || HIWORD(lParam) ) break;
			
			HMENU hMenu = GetMenu( hWnd );
			if( wParam == (WPARAM) GetSubMenu( hMenu, 2 ) )
			{
				CheckMenuItemI( hMenu, IDM_ALLOWMULTI, _T("AllowMulti"), GetIniPath() );
			}
			break;
		}

		case WM_SIZE:
		{
			if( wParam == SIZE_MINIMIZED )
				ShowWindow( hWnd, SW_HIDE );
			break;
		}

		case WM_MOVE:
		{
			if( IsWindowVisible( hWnd ) && ! IsIconic( hWnd ) )
				SetWindowPosIni( hWnd );
			break;				
		}

		case WM_USER_RESTART:
		{
			int id = (int) LOWORD( wParam );
			int mode = (int) HIWORD( wParam );
			if( id < MAX_SLOTS && id != 3 )
			{
				// prevent double restarting
				if( WaitForSingleObject( hSemaphore[ id ], 200 ) == WAIT_OBJECT_0 )
				{
					ti[ id ].mode = (WORD) mode;
					SendMessage( hWnd, WM_USER_HACK, (WPARAM) id, (LPARAM) &ti[ id ] );
				}
				else
					MessageBox( hWnd, TEXT( "Semaphore Error" ), APP_NAME, MB_OK | MB_ICONSTOP );
			}
			break;
		}

		case WM_USER_HACK:
		{
			unsigned iMyId = LOWORD( wParam );
			if( MAX_SLOTS <= iMyId || iMyId == 3 || ! lParam ) break;
			if( hChildThread[ iMyId ] )
			{
#ifdef _DEBUG
				MessageBox( hWnd, _T("hChildThread[ iMyId ] is not NULL"), NULL, MB_ICONSTOP );
#endif
				break;
			}
			TARGETINFO * lpTarget = (TARGETINFO*) lParam;
			lpTarget->fRealTime = g_fRealTime;
			lpTarget->fUnlimited = false;
			g_bHack[ iMyId ] = TRUE;

			hChildThread[ iMyId ] = CreateThread2( &Hack, (void *) lParam );

			if( hChildThread[ iMyId ] == NULL )
			{
				ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LPLONG) NULL );
				g_bHack[ iMyId ] = FALSE;
				MessageBox( hWnd, TEXT( "CreateThread failed." ), APP_NAME, MB_OK | MB_ICONSTOP );
			}
			else
			{
///				SetThreadPriority( hChildThread[ iMyId ], THREAD_PRIORITY_TIME_CRITICAL );

				TCHAR lpszWindowText[ MAX_WINDOWTEXT ] = _T("");
				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T( "%s - Active%s" ), APP_LONGNAME,
					g_fRealTime ? _T( " (Real-time mode)" ) : _T( "" ) );

				SetWindowText( hWnd, lpszWindowText );
				SendMessage( hWnd, WM_USER_STOP, JUST_UPDATE_STATUS, STOPF_LESS_FLICKER );
			}
			break;
		}

		case WM_USER_STOP:
		{
			TCHAR szStr[ 128 ];
			_stprintf_s( szStr, _countof(szStr),
				_T( "WM_USER_STOP: wParam = 0x%04lX , lParam = 0x%04lX" ), wParam, (UINT) lParam );
			WriteDebugLog( szStr );

			unsigned iMyId = LOWORD( wParam );

			if( iMyId < MAX_SLOTS && iMyId != 3 )
			{
				g_bHack[ iMyId ] = FALSE;

				// FIX: race condition 2014-03-27
				if( ! hChildThread[ iMyId ] ) break;

				DWORD dwExitCode = (DWORD) -1;
				DWORD dwWaitResult = WaitForSingleObject( hSemaphore[ iMyId ], 1000 );
				ti[ iMyId ].fUnlimited = ( lParam & STOPF_UNLIMIT )? true : false ;

				if( dwWaitResult == WAIT_OBJECT_0 )
				{
#if 0
					for( int t = 0; t < 50; t++ )
					{
						GetExitCodeThread( hChildThread[ iMyId ], &dwExitCode );
						if( dwExitCode != STILL_ACTIVE )
						{
							break;
						}
						Sleep( 50UL );
					}
#else
					// Typically one has to wait less-than-1 ~ about 20 ms
					if( ! WaitForSingleObject( hChildThread[ iMyId ], 250 ) == WAIT_OBJECT_0 )
						WriteDebugLog(_T("###Bad Wait###"));

					GetExitCodeThread( hChildThread[ iMyId ], &dwExitCode );
#endif
					_stprintf_s( szStr, _countof(szStr), 
						_T( "ChildThread[ %d ] : ExitCode = 0x%04lX" ), iMyId, dwExitCode );
					WriteDebugLog( szStr );
					ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
					if( iMyId < 3 )
					{
						_stprintf_s( s_szStatus[ 0 + iMyId * 4 ], cchStatus,
									_T( "Target #%d %s" ),
									iMyId + 1,
									dwExitCode == NORMAL_TERMINATION ? _T( "OK" )
									: dwExitCode == THREAD_NOT_OPENED ? _T( "Access denied" )
									: dwExitCode == TARGET_MISSING ? _T( "Target missinig" )
									: dwExitCode == STILL_ACTIVE ? _T( "Time out" )
									: _T( "Status unknown" ) );
					}
				}
				else
				{
					WriteDebugLog( TEXT( "### Semaphore Error ###" ) );
					if( hChildThread[ iMyId ] )
					{
						GetExitCodeThread( hChildThread[ iMyId ], &dwExitCode );
					}
					_stprintf_s( szStr, _countof(szStr), 
							_T( "ChildThread[ %d ] : ExitCode = 0x%04lX" ), iMyId, dwExitCode );
					WriteDebugLog( szStr );

					if( iMyId < 3 )
					{
						_stprintf_s( s_szStatus[ 0 + iMyId * 4 ], cchStatus,
									_T( "Target #%d Semaphore Error" ), iMyId + 1 );
					}
				}

				if( hChildThread[ iMyId ] )
				{
					CloseHandle( hChildThread[ iMyId ] );
					hChildThread[ iMyId ] = NULL;
				}


				EnableWindow( hButton[ 0 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_ENABLED );
			}
			
			if( IsActive() )
			{
#ifdef _UNICODE
				if( IS_JAPANESE )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_2000, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else if( IS_FINNISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_2000, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else if( IS_SPANISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_2000, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else if( IS_CHINESE_T )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000T, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else if( IS_CHINESE_S )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000S, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else if( IS_FRENCH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_2000, -1, s_szLimitingStatus, _countof(s_szLimitingStatus) );
				}
				else
#endif
				{
					_tcscpy_s( s_szLimitingStatus, _countof(s_szLimitingStatus),
								_T(" Limiting CPU load:") );
				}
				
				ptrdiff_t e = 0;
				TCHAR buf[ 128 ];
				int numOfActiveSlots = 0;
				for( ; e < MAX_SLOTS && numOfActiveSlots < 7; ++e )
				{
					if( e == 3 ) continue;
					if( g_bHack[ e ] )
					{
						_stprintf_s( buf, _countof(buf), _T(" #%d"), (int)( e < 3 ? e + 1 : e ) );
						_tcscat_s( s_szLimitingStatus, _countof(s_szLimitingStatus), buf );
						++numOfActiveSlots;
					}
				}
				const int num1 = numOfActiveSlots;
				for( ; e < MAX_SLOTS; ++e ) { if( g_bHack[ e ] ) ++numOfActiveSlots; }
				if( numOfActiveSlots )
				{
					_stprintf_s( buf, _countof(buf),
								_T("%s\n   %d slot%s active"),
									( num1 < numOfActiveSlots ) ? _T(" etc.") : _T(""),
									numOfActiveSlots,
									( numOfActiveSlots != 1 ? _T("s are") :_T(" is") ));
					_tcscat_s( s_szLimitingStatus, _countof(s_szLimitingStatus), buf );
				}

				EnableWindow( hButton[ 1 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_STOP, MF_BYCOMMAND | MF_ENABLED );
				EnableWindow( hButton[ 2 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_SETTINGS, MF_BYCOMMAND | MF_ENABLED );
				EnableWindow( hButton[ 3 ], FALSE );
				EnableMenuItem( GetMenu( hWnd ), IDM_EXIT, MF_BYCOMMAND | MF_GRAYED );
				EnableMenuItem( GetMenu( hWnd ), IDM_EXIT_ANYWAY, MF_BYCOMMAND | MF_ENABLED );

				SetClassLongPtr_Floral( hWnd, GCL_HICON, (LONG_PTR) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ACTIVE ) ) );
				SetClassLongPtr_Floral( hWnd, GCL_HICONSM, (LONG_PTR) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ACTIVE ) ) );
				SendMessage( hWnd, WM_SETICON, ICON_BIG, (LPARAM) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ACTIVE ) ) );
				SendMessage( hWnd, WM_SETICON, ICON_SMALL, (LPARAM) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_ACTIVE ) ) );
			}
			else // not active
			{
				*s_szLimitingStatus = _T('\0');
				if( g_bHack[ 3 ] /*g_dwTargetProcessId[ 2 ] == WATCHING_IDLE*/ )
				{
					EnableWindow( hButton[ 1 ], TRUE );
					EnableMenuItem( GetMenu( hWnd ), IDM_STOP, MF_BYCOMMAND | MF_ENABLED );
				}
				else
				{
					EnableWindow( hButton[ 1 ], FALSE );
				}
				if( ti[ 0 ].dwProcessId || ti[ 1 ].dwProcessId || ti[ 2 ].dwProcessId
					|| g_bHack[ 3 ] )
				{
					EnableWindow( hButton[ 2 ], TRUE );
					EnableMenuItem( GetMenu( hWnd ), IDM_SETTINGS, MF_BYCOMMAND | MF_ENABLED );
				}
				else
				{
					EnableWindow( hButton[ 2 ], FALSE );
					EnableMenuItem( GetMenu( hWnd ), IDM_SETTINGS, MF_BYCOMMAND | MF_GRAYED );
				}

				EnableWindow( hButton[ 3 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_EXIT, MF_BYCOMMAND | MF_ENABLED );
				EnableMenuItem( GetMenu( hWnd ), IDM_EXIT_ANYWAY, MF_BYCOMMAND | MF_GRAYED );

				SetClassLongPtr_Floral( hWnd, GCL_HICON, (LONG_PTR) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_IDLE ) ) );
				SetClassLongPtr_Floral( hWnd, GCL_HICONSM, (LONG_PTR) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_IDLE ) ) );
				SendMessage( hWnd, WM_SETICON, ICON_BIG, (LPARAM) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_IDLE ) ) );
				SendMessage( hWnd, WM_SETICON, ICON_SMALL, (LPARAM) LoadIcon( g_hInst, MAKEINTRESOURCE( IDI_IDLE ) ) );

				TCHAR lpszWindowText[ MAX_WINDOWTEXT ] = _T("");
				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T( "%s - Idle%s" ), APP_LONGNAME,
					g_fRealTime ? _T( " (Real-time mode)" ) : _T( "" ) );
				SetWindowText( hWnd, lpszWindowText );
			}
/***
			if( g_bHack[ 3 ] )
			{
				if( g_nPathInfo >= MAX_WATCH ) EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
				else EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED );
				EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_ENABLED );
			}
			else
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED );
				EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
			}
***/
			/*if( g_bHack[ 0 ] && g_bHack[ 1 ] && ( g_bHack[ 2 ] || g_bHack[ 3 ] ) )
			{
				EnableWindow( hButton[ 0 ], FALSE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_GRAYED );
			}
			else*/
			{
				EnableWindow( hButton[ 0 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_ENABLED );
			}

			/**if( g_bHack[ 2 ] || g_bHack[ 3 ] )
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
			}
			else
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED );
			}**/

			SendNotifyIconData( hWnd, ti, NIM_MODIFY );

			if( g_hSettingsDlg )
				SendMessageTimeout( g_hSettingsDlg, WM_USER_REFRESH, 0, 0,
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );
			
			if( g_hListDlg && !(lParam & STOPF_NO_LV_REFRESH) )
				SendMessageTimeout( g_hListDlg, WM_USER_REFRESH, (WPARAM) -1, 0,
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 500, NULL );

			if( ! g_hSettingsDlg && ! g_hListDlg )
				SetFocus( hWnd );//*** v1.3.8: needed if a just-disabled button was focused

			if( !( lParam & STOPF_NO_INVALIDATE ) )
			{
				RECT rc;
				GetClientRect( hWnd, &rc );
				if( lParam & STOPF_LESS_FLICKER ) rc.right = 475L;
				InvalidateRect( hWnd, &rc, FALSE );
			}
			break;
		}

		case WM_USER_NOTIFYICON:
		{
			static bool fClicked = false;
			static bool fContextMenuOn = false;

#ifdef NOTIFYICON_VERSION_4
			if( HIWORD(lParam) != NI_ICONID ) break;
			lParam = LOWORD(lParam);
#else
			if( wParam != NI_ICONID ) break;
#endif
			if( lParam == WM_LBUTTONDOWN )
			{
				fClicked = true;
			}
			else if( lParam == WM_LBUTTONUP	|| lParam == WM_LBUTTONDBLCLK
					|| lParam == NIN_KEYSELECT || lParam == NIN_SELECT )
			{
				if( fClicked || lParam != WM_LBUTTONUP )
				{
					ShowWindow( hWnd, SW_RESTORE );
					SetForegroundWindow( hWnd );
				}
				fClicked = false;
			}
			else if( lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU )
			{
				fClicked = false;
				
				// Handling WM_CONTEXTMENU is enough on WinXP; not sure if we get WM_CONTEXTMENU
				// on Win2K.  On VK_APPS, only WM_CONTEXTMENU comes.
				if( ! fContextMenuOn )
				{
					fContextMenuOn = true;
					NotifyIcon_OnContextMenu( hWnd );
					fContextMenuOn = false;
				}
			}
			break;
		}

		// Handling rare cases when the user LBUTTONDOWNed on one of the buttons
		// created as child windows, and then he or she LBUTTONUPed *not* on it.
		// The button was not pushed after all,
		// and without this algo, the button will keep having the focus.
		// We let the parent (hWnd) reget the focus here.
		// WM_USER_BUTTON is sent from the GetMessage Loop (main.cpp)
		// whenever WM_LBUTTONUP is sent to a window whose handle is not hWnd.
		// wParam is its handle.
		case WM_USER_BUTTON:
		{
			HWND h = (HWND) wParam;
			// Who received WM_LBUTTONUP ?
			int iHotButton;
			if( h == hButton[ 0 ] ) iHotButton = 0;
			else if( h == hButton[ 1 ] ) iHotButton = 1;
			else if( h == hButton[ 2 ] ) iHotButton = 2;
			else if( h == hButton[ 3 ] ) iHotButton = 3;
			else break;

			// Where is the Mouse Cursor now?
			POINT pt;
			GetCursorPos( &pt );
			ScreenToClient( hWnd, &pt );
			int x = (int) pt.x;
			int y = (int) pt.y;

			// Is the Mouse Cursor on the 'hot' button?
			int iCursorPos;
			if( x < BTN_X1 || x > BTN_X2 ) iCursorPos = -1;
			else if( y >= BTN0_Y1 && y <= BTN0_Y2 ) iCursorPos = 0;
			else if( y >= BTN1_Y1 && y <= BTN1_Y2 ) iCursorPos = 1;
			else if( y >= BTN2_Y1 && y <= BTN2_Y2 ) iCursorPos = 2;
			else if( y >= BTN3_Y1 && y <= BTN3_Y2 ) iCursorPos = 3;
			else iCursorPos = -1;

			// If no, we reset the focus.
			if( iCursorPos != iHotButton )
			{
				SetFocus( hWnd );
			}
			break;
		}

		case WM_PARENTNOTIFY: //v1.3.8
		{
#if !(_WIN32_WINNT >= 0x0500)
#define WM_XBUTTONDOWN                  0x020B
#endif
			const int event = (int) LOWORD( wParam );
			if( event == WM_LBUTTONDOWN || event == WM_RBUTTONDOWN ||
				event == WM_MBUTTONDOWN || event == WM_XBUTTONDOWN )
			{
				POINT pt;
				pt.x = (LONG) GET_X_LPARAM( lParam );
				pt.y = (LONG) GET_Y_LPARAM( lParam );
				HWND hwndChild = ChildWindowFromPointEx( hWnd, 
					pt, CWP_SKIPDISABLED | CWP_SKIPINVISIBLE );
				if( hwndChild != NULL )
				{
					const int idControl = GetWindowID( hwndChild );
					const int idx = ( idControl==IDM_LIST )? 0
						: ( idControl==IDM_STOP )? 1
						: ( idControl==IDM_SETTINGS )? 2
						: 3
						;
					SendMessage( hToolTip[ idx ], TTM_ACTIVATE, 
						(WPARAM) FALSE, (LPARAM) 0 );
					SendMessage( hToolTip[ idx ], TTM_ACTIVATE, 
						(WPARAM) TRUE, (LPARAM) 0 );
				}
			}
			break;
		}

		// on WM_COPYDATA, return TRUE if data is fully handled by this instance, as in the target
		// specified is recognized as the progess #1 or #2 or #3; otherwise return FALSE.
		// If TRUE is returned, the caller stops calling FindWindowEx; if FALSE is returned,
		// the caller keeps calling FindWindowEx, sending WM_COPYDATA to the next instance of BES.
		case WM_COPYDATA:
		{
			const COPYDATASTRUCT * _pcd = (COPYDATASTRUCT *) lParam;
			if( _pcd == NULL
				|| _pcd->dwData != BES_COPYDATA_ID
				|| _pcd->cbData != sizeof(BES_COPYDATA)
				|| _pcd->lpData == NULL )
			{
				return (LRESULT) FALSE;
				break;
			}

			BES_COPYDATA mydata; // local copy (the origial will be freed if it's timed out)
			memcpy( &mydata, _pcd->lpData, sizeof(BES_COPYDATA) );
	
#ifdef _DEBUG
			TCHAR str[ 1024 ] = _T("");
			_stprintf_s( str, 1024,
						_T( "BES PID 0x%08lx (%p): Received message 0x%08x from PID 0x%08x" ),
						GetCurrentProcessId(), hWnd, mydata.uCommand, mydata.dwParam );
			WriteDebugLog( str );
#endif
			
			BOOL bHandled = FALSE; // ret.val
			switch( mydata.uCommand )
			{
			case BES_COPYDATA_COMMAND_EXITNOW:
				SendMessage( hWnd, WM_COMMAND, IDM_STOP, 0 );
				PostMessage( hWnd, WM_COMMAND, IDM_EXIT, 0 );
				break;

			case BES_COPYDATA_COMMAND_PARSE:
			{
				TCHAR strCommand[ CCH_MAX_CMDLINE ] = _T("");
#ifdef _UNICODE
				if( mydata.uFlags & BESCDF_ANSI )
					MultiByteToWideChar( CP_ACP, 0L, (char*) mydata.wzCommand, -1,
											strCommand, CCH_MAX_CMDLINE );
				else
					wcscpy_s( strCommand, CCH_MAX_CMDLINE, mydata.wzCommand );
#else
				if( mydata.uFlags & BESCDF_ANSI )
					strcpy_s( strCommand, CCH_MAX_CMDLINE, (char*) mydata.wzCommand );
				else
					WideCharToMultiByte( CP_ACP, 0L, mydata.wzCommand, -1,
											strCommand, CCH_MAX_CMDLINE, NULL, NULL );
#endif
				if( ! strCommand[ 0 ] ) break;
				
				TCHAR strTargetPath[ CCHPATH ] = _T("");
				TCHAR strTargetExe[ CCHPATH ] = _T("");
				TCHAR strOptions[ CCHPATH ] = _T("");

				HMENU hMenu = GetMenu( hWnd );
				bool fAllowMoreThan99 = IsMenuChecked( hMenu, IDM_ALLOWMORETHAN99 );
				int aiParams[ 2 ] = { 0, 0 };
				int mode = DEF_MODE;
				const int iSlider = ParseArgs( strCommand, CCHPATH, strTargetPath, strTargetExe, strOptions, fAllowMoreThan99, &aiParams[ 0 ], &mode );

				const DWORD pid = ParsePID( strTargetPath ); // v1.7.5

				TCHAR dbgstr[ 128 ] = _T("");
				_stprintf_s( dbgstr, _countof(dbgstr), _T("cycle=%d, delay=%d, pid=%lu @ WM_COPYDATA"), aiParams[0], aiParams[1], pid );
				WriteDebugLog( dbgstr );

				const BOOL bUnlimit = IsOptionSet( strOptions, _T("--unlimit"), _T("-u") );

				if( pid )
				{
					LimitPID( hWnd, &ti[ 0 ], pid, iSlider, bUnlimit, &aiParams[ 0 ] );
					bHandled = TRUE;
					break;
				}
				else if( iSlider == IGNORE_ARGV )
				{
					bHandled = TRUE;
					break;
				}
				else if( IsOptionSet( strOptions, _T("--hide"), NULL ) )
				{
					g_fHide = true;
					ShowWindow( hWnd, SW_HIDE );
					DeleteNotifyIcon( hWnd );
					bHandled = TRUE;
					break;
				}
				else if( IsOptionSet( strOptions, _T("--reshow"), NULL ) )
				{
					g_fHide = false;
					SendNotifyIconData( hWnd, ti, NIM_ADD );
					bHandled = TRUE;
					break;
				}
				else if( IsOptionSet( strOptions, _T("--job-list"), _T("-J") ) )
				{
					HandleJobList( hWnd, strCommand, fAllowMoreThan99, ti );
					bHandled = TRUE;
					break;
				}

				BOOL bAdd = IsOptionSet( strOptions, _T("--add"), _T("-a") ) && ( 2 <= _tcslen( strTargetPath ) );

				const TCHAR * pStar = _tcschr( strTargetPath, _T('*') );
				if( iSlider != -1 && strTargetExe[ 0 ] && ! pStar )
				{
					SetSliderIni( strTargetExe, iSlider );
				}
#if 0
				if( ( bAdd || IsOptionSet( strOptions, _T("--watch-multi"), NULL ) )
					&&
					! IsMenuChecked( hMenu, IDM_WATCH_MULTI ) )
				{
					_toggle_opt_menu_item( hWnd, IDM_WATCH_MULTI, _T("WatchMulti") );
				}
#endif
				PATHINFO pathInfo;
				ZeroMemory( &pathInfo, sizeof(PATHINFO) );
				if( pStar )
				{
					pathInfo.cchHead = (size_t)( pStar - strTargetPath );
					pathInfo.cchTail = _tcslen( pStar + 1 );
				}
				pathInfo.cchPath = _tcslen( strTargetPath );
				pathInfo.slider = iSlider;
				if( aiParams[ 0 ] != -1 ) pathInfo.wCycle = (WORD) aiParams[ 0 ];
				if( aiParams[ 1 ] != -1 ) pathInfo.wDelay = (WORD) aiParams[ 1 ];
				if( mode == 0 || mode == 1 ) pathInfo.mode = (WORD) mode;

				if( g_bHack[ 3 ] && bAdd )
				{
					size_t cchMem = pathInfo.cchPath + 1;
					TCHAR * lp = TcharAlloc( cchMem );
					if( ! lp ) return TRUE;
					_tcscpy_s( lp, cchMem, strTargetPath );

					pathInfo.pszPath = lp;
					
					for( ptrdiff_t sx = 0; sx < MAX_SLOTS; ++sx ) // (1) Update Sliders and/or ReLimit if needed
					{
						if( sx != 3 && ( g_bHack[ sx ] || ti[ sx ].fUnlimited ) )
						{
							const TCHAR * q = ti[ sx ].disp_path
												? ti[ sx ].disp_path : ti[ sx ].lpPath ;
							if( PathEqual( pathInfo, q, _tcslen( q ), false ) )
							{
								if( iSlider != -1 ) g_Slider[ sx ] = iSlider;
								ti[ sx ].fUnlimited = false;
								if( aiParams[ 0 ] != -1 ) ti[ sx ].wCycle = (WORD) aiParams[ 0 ];
								if( aiParams[ 1 ] != -1 ) ti[ sx ].wDelay = (WORD) aiParams[ 1 ];
								if( mode == 0 || mode == 1 ) ti[ sx ].mode = (WORD) mode;
							}
						}
					}
					
					if( WaitForSingleObject( hReconSema, 1000 ) == WAIT_OBJECT_0 ) // (2) Update the g_arPathInfo table (esp. if the target is new)
					{
						if( 1 <= g_nPathInfo && g_nPathInfo < MAX_WATCH )
						{
							ptrdiff_t px = 0;
							for( ; px < g_nPathInfo; ++px )
							{
								if( StrEqualNoCase( strTargetPath,  g_arPathInfo[ px ].pszPath ) )
								{
									TcharFree( g_arPathInfo[ px ].pszPath );
									break;
								}
							}
							memcpy( &g_arPathInfo[ px ], &pathInfo, sizeof(PATHINFO) );
							if( g_nPathInfo == px ) ++g_nPathInfo;
						}
						ReleaseSemaphore( hReconSema, 1L, NULL );
					}
					PostMessage( hWnd, WM_USER_STOP, JUST_UPDATE_STATUS, STOPF_NO_INVALIDATE );
					return (LRESULT) TRUE; // bHandled
				} // if( g_bHack[ 3 ] && bAdd )

				if( ( aiParams[ 0 ] != -1 || aiParams[ 1 ] != -1 ) && 1 <= g_nPathInfo && g_nPathInfo < MAX_WATCH ) // Update the g_arPathInfo table
				{
					if( WaitForSingleObject( hReconSema, 1000 ) == WAIT_OBJECT_0 )
					{
						for( ptrdiff_t px = 0; px < g_nPathInfo; ++px )
						{
							if( StrEqualNoCase( strTargetPath, g_arPathInfo[ px ].pszPath ) )
							{
								if( aiParams[ 0 ] != -1 ) g_arPathInfo[ px ].wCycle = (WORD) aiParams[ 0 ];
								if( aiParams[ 1 ] != -1 ) g_arPathInfo[ px ].wDelay = (WORD) aiParams[ 1 ];
								if( mode == 0 || mode == 1 ) g_arPathInfo[ px ].mode = (WORD) mode;
								break;
							}
						}
						ReleaseSemaphore( hReconSema, 1L, NULL );
					}
				}

				pathInfo.pszPath = strTargetPath; // used in the "while" below

				ptrdiff_t numOfPIDs = 0;
				DWORD * rgPIDs = PathToProcessIDs( pathInfo, numOfPIDs ); //TODO what if rgPIDs=NULL?
				const BOOL bToggle = ! bUnlimit && IsOptionSet( strOptions, _T("--toggle"), _T("-t") );
				BOOL bRewatch = FALSE;
				
				for( ptrdiff_t i = 0; i < MAX_SLOTS; ++i )
				{
					if( i == 3 || ! g_bHack[ i ] && ! ti[ i ].fUnlimited ) continue; //TODO
					for( ptrdiff_t n = 0; n < numOfPIDs; ++n )
					{
						if( rgPIDs[ n ] == ti[ i ].dwProcessId )
						{
							if( iSlider != -1 )
							{
								g_Slider[ i ] = iSlider;
								if( pStar )
								{
									const TCHAR * q = ti[ i ].disp_path;
									if( ! q ) q = ti[ i ].lpPath;
									SetSliderIni( q, iSlider );
								}
							}

							if( aiParams[ 0 ] != -1 ) ti[ i ].wCycle = (WORD) aiParams[ 0 ];
							if( aiParams[ 1 ] != -1 ) ti[ i ].wDelay = (WORD) aiParams[ 1 ];
							if( mode == 0 || mode == 1 ) ti[ i ].mode = (WORD) mode;

							if( g_bHack[ i ] ) // currently limited
							{
								if( bUnlimit || bToggle )
								{
									if( i == 2 && g_bHack[ 3 ] )
										SendMessage( hWnd, WM_COMMAND, IDM_UNWATCH, 0 );

									g_bHack[ i ] = FALSE;
									SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, STOPF_UNLIMIT );
								}
							}
							else if( i == 2 && ! g_bHack[ 2 ] && ! g_bHack[ 3 ] ) // unwatched/unlimited
							{
								bRewatch = TRUE;
							}
							else // currently unlimited
							{
								SendMessage( hWnd, WM_USER_RESTART, (WPARAM) i, 0 );
							}

							bHandled = TRUE;
							
							break;  // break the inner loop; next i
						} // pid mapped to slot
					} // for n
				} // for i

				if( rgPIDs ) MemFree( rgPIDs );

				if( bHandled && ! bRewatch )
					break;

				if( ! *strTargetPath ) // target not specified
				{
					if( bUnlimit ) // "unlimit all"
						PostMessage( hWnd, WM_COMMAND, IDM_STOP_FROM_TRAY, 0 );
					
					break;
				}

				// Unwatch
				if( ! bHandled && g_bHack[ 3 ] )// watching idly
				{
#if 1 // ANSIFIX
					if( ti[ 2 ].disp_path && StrEqualNoCase( ti[ 2 ].disp_path, strTargetPath )
						||
						ti[ 2 ].lpPath && StrEqualNoCase( ti[ 2 ].lpPath, strTargetPath ) )
#else
					// In non-Unicode, a watched target path may be a full-path, an exe-only path,
					// or possibly a 15-character truncated version of a long exe name.
					if( _stricmp( strTargetPath, ti[ 2 ].szPath ) == 0
						|| _strnicmp( strTargetExe, ti[ 2 ].szPath, 15 ) == 0 )
#endif
					{
						if( SLIDER_MIN <= iSlider )
							g_Slider[ 2 ] = iSlider;

						if( bUnlimit || bToggle )
							SendMessage( hWnd, WM_COMMAND, IDM_UNWATCH, 0 );
					
						bHandled = TRUE;
					}
				}

				if( bUnlimit // unlimit it; don't watch
					|| ( bToggle && bHandled ) && ! bRewatch // toggle-handled; don't watch (in this case g_bHack[ 3 ] is true anyway)
					|| g_bHack[ 3 ]
				) break;

				ti[ 2 ].dwProcessId = TARGET_PID_NOT_SET; // reset


#if ! defined(_UNICODE)
				//strcpy_s( strTargetPath, CCHPATH, strTargetExe );//ANSIFIX7
#endif

				SetTargetPlus( hWnd, ti, strTargetPath, iSlider, &aiParams[ 0 ] );
				bHandled = TRUE;
				break;
			}

			default:
				break;
			}

			// Post?
			PostMessage( hWnd, WM_USER_STOP, JUST_UPDATE_STATUS, STOPF_NO_INVALIDATE );

			return (LRESULT) bHandled;
			break;
		}
		
		case WM_COMMAND:
		{
			const UINT id = LOWORD( wParam );
			const UINT eventCode = HIWORD( wParam );

			// Buttons : may get various event notifications
			if( id == IDM_LIST || id == IDM_STOP || id == IDM_SETTINGS || id == IDM_EXIT )
			{
				if( eventCode == 0 && lParam == 0 /*Menu*/
					||
					eventCode == 1 && lParam == 0 /*Accelerator*/
					||
					eventCode == BN_CLICKED	)
				{
					; // ok
				}
				else break;
			}
			
			switch( id )
			{
				case IDM_LIST:
					/*g_hListDlg = CreateDialogParam(
									g_hInst,
									MAKEINTRESOURCE(IDD_LIST), 
									hWnd, 
									&xList, 
									(LPARAM) hp );*/

					DialogBoxParam( g_hInst, MAKEINTRESOURCE(IDD_LIST), hWnd, &xList, (LPARAM) ti );
					break;

				case IDM_WATCH:
					SelectWatch( hWnd, ti );
					break;

				case IDM_UNWATCH:
					WriteDebugLog( TEXT( "IDM_UNWATCH" ) );
					if( Unwatch( ti ) )
						EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
					break;

				case IDM_SHOWWATCHLIST: // 2017-11-03
					ShowWatchList( hWnd );
					break;

				case IDM_STOP:
				case IDM_STOP_FROM_TRAY:
				{
					WriteDebugLog( TEXT( "IDM_STOP" ) );
					
					if( g_bHack[ 3 ] ) Unwatch( ti );

					bool fStopIt[ MAX_SLOTS ];
					ptrdiff_t slotid = 0;
					for( ; slotid < MAX_SLOTS; ++slotid )
					{
						if( slotid != 3 && g_bHack[ slotid ] )
						{
							fStopIt[ slotid ] = true;
							g_bHack[ slotid ] = FALSE;
						}
						else fStopIt[ slotid ] = false;
					}

					for( slotid = 0; slotid < MAX_SLOTS; ++slotid )
					{
						if( fStopIt[ slotid ] ) {
							SendMessage( hWnd, WM_USER_STOP, (WPARAM) slotid, 0 );
							//ti[ slotid ].fUnlimited = true;  // flag not (yet) used: use ! g_bHack[ slotid ] instead
						}
					}
					break;
				}
				
				case IDM_SETTINGS:
					DialogBoxParam( g_hInst, MAKEINTRESOURCE( IDD_SETTINGS ), hWnd, 
									&Settings, (LPARAM) &ti[ 0 ] );
					UpdateStatus( hWnd );
		 			break;

				case IDM_REALTIME:
					g_fRealTime = ! g_fRealTime;
					SetRealTimeMode( hWnd, g_fRealTime );
					break;

				case IDM_ALLOWMORETHAN99:
				{
					HMENU hMenu = GetMenu( hWnd );
					
					// current state
					bool fAllowMoreThan99 = IsMenuChecked( hMenu, IDM_ALLOWMORETHAN99 );
					
					// new state
					fAllowMoreThan99 = ! fAllowMoreThan99;

					CheckMenuItemB( hMenu, IDM_ALLOWMORETHAN99, fAllowMoreThan99 );

					bool fUpdated = false;
					if( ! fAllowMoreThan99 )
					{
						for( int i = 0; i < 3; ++ i )
						{
							if( 99 < g_Slider[ i ] )
							{
								g_Slider[ i ] = 99;
								fUpdated = true;
							}
						}
					}
					
					WritePrivateProfileString( TEXT("Options"), TEXT("AllowMoreThan99"),
						fAllowMoreThan99 ? TEXT("1") : TEXT("0"), GetIniPath() );

					if( fUpdated )
						UpdateStatus( hWnd );
					break;
				}

				case IDM_LOGGING:
				{
					HMENU hMenu = GetMenu( hWnd );
					const UINT uMenuState = GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND );
					if( uMenuState == (UINT) -1 )
						break;

					if( uMenuState & MFS_CHECKED ) // currently enabled
					{
						if( MessageBox( hWnd, 
										TEXT("Disabling logging."), 
										APP_NAME, MB_ICONINFORMATION | MB_OKCANCEL ) == IDOK )
						{
							CheckMenuItem( hMenu, IDM_LOGGING, MF_BYCOMMAND | MFS_UNCHECKED );
							EnableLoggingIni( FALSE );
							if( g_bLogging )
							{
								CloseDebugLog();
								g_bLogging = FALSE;
							}
						}
					}
					else // currently disabled
					{
						if( MessageBox( hWnd, 
										TEXT("Enabling logging."), 
										APP_NAME, MB_ICONINFORMATION | MB_OKCANCEL ) == IDOK )
						{
							CheckMenuItem( hMenu, IDM_LOGGING, MF_BYCOMMAND | MFS_CHECKED );
							EnableLoggingIni( TRUE );
							if( ! g_bLogging )
							{
								g_bLogging = TRUE;
								OpenDebugLog();
							}
						}
					}
					break;
				}

				case IDM_ALLOWMULTI:
					toggle_opt_menu_item( hWnd, id, _T("AllowMulti"), 1 );
					break;
				case IDM_ALWAYS_LISTALL:
					toggle_opt_menu_item( hWnd, id, _T("ListAll"), 2 );
					break;

				case IDM_LOWER_PRIVILEGE:
					toggle_opt_menu_item( hWnd, id, _T("LowerPriv"), 1 );
					break;

#if 0
				case IDM_WATCH_MULTI:
					_toggle_opt_menu_item( hWnd, id, _T("WatchMulti") );
					break;
#endif

				case IDM_DISABLE_F1:
					toggle_opt_menu_item( hWnd, id, _T("DisableF1"), 1 );
					break;

				case IDM_WATCH_RT8:
				case IDM_WATCH_RT4:
				case IDM_WATCH_RT2:
					CheckMenuRadioItem( GetMenu( hWnd ), IDM_WATCH_RT8, IDM_WATCH_RT2,
										id,	MF_BYCOMMAND );
					
					WritePrivateProfileString(
						TEXT( "Options" ), 
						TEXT( "WatchRT" ),
						id == IDM_WATCH_RT2 ? TEXT("20") : id == IDM_WATCH_RT4 ? TEXT("40") : TEXT("80"),
						GetIniPath() );
					break;

				case IDM_SHOWWINDOW:
					if( ! g_fHide )
					{
						ShowWindow( hWnd, SW_RESTORE );
						SetForegroundWindow( hWnd );
					}
					break;

				case IDM_ABOUT:
					DialogBoxParam( g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, &About, (LPARAM) hWnd );
		 			break;
				
				case IDM_HOMEPAGE:
					OpenBrowser( APP_HOME_URL );
					break;
				
				case IDM_ABOUT_SHORTCUTS:
					AboutShortcuts( hWnd );
					break;

				case IDM_HELP_CMDLINE:
					ShowCommandLineHelp( hWnd );
					break;

				case IDM_SKIN:
					ChangeSkin( hWnd, hMemDC, SkinSize, hOrigBmp );
					break;

				case IDM_SNAP:
					SaveSnap( hWnd );
					break;

				case IDM_EXIT_FROM_TRAY:
					if( IsActive() )
						SendMessage( hWnd, WM_COMMAND, IDM_STOP, 0 );

					Exit_CommandFromTaskbar( hWnd );
				    break;
				
				case IDM_EXIT:
					SendMessage( hWnd, WM_CLOSE, 0, 0 );
				    break;
				
				case IDM_EXIT_ANYWAY:
					ShowWindow( hWnd, SW_HIDE );
					SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, 0 );
					Sleep( 2000 );
					SendMessage( hWnd, WM_CLOSE, 0, 0 );
					break;

				case IDM_GSHOW:
					GShow( hWnd );
					break;

				case IDM_MINIMIZE: // VK_M: undocumented
				{
					if( ! g_fHide )
					{
						if( g_hListDlg )
							SendMessage( g_hListDlg, WM_COMMAND, IDCANCEL, 0 );
					
						if( g_hSettingsDlg )
							SendMessage( g_hSettingsDlg, WM_COMMAND, IDCANCEL, 0 );
						
						if( g_hAboutDlg )
							SendMessage( g_hAboutDlg, WM_COMMAND, IDCANCEL, 0 );

						ShowWindow( hWnd, SW_SHOWMINIMIZED );
					}
					break;
				}

#ifdef _UNICODE
				case IDM_ENGLISH:
				{
					SetLanguage( LANG_ENGLISH );

					InitToolTipsEng( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuEng( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_FRENCH:
				{
					SetLanguage( LANG_FRENCH );

					InitToolTipsFre( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuFre( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_FINNISH:
				{
					SetLanguage( LANG_FINNISH );

					InitToolTipsFin( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuFin( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_SPANISH:
				{
					SetLanguage( LANG_SPANISH );

					InitToolTipsSpa( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuSpa( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_CHINESE_T:
				{
					SetLanguage( LANG_CHINESE_T );
					
					InitToolTipsChiT( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuChiT( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_CHINESE_S:
				{
					SetLanguage( LANG_CHINESE_S );

					InitToolTipsChiS( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuChiS( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_JAPANESEo:
				{
					SetLanguage( LANG_JAPANESEo );

					InitToolTipsJpn( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuJpn( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
				case IDM_JAPANESE:
				{
					SetLanguage( LANG_JAPANESE );

					InitToolTipsJpn( strToolTips );
					for( int i = 0; i < 4; i++ )
					{
						UpdateTooltip( g_hInst, hButton[ i ], strToolTips[ i ], hToolTip[ i ] );
					}
					InitMenuJpn( hWnd );
					CheckLanguageMenuRadio( hWnd );
					InvalidateRect( hWnd, (LPRECT) NULL, 0 );
					break;
				}
#endif // _UNICODE
				default:
					break;
			}
			break;
		}

		case WM_MOUSEMOVE:
		{
			if( GetFocus() != hWnd ) break;
			if( iMouseDown == -1 ) break;

			int iPrevCursor = iCursor;
			iCursor = MainWindowUrlHit( lParam, urlrect );
			if( iCursor == 1 && iMouseDown == 1 ) iCursor = 2;

/*
			// This is actually enough:
			if( iPrevCursor != iCursor )
			{
				SetCursor( hCursor[ iCursor ] );
				InvalidateRect( hWnd, &urlrect, TRUE );
			}
*/
			// But we do this just in case:
			if( iCursor != 0 ) SetCursor( hCursor[ 1 ] );

			if( iPrevCursor != iCursor )
			{
				if( iCursor == 0 ) SetCursor( hCursor[ 0 ] );
				InvalidateRect( hWnd, &urlrect, 0 );
			}

			break;
		}

		case WM_LBUTTONDOWN:
		{
			SetCapture( hWnd );
			
			if( MainWindowUrlHit( lParam, urlrect ) )
			{
				iMouseDown = 1;
				SetCursor( hCursor[ 1 ] );
				iCursor = 2;
				InvalidateRect( hWnd, &urlrect, 0 );
			}
			else
			{
				iMouseDown = -1;
			}

			break;
		}

		case WM_LBUTTONUP:
		{
			ReleaseCapture();
			
			if( iMouseDown == 1 && MainWindowUrlHit( lParam, urlrect ) )
			{
				OpenBrowser( APP_HOME_URL );
			}
			iCursor = 0;
			SetCursor( hCursor[ 0 ] );
			iMouseDown = 0;

			break;
		}

		case WM_LBUTTONDBLCLK:
		{
			if( MainWindowUrlHit( lParam, urlrect ) ) break;
			
			POINT pt = {0};
			GetCursorPos( &pt );
			ScreenToClient( hWnd, &pt );
			if( ChildWindowFromPoint( hWnd, pt ) != hWnd ) break;

			SendMessage( hWnd, WM_COMMAND, IDM_SKIN, 0 );
			break;
		}

		case WM_RBUTTONDBLCLK:
		{
			SendMessage( hWnd, WM_COMMAND, IDM_GSHOW, 0 );
			break;
		}

		case WM_SETFOCUS:
		{
			if( g_hListDlg ) SetFocus( g_hListDlg );
			else if( g_hSettingsDlg ) SetFocus( g_hSettingsDlg );
			break;
		}
		
		case WM_CONTEXTMENU:
		{
			POINT ptScreen = {0};
			GetCursorPos( &ptScreen );

			POINT ptClient;
			ptClient = ptScreen;
			ScreenToClient( hWnd, &ptClient );

			RECT rcClient = {0};
			GetClientRect( hWnd, &rcClient );

			UINT flags = (UINT)( TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN );

#ifdef _WIN64
#define UInt32_(l) ((UINT32)(((UINT64)(l)) & 0xffffFFFFui64))
#else
#define UInt32_(l) ((UINT32)(l))
#endif
			//const bool fMouseClicked = !( GET_X_LPARAM(lParam) == -1 && GET_Y_LPARAM(lParam) == -1 );
			if( UInt32_(lParam) ^ 0xffffFFFFu ) // fMouseClicked
			{
				// non-client (e.g. titlebar)
				if( ! PtInRect( &rcClient, ptClient ) )
					return DefWindowProc( hWnd, uMessage, wParam, lParam );

				if( ChildWindowFromPoint( hWnd, ptClient ) != hWnd
					|| MainWindowUrlHit( MAKELPARAM( ptClient.x, ptClient.y ), urlrect ) )
					break;
			}
			else // VK_APPS or Shift+F10
			{
				ptScreen.x = rcClient.right / 2L;
				ptScreen.y = rcClient.bottom / 2L;
				ClientToScreen( hWnd, &ptScreen );
				flags = (UINT)( TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_CENTERALIGN | TPM_VCENTERALIGN );
			}
			
			HMENU hMenu = CreatePopupMenu();
			if( hMenu )
			{
				AppendMenu( hMenu, MF_STRING, IDM_MINIMIZE, TEXT( "&Boss (Minimize)" ) );
				AppendMenu( hMenu, MF_SEPARATOR, 0u, NULL );
				AppendMenu( hMenu, MF_STRING, IDM_SKIN, TEXT( "Ski&n..." ) );
				if( ! hMemDC ) EnableMenuItem( hMenu, IDM_SKIN, MF_BYCOMMAND | MF_GRAYED );
				TrackPopupMenuEx( hMenu, flags, (int) ptScreen.x, (int) ptScreen.y, hWnd, NULL );
				DestroyMenu( hMenu );
			}
			
			break;
		}

		case WM_KILLFOCUS:
		{
			iCursor = 0;
			SetCursor( hCursor[ 0 ] );
			InvalidateRect( hWnd, &urlrect, FALSE );
			break;
		}

		case WM_PAINT:
		{
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint( hWnd, &ps );
			const int iSaved = SaveDC( hdc );
			SetTextAlign( hdc, TA_TOP | TA_LEFT );

			DrawSkin( hdc, hMemDC, SkinSize );

			HFONT hOldFont = NULL;
			
			if( _tcscmp( s_szStatus[ 0 ], APP_LONGNAME ) == 0 )
			{
				hOldFont = SelectFont( hdc, hFontItalic );
#ifdef _UNICODE
				_tcscpy_s( s_szStatus[ 2 ], cchStatus, _T( "Unicode Build / " ) );

				if( IS_JAPANESE )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE,
						IS_JAPANESEo ? S_JPNo_0001 : S_JPN_0001,
						-1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else if( IS_FINNISH )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_0001, -1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else if( IS_SPANISH )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_0001, -1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else if( IS_CHINESE_T )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_0001T, -1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else if( IS_CHINESE_S )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_0001S, -1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else if( IS_FRENCH )
				{
					WCHAR str[ cchStatus ] = L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_0001, -1, str, cchStatus );
					_tcscat_s( s_szStatus[ 2 ], cchStatus, str );
				}
				else
				{
					_tcscat_s( s_szStatus[ 2 ], cchStatus, L"English" );
				}
#else
				strcpy_s( s_szStatus[ 2 ], cchStatus, _T( "Non-Unicode Build" ) );
#endif
			}
			else
			{
				hOldFont = SelectFont( hdc, g_hFont );
			}
			
			SetTextColor( hdc, RGB( 0x99, 0xff, 0x66 ) );
			SetBkMode( hdc, TRANSPARENT );
			int x = 30;
			int y = 20;

			TextOut( hdc, x, y, s_szStatus[ 0 ], (int) _tcslen( s_szStatus[ 0 ] ) );
			y += 20;

			SelectFont( hdc, g_hFont );

			for( int i = 1; i < 12; i++ )
			{
				if( i % 4 == 0 ) SetTextColor( hdc, RGB( 0x99, 0xff, 0x66 ) );
				else if( i % 4 == 3 ) SetTextColor( hdc, RGB( 0x66, 0xee, 0xee ) );
				else SetTextColor( hdc, RGB( 0xff, 0xff, 0x99 ) );
				TextOut( hdc, x, y, s_szStatus[ i ], (int) _tcslen( s_szStatus[ i ] ) );
				y += 20;
				if( i % 4 == 3 ) y += 10;
			}
			
			RECT rcLimStatus;
			SetRect( &rcLimStatus, x + 20, y - 5, 530, y + 40 );
			SetTextColor( hdc, RGB( 0xe0, 0xf0, 0x30 ) );
			DrawText( hdc, s_szLimitingStatus, (int) _tcslen( s_szLimitingStatus ), &rcLimStatus,
						DT_END_ELLIPSIS | DT_LEFT | DT_NOCLIP | DT_TOP | DT_WORDBREAK );

			/*else
			{
				TextOut( hdc, x + 20, y, s_szStatus[ 12 ], (int) _tcslen( s_szStatus[ 12 ] ) );
			}*/
			
			SetTextColor( hdc, RGB( 0xff, 0xff, 0xcc ) );
			x += 40;
			y += 35 + 20;
			for( ptrdiff_t sx = 13; sx < 18; ++sx, y += 20 )
				TextOut( hdc, x, y, s_szStatus[ sx ], (int) _tcslen( s_szStatus[ sx ] ) );

			HFONT hFontURL = GetFontForURL( hdc, 10.0 );
			SelectFont( hdc, hFontURL );

			SetTextColor( hdc, iCursor == 0? RGB( 0x66, 0x99, 0x99 ) :
				iCursor == 1? RGB( 0x66, 0xff, 0xff ) : RGB( 0xff, 0xcc, 0x00 ) );

			TextOut( hdc, urlrect.left, urlrect.top, APP_HOME_URL, 28 );
			SIZE size;
			GetTextExtentPoint32( hdc, APP_HOME_URL, 28, &size );
			urlrect.right = urlrect.left + size.cx;
			urlrect.bottom = urlrect.top + size.cy;

			SelectFont( hdc, hOldFont );
			DeleteFont( hFontURL );

			RestoreDC( hdc, iSaved );
			EndPaint( hWnd, &ps );
			break;
		}

		case WM_SYSKEYDOWN:
#define FIRST_PRESSED( lParam ) ( ! ( ((ULONG_PTR)lParam) & 0xC0000000 ) )
#define KEY_CONTEXT_1( lParam ) (     (lParam) & 0x20000000L   )

			// [Alt]+[T] (&Target) handler: Valid unless the language is Finnish; if Finnish, ALT + T
			// is for the File menu ("&Tiedosto") by system def handling.
			// Note [T] without [Alt] is also a valid (single key) hotkey for &Target.
			if( wParam == _T('T')
				&& FIRST_PRESSED( lParam )
				&& KEY_CONTEXT_1( lParam )
				&& GetLanguage() != LANG_FINNISH )
			{
				SendMessage( hWnd, WM_COMMAND, MAKEWPARAM( IDM_LIST, 1 ), 0 );
				break;
			}

			return DefWindowProc( hWnd, uMessage, wParam, lParam );
			break;

		case WM_KEYDOWN:
		{
			if( wParam == VK_ESCAPE	&& ! REPEATED_KEYDOWN( lParam ) )
			{
				const UINT uMenuState = GetMenuState( GetMenu( hWnd ), IDM_EXIT, MF_BYCOMMAND );
				if( uMenuState == (UINT) -1 ) // error
					break;
				else if( uMenuState & MF_GRAYED ) // can't exit because it's active
					SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, 0 );
				else
					SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_EXIT, 0 );
			}
			else if( wParam == VK_F1 && ! REPEATED_KEYDOWN( lParam ) )
			{
				const UINT uMenuState = GetMenuState( GetMenu( hWnd ), IDM_DISABLE_F1, MF_BYCOMMAND );
				if( ! (uMenuState & MF_CHECKED) )
				{
					PostMessage( hWnd, WM_COMMAND, (WPARAM) IDM_ABOUT_SHORTCUTS, 0 );
				}
			}
			else if( wParam == VK_F2 && ! REPEATED_KEYDOWN( lParam ) )
			{
				PostMessage( hWnd, WM_COMMAND, (WPARAM) IDM_HELP_CMDLINE, 0 );
			}

			return DefWindowProc( hWnd, uMessage, wParam, lParam );
		}
		
		case WM_CLOSE:
		{
			WriteDebugLog( TEXT( "WM_CLOSE" ) );
			if( IsActive() )
			{
				WriteDebugLog( TEXT( "EXIT_ANYWAY?" ) );

				// Just in case
				if( ! g_fHide )
				{
					ShowWindow( hWnd, SW_SHOWDEFAULT );
					SetForegroundWindow( hWnd );
				}

				TCHAR msg[ 1024 ] = TEXT( "BES is active. Exiting now is risky.\r\n\r\nExit anyway?" );
#ifdef _UNICODE
				if( IS_JAPANESE )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_EXIT_ANYWAY, -1, msg, 1023 );
				}
				else if( IS_FINNISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_EXIT_ANYWAY, -1, msg, 1023 );
				}
				else if( IS_SPANISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_EXIT_ANYWAY, -1, msg, 1023 );
				}
				else if( IS_CHINESE_T )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_EXIT_ANYWAY_T, -1, msg, 1023 );
				}
				else if( IS_CHINESE_S )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_EXIT_ANYWAY_S, -1, msg, 1023 );
				}
				else if( IS_FRENCH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_EXIT_ANYWAY, -1, msg, 1023 );
				}
#endif
				if( IDNO == MessageBox( hWnd, 
					msg,
					APP_NAME, MB_ICONEXCLAMATION | MB_YESNO | MB_DEFBUTTON2 )
				)
				{
					break;
				}

				WriteDebugLog( TEXT( "User wants to exit anyway" ) );
			}

			SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, 0 );

			int nOK = 0;
			ptrdiff_t i = 0;
			for( ; i < MAX_SLOTS; ++ i )
			{
				DWORD dwResult = WaitForSingleObject( hSemaphore[ i ], 250 );
				if( dwResult == WAIT_OBJECT_0 ) ++nOK;
#ifdef _DEBUG
				else
				{
					TCHAR s[100];_stprintf_s(s,100,_T("\tSema bad #%d\n"),(int)i);
					OutputDebugString(s);
				}
#endif
			}
			
			if( nOK == MAX_SLOTS )
			{
				WriteDebugLog( TEXT( "[ All Semaphores OK ]" ) );
				for( i = 0; i < MAX_SLOTS; ++i )
					ReleaseSemaphore( hSemaphore[ i ], 1L, NULL );
			}
			else
			{
				WriteDebugLog( TEXT( "[!] Semaphore Error" ) );
			}

			DestroyWindow( hWnd );
			break;
		}

		case WM_DESTROY:
		{
			WriteDebugLog( TEXT( "WM_DESTROY" ) );

			if( hMemDC )
			{
				if( hOrigBmp )
				{
					HBITMAP hBmp = SelectBitmap( hMemDC, hOrigBmp );
					DeleteBitmap( hBmp );

#ifdef _DEBUG
TCHAR s[100];swprintf_s(s,100,L"\t&\tDeleting %p, Orig %p\n",hBmp,hOrigBmp);
OutputDebugString(s);
#endif

					hOrigBmp = NULL;
				}
				
				DeleteDC( hMemDC );
				hMemDC = NULL;
			}

			DeleteNotifyIcon( hWnd );
			DeleteFont( g_hFont );
			g_hFont = NULL;
			DeleteFont( hFontItalic );

			for( ptrdiff_t i = MAX_SLOTS - 1; 0 <= i; --i )
			{
				if( hChildThread[ i ] )
				{
					CloseHandle( hChildThread[ i ] );
					hChildThread[ i ] = NULL;
				}
				if( hSemaphore[ i ] )
				{
					CloseHandle( hSemaphore[ i ] );
					hSemaphore[ i ] = NULL;
				}

				if( ti[ i ].lpPath )
				{
					HeapFree( GetProcessHeap(), 0, ti[ i ].lpPath );
					ti[ i ].lpPath = NULL;
				}
				if( ti[ i ].disp_path )
				{
					HeapFree( GetProcessHeap(), 0, ti[ i ].disp_path );
					ti[ i ].disp_path = NULL;
				}
				//if( ti[ i ].lpExe )
				//{
				//	HeapFree( GetProcessHeap(), 0, ti[ i ].lpExe );
				//	ti[ i ].lpExe = NULL;
				//}
			}

			PostQuitMessage( 0 );
			break;
		}
		
		default:
			if( uMessage == uMsgTaskbarCreated && uMessage != 0 )
			{
				SendNotifyIconData( hWnd, ti, NIM_ADD );
			}
			else
			{
				return DefWindowProc( hWnd, uMessage, wParam, lParam );
			}
			break;
   }
   return 0;
}

static void SetRealTimeMode( HWND hWnd, bool fRealTime )
{
	g_fRealTime = fRealTime;
	if( fRealTime )
	{
		SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
//		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
		CheckMenuItem( GetMenu( hWnd ), IDM_REALTIME, MF_BYCOMMAND | MFS_CHECKED );
		WriteDebugLog( TEXT( "Real-time mode: Yes" ) );
	}
	else
	{
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
//		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
		CheckMenuItem( GetMenu( hWnd ), IDM_REALTIME, MF_BYCOMMAND | MFS_UNCHECKED );
		WriteDebugLog( TEXT( "Real-time mode: No" ) );
	}

	TCHAR szWindowText[ MAX_WINDOWTEXT ] = _T("");
	_stprintf_s( szWindowText, _countof(szWindowText), _T("%s - %s%s"),
					APP_LONGNAME,
					IsActive() ? _T("Active") : _T("Idle"),
					fRealTime ? _T(" (Real-time mode)") : _T("") );
	SetWindowText( hWnd, szWindowText );
}


static HFONT _create_font( LPCTSTR lpszFace, int iSize, BOOL bBold, BOOL bItalic )
{
	return CreateFont( iSize,
				0, 0, 0,
				bBold ? FW_BOLD : FW_NORMAL,
				(DWORD) bItalic,
				0UL, 0UL,
				DEFAULT_CHARSET,
				OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				FF_SWISS | VARIABLE_PITCH,
				lpszFace );
}


static bool toggle_opt_menu_item( HWND hWnd, UINT uMenuId, const TCHAR * pIniOptionName, int nIfEnabled )
{
	HMENU hMenu = GetMenu( hWnd );
	UINT uMenuState = GetMenuState( hMenu, uMenuId, MF_BYCOMMAND );
	const bool fEnable = !( uMenuState & MFS_CHECKED ); // toggle

	TCHAR szValue[ 10 ];
	_stprintf_s( szValue, _countof(szValue), _T("%d"), fEnable ? nIfEnabled : 0 );
	WritePrivateProfileString(
		TEXT( "Options" ), 
		pIniOptionName,
		szValue,
		GetIniPath() );
	
	CheckMenuItemB( hMenu, uMenuId, fEnable );

	return fEnable;
}

bool IsActive( void )
{
/*	return ( g_bHack[ 0 ]
			|| g_bHack[ 1 ]
			|| g_bHack[ 2 ] && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE );
*/
	for( ptrdiff_t i = 0; i < MAX_SLOTS; ++i )
	{
		if( i != 3 && g_bHack[ i ] ) return true;
	}
	return false;
}
static void EnableLoggingIni( BOOL bEnabled )
{
	WritePrivateProfileString( TEXT("Options"), TEXT("Logging"),
		bEnabled ? TEXT("1") : TEXT("0"), GetIniPath() );
}

static void CheckMenuItemB( HMENU hMenu, UINT uMenuId, bool fCheck )
{
	UINT uMenuFlags = (UINT)( MF_BYCOMMAND | (fCheck ? MFS_CHECKED : MFS_UNCHECKED) );
	CheckMenuItem( hMenu, uMenuId, uMenuFlags );
}

static bool CheckMenuItemI( HMENU hMenu, UINT uMenuId, const TCHAR * strKey, const TCHAR * strIniPath )
{
	bool fCheck = !! GetPrivateProfileInt( TEXT("Options"), strKey, 0, strIniPath );
	CheckMenuItemB( hMenu, uMenuId, fCheck );
	return fCheck;
}
