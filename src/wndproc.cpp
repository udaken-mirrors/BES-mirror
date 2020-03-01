#include "BattleEnc.h"

extern bool g_fRealTime;
extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hSettingsDlg;
extern HWND g_hListDlg;

BOOL g_bLogging = FALSE;
volatile int g_Slider[ 4 ];

HANDLE hSemaphore[ 4 ];
volatile BOOL g_bHack[ 4 ];

volatile DWORD g_dwTargetProcessId[ 4 ];
TCHAR g_szTarget[ 3 ][ CCHPATHEX ];

HFONT g_hFont = NULL;

HBITMAP LoadSkin( HWND hWnd, HDC& hMemDC, SIZE& PicSize );
BOOL ChangeSkin( HWND hWnd, HDC hMemDC, SIZE& PicSize, HBITMAP& hOrigBmp );
BOOL DrawSkin( HDC hdc, HDC hMemDC, SIZE& SkinSize );
HFONT GetFontForURL( HDC hdc );
static inline HFONT _create_font( LPCTSTR lpszFace, int iSize, BOOL bBold, BOOL bItalic );
static bool _toggle_opt_menu_item( HWND hWnd, UINT uMenuId, const TCHAR * pIniOptionName );

static bool IsActive( void )
{
	return ( g_bHack[ 0 ]
			|| g_bHack[ 1 ]
			|| g_bHack[ 2 ] && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE );
}

void ShowCommandLineHelp( HWND hWnd );
static int MainWindowUrlHit( LPARAM lParam );

LRESULT CALLBACK WndProc( HWND hWnd, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	const static RECT urlrect = { 420L, 390L, 620L, 420L };
	static HCURSOR hCursor[ 3 ];
	static int iCursor = 0;
	static int iMouseDown = 0;
	static HANDLE hChildThread[ 4 ] = { NULL, NULL, NULL, NULL };
	static HACK_PARAMS hp[ 3 ];
	static TARGETINFO ti[ 3 ];
	static HFONT hFontItalic;
	static HWND hButton[ 4 ];
	static HWND hToolTip[ 4 ];
	static TCHAR lpszStatus[ 16 ][ cchStatus ];
	static TCHAR strToolTips[ 4 ][ 256 ];
	static TCHAR lpszWindowText[ MAX_WINDOWTEXT ] = { _T('\0') };

	static HDC hMemDC = NULL;
	static HBITMAP hOrigBmp = NULL;
	static SIZE SkinSize = { 640L, 480L };

	static UINT uMsgTaskbarCreated;
	
	switch ( uMessage ) 
	{
		case WM_CREATE:
		{
			g_hWnd = hWnd;
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

			hOrigBmp = LoadSkin( hWnd, hMemDC, SkinSize );

			HMENU hMenu = GetMenu( hWnd );
			CheckMenuItem( hMenu, IDM_REALTIME,
							(UINT) MF_BYCOMMAND | ( g_fRealTime ? MF_CHECKED : MF_UNCHECKED ) );

			DeleteMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND );

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

			/*EnableMenuItem( hMenu, 
				IDM_DEBUGPRIVILEGE, 
				MF_BYCOMMAND | MF_GRAYED | MF_DISABLED );*/

#endif
			CheckLanguageMenuRadio( hWnd );
			CheckMenuItem( hMenu, IDM_LOGGING,
							(UINT) MF_BYCOMMAND | ( g_bLogging? MFS_CHECKED : MFS_UNCHECKED ) );

			const TCHAR * strIniPath = GetIniPath();
#if 0//def _UNICODE
			const bool fDebugPriv = !! GetPrivateProfileInt( TEXT( "Options" ), 
				TEXT( "DebugPrivilege" ), 0, strIniPath );
			CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, 
				(UINT)( fDebugPriv ? ( MF_BYCOMMAND | MFS_CHECKED )
				: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
#endif
			const bool fAllowMoreThan99 = !! GetPrivateProfileInt( TEXT( "Options" ), 
												TEXT( "AllowMoreThan99" ), 0, strIniPath );
			CheckMenuItem( hMenu, IDM_ALLOWMORETHAN99, 
				(UINT)( fAllowMoreThan99 ? ( MF_BYCOMMAND | MFS_CHECKED )
				: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
			
			const bool fAllowMulti = !! GetPrivateProfileInt( TEXT( "Options" ), 
					TEXT( "AllowMulti" ), FALSE, strIniPath );
			CheckMenuItem( hMenu, IDM_ALLOWMULTI, 
					(UINT)( fAllowMulti ? ( MF_BYCOMMAND | MFS_CHECKED )
					: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
			const bool fListAll = !! GetPrivateProfileInt( TEXT( "Options" ), 
					TEXT( "ListAll" ), FALSE, strIniPath );
			CheckMenuItem( hMenu, IDM_ALWAYS_LISTALL, 
					(UINT)( fListAll ? ( MF_BYCOMMAND | MFS_CHECKED )
					: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
			const bool fDisableF1 = !! GetPrivateProfileInt( TEXT( "Options" ), 
					TEXT( "DisableF1" ), FALSE, strIniPath );
			CheckMenuItem( hMenu, IDM_DISABLE_F1, 
					(UINT)( fDisableF1 ? ( MF_BYCOMMAND | MFS_CHECKED )
					: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );

			const UINT wrt = GetPrivateProfileInt( _T("Options"), _T("WatchRT"), 80, strIniPath );
			CheckMenuRadioItem( GetMenu( hWnd ), IDM_WATCH_RT8, IDM_WATCH_RT2,
								(UINT)( wrt == 20 ? IDM_WATCH_RT2
										: wrt == 40 ? IDM_WATCH_RT4
										: IDM_WATCH_RT8 ),
								MF_BYCOMMAND );

			g_hFont = _create_font( TEXT( "Tahoma" ), 17, TRUE, FALSE );

			HDC hDC = GetDC( hWnd );
			hFontItalic = MyCreateFont( hDC, TEXT( "Georgia" ), 13, FALSE, TRUE );
			ReleaseDC( hWnd, hDC );
			
			hButton[ 0 ] = CreateWindow( TEXT( "BUTTON" ), TEXT( "&Target..." ), 
				WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON //***1.4.0
				//| BS_NOTIFY
				,
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
			
			ptrdiff_t i  = 0;
			for( ; i < 3; ++i )
			{
				_tcscpy_s( g_szTarget[ i ], CCHPATHEX, TARGET_UNDEF );
				hp[ i ].iMyId = (int) i;

				_tcscpy_s( ti[ i ].szPath, CCHPATH, TARGET_UNDEF );
				_tcscpy_s( ti[ i ].szExe, CCHPATH, TARGET_UNDEF );

				hp[ i ].lpTarget = &ti[ i ];
				for( ptrdiff_t j = 0; j < 16; ++j )
					hp[ i ].lpszStatus[ j ] = lpszStatus[ j ];
			}

			for( i = 0; i < 4; ++i )
			{
				g_bHack[ i ] = FALSE;
				hSemaphore[ i ] = CreateSemaphore( NULL, 1L, 1L, NULL );
				hToolTip[ i ] = CreateTooltip( g_hInst, hButton[ i ], strToolTips[ i ] );
				SendMessage( hButton[ i ], WM_SETFONT, (WPARAM) hFontItalic, 0L );
			}
			_tcscpy_s( lpszStatus[ 0 ], cchStatus, APP_LONGNAME );
			_tcscpy_s( lpszStatus[ 1 ], cchStatus, _T( "Hit [a] for more info." ) );

			uMsgTaskbarCreated = RegisterWindowMessage( TEXT( "TaskbarCreated" ) );

			TCHAR lpszTargetPath[ CCHPATH ];
			TCHAR lpszTargetExe[ CCHPATH ];
			TCHAR strOptions[ CCHPATH ];

			int iSlider = ParseArgs( GetCommandLine(), CCHPATH,
											lpszTargetPath, lpszTargetExe, strOptions );

			if( IsOptionSet( strOptions, _T("--unlimit"), _T("-u") ) )
			{
				iSlider = IGNORE_ARGV;
			}
			else if( IsOptionSet( strOptions, _T("--watch-multi"), NULL ) )
			{
				// Just created menu is not checked; toggle = check it & update ini
				_toggle_opt_menu_item( hWnd, IDM_WATCH_MULTI, _T("WatchMulti") );
			}
			else
			{
				const bool fWatchMulti = !! GetPrivateProfileInt( _T( "Options" ),
												_T( "WatchMulti" ), FALSE, strIniPath );

				CheckMenuItem( hMenu, IDM_WATCH_MULTI, 
						(UINT)( fWatchMulti ? ( MF_BYCOMMAND | MFS_CHECKED )
						: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
			}
			
			if( iSlider != IGNORE_ARGV && *lpszTargetPath )
			{
				if( 99 < iSlider && ! fAllowMoreThan99 )
					iSlider = 99;

				//WriteDebugLog( TEXT( "Runing from the command line..." ) );
				const DWORD dwTargetProcess = PathToProcessID( lpszTargetPath );
				if( IsProcessBES( dwTargetProcess ) )
				{
					WriteDebugLog( TEXT( "BES can't target itself!" ) );
				}
				else
				{
					hp[ 2 ].lpTarget->dwProcessId = TARGET_PID_NOT_SET; // just in case
					hp[ 2 ].lpTarget->fWatch = true;

					if( iSlider >= SLIDER_MIN && iSlider <= SLIDER_MAX )
					{
						SetSliderIni( lpszTargetPath, iSlider );
					}

					SetTargetPlus( hWnd, &hChildThread[ 3 ], &hp[ 2 ], lpszTargetPath, lpszTargetExe );
				}
			}

			NOTIFYICONDATA ni;
			DWORD iDllVersion = InitNotifyIconData( hWnd, &ni, &ti[ 0 ] );
			Shell_NotifyIcon( NIM_ADD, &ni );
			
			if( iDllVersion >= 5 )
			{
				ni.uVersion = NOTIFYICON_VERSION;
				Shell_NotifyIcon( NIM_SETVERSION, &ni );
			}
			
			hCursor[ 0 ] = LoadCursor( (HINSTANCE) NULL, IDC_ARROW );
			hCursor[ 2 ] = hCursor[ 1 ] = LoadCursor( (HINSTANCE) NULL, IDC_HAND );
			SetCursor( hCursor[ 0 ] );

			SetParent( hWnd, NULL );
			
			/*if( ! IsOptionSet( strOptions, _T("--minimize"), _T("-m") ) )
			{
				PostMessage( hWnd, WM_USER_CAPTION, 0, 1 );
			}*/
			break;
		}
		
		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			if( wParam == 0 )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hWnd, strCurrentText, (int) _countof(strCurrentText) );
				if( _tcscmp( strCurrentText, lpszWindowText ) != 0 )
					SendMessage( hWnd, WM_SETTEXT, 0, (LPARAM) lpszWindowText );
			}
			
			if( uMessage == WM_USER_CAPTION )
			{
				break;
			}

			return DefWindowProc( hWnd, uMessage, wParam, lParam );
			break;
		}

		case WM_NCUAHDRAWCAPTION:
		{
			TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
			GetWindowText( hWnd, strCurrentText, (int) _countof(strCurrentText) );
			if( _tcscmp( strCurrentText, lpszWindowText ) != 0 )
				SendMessage( hWnd, WM_SETTEXT, 0, (LPARAM) lpszWindowText );
			return DefWindowProc( hWnd, uMessage, wParam, lParam );
			break;
		}

		case WM_INITMENUPOPUP:
		{
			if( LOWORD(lParam) != 2 || HIWORD(lParam) ) break;
			
			HMENU hMenu = GetMenu( hWnd );
			if( wParam == (WPARAM) GetSubMenu( hMenu, 2 )  )
			{
				/*EnableMenuItem( hMenu, IDM_ALLOWMORETHAN99, 
					(UINT)( ( g_bHack[0] || g_bHack[1] || g_bHack[2] || g_bHack[3] )
					? ( MF_GRAYED | MF_BYCOMMAND )
					: ( MF_ENABLED | MF_BYCOMMAND ) ) );*/
			
				const TCHAR * strIniPath = GetIniPath();
				const bool fAllowMulti = !! GetPrivateProfileInt( TEXT( "Options" ), 
					TEXT( "AllowMulti" ), FALSE, strIniPath );
				CheckMenuItem( hMenu, IDM_ALLOWMULTI, 
					(UINT)( fAllowMulti ? ( MF_BYCOMMAND | MFS_CHECKED )
					: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
			}
			break;
		}

		case WM_SIZE:
		{
			if( wParam == SIZE_MINIMIZED )
			{
				ShowWindow( hWnd, SW_HIDE );
			}

			break;
		}

		case WM_MOVE:
		{
			if( ! IsIconic( hWnd ) )
			{
				SetWindowPosIni( hWnd );
			}
			break;				
		}

		case WM_USER_RESTART:
		{
			int id = (int) wParam;
			if( id >= 0 && id <= 2 )
			{
				if( WaitForSingleObject( hSemaphore[ id ], 200 ) == WAIT_OBJECT_0 )
				{
					SendMessage( hWnd, WM_USER_HACK, (WPARAM) id, (LPARAM) &hp[ id ] );
				}
				else
				{
					MessageBox( hWnd, TEXT( "Semaphore Error" ), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
					break;
				}
			}

			break;
		}

		case WM_USER_HACK:
		{
	//		DWORD dwChildThreadId;
			int iMyId = ( int ) wParam;
			if( ! ( iMyId >= 0 && iMyId < 3 ) ) break;

			LPTARGETINFO lpTarget = ( (LPHACK_PARAMS) lParam )->lpTarget;
			lpTarget->fRealTime = g_fRealTime;
			g_bHack[ iMyId ] = TRUE;
			/*SECURITY_ATTRIBUTES sa;
			sa.nLength = sizeof( SECURITY_ATTRIBUTES );
			sa.lpSecurityDescriptor = NULL;
			sa.bInheritHandle = FALSE;*/

			hChildThread[ iMyId ] = CreateThread2( &Hack, (void *)(LONG_PTR) lParam );

			if( hChildThread[ iMyId ] == NULL )
			{
				ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LPLONG) NULL );
				g_bHack[ iMyId ] = FALSE;
				MessageBox( hWnd, TEXT( "CreateThread failed." ), APP_NAME, MB_OK | MB_ICONSTOP );
				break;
			}
			else
			{
/*
				if( g_bRealTime )
				{
					SetThreadPriority( hChildThread[ iMyId ],
						THREAD_PRIORITY_TIME_CRITICAL );
				}
				else
				{
					SetThreadPriority( hChildThread[ iMyId ], THREAD_PRIORITY_NORMAL );
				}
*/

				SetThreadPriority( hChildThread[ iMyId ], THREAD_PRIORITY_TIME_CRITICAL );

				g_dwTargetProcessId[ iMyId ] = lpTarget->dwProcessId;
				_tcscpy_s( g_szTarget[ iMyId ], CCHPATHEX, lpTarget->szPath );

				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T( "%s - Active%s" ), APP_LONGNAME,
					g_fRealTime ? _T( " (Real-time mode)" ) : _T( "" ) );

				SetWindowText( hWnd, lpszWindowText );
				UpdateStatus( hWnd );
			}
			break;
		}

		case WM_USER_STOP:
		{
			TCHAR str[ 1024 ];
			_stprintf_s( str, _countof(str), _T( "WM_USER_STOP: wParam = 0x%04lX , lParam = 0x%04lX" ), wParam, (UINT) lParam );

			WriteDebugLog( str );

			int iMyId = (int) wParam;

			if( iMyId < 0 || iMyId > 3 ) break;

			if( iMyId != JUST_UPDATE_STATUS )
			{
				g_bHack[ iMyId ] = FALSE;
				DWORD dwExitCode = (DWORD) -1;
				const DWORD dwWaitResult = WaitForSingleObject( hSemaphore[ iMyId ], 1000UL );
				if( dwWaitResult == WAIT_OBJECT_0 )
				{
					for( int t = 0; t < 50; t++ )
					{
						GetExitCodeThread( hChildThread[ iMyId ], &dwExitCode );
						if( dwExitCode != STILL_ACTIVE )
						{
							break;
						}
						Sleep( 50UL );
					}

					_stprintf_s( str, _countof(str), _T( "ChildThread[ %d ] : ExitCode = 0x%04lX" ), iMyId, dwExitCode );
					WriteDebugLog( str );
					ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LPLONG) NULL );
					_stprintf_s( lpszStatus[ 0 + iMyId * 4 ], cchStatus,
						_T( "Target #%d %s" ),
						iMyId + 1,
						dwExitCode == NORMAL_TERMINATION ? _T( "OK" )	:
						dwExitCode == THREAD_NOT_OPENED ? _T( "Access denied" ) :
						dwExitCode == TARGET_MISSING ? _T( "Target missinig" ) :
						dwExitCode == STILL_ACTIVE ?  _T( "Time out" ) :
						dwExitCode == NOT_WATCHING ? _T( "Unwatch: OK" ) : _T( "Status unknown" )
					);

					if( hChildThread[ iMyId ] )
					{
						CloseHandle( hChildThread[ iMyId ] );
						hChildThread[ iMyId ] = NULL;
					}
				}
				else
				{
					WriteDebugLog( TEXT( "### Semaphore Error ###" ) );
					if( hChildThread[ iMyId ] )
						GetExitCodeThread( hChildThread[ iMyId ], &dwExitCode );
					_stprintf_s( str, _countof(str), _T( "ChildThread[ %d ] : ExitCode = 0x%04lX" ), iMyId, dwExitCode );
					WriteDebugLog( str );
					_stprintf_s( lpszStatus[ 0 + iMyId * 4 ], cchStatus, _T( "Target #%d Semaphore Error" ), iMyId + 1 );
				}

				EnableWindow( hButton[ 0 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_ENABLED );
			}
			
			if( IsActive() )
			{
				TCHAR strStatus[ 1024 ] = TEXT( " Limiting CPU load:" );
#ifdef _UNICODE
				if( IS_JAPANESE )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_2000, -1, strStatus, 1023 );
				}
				else if( IS_FINNISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_2000, -1, strStatus, 1023 );
				}
				else if( IS_SPANISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_2000, -1, strStatus, 1023 );
				}
				else if( IS_CHINESE_T )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000T, -1, strStatus, 1023 );
				}
				else if( IS_CHINESE_S )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_2000S, -1, strStatus, 1023 );
				}
				else if( IS_FRENCH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_2000, -1, strStatus, 1023 );
				}
#endif
				if( g_bHack[ 0 ] )
				{
					_tcscat_s( strStatus, _countof(strStatus), _T( " #1" ) );
				}
				
				if( g_bHack[ 1 ] )
				{
					_tcscat_s( strStatus, _countof(strStatus), _T( " #2" ) );
				}
				
				if( g_bHack[ 2 ] )
				{
					if( g_dwTargetProcessId[ 2 ] != WATCHING_IDLE )
					{
						_tcscat_s( strStatus, _countof(strStatus), _T( " #3" ) );
					}
				}
		
				_stprintf_s( lpszStatus[ 12 ], cchStatus, _T( "%.256s" ), strStatus );

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
			else
			{
				*lpszStatus[ 12 ] = _T('\0');
				if( g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
				{
					EnableWindow( hButton[ 1 ], TRUE );
					EnableMenuItem( GetMenu( hWnd ), IDM_STOP, MF_BYCOMMAND | MF_ENABLED );
				}
				else
				{
					EnableWindow( hButton[ 1 ], FALSE );
				}
				if( g_dwTargetProcessId[ 0 ] || g_dwTargetProcessId[ 1 ] || g_dwTargetProcessId[ 2 ] )
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

				_stprintf_s( lpszWindowText, _countof(lpszWindowText), _T( "%s - Idle%s" ), APP_LONGNAME,
					g_fRealTime ? _T( " (Real-time mode)" ) : _T( "" ) );
				SetWindowText( hWnd, lpszWindowText );
			}

			if( g_bHack[ 3 ] )
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
				EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_ENABLED );
			}
			else if( g_bHack[ 2 ] )
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
				EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
			}
			else
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED );
				EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
			}

			if( g_bHack[ 0 ] && g_bHack[ 1 ] && ( g_bHack[ 2 ] || g_bHack[ 3 ] ) )
			{
				EnableWindow( hButton[ 0 ], FALSE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_GRAYED );
			}
			else
			{
				EnableWindow( hButton[ 0 ], TRUE );
				EnableMenuItem( GetMenu( hWnd ), IDM_LIST, MF_BYCOMMAND | MF_ENABLED );
			}

			if( g_bHack[ 2 ] || g_bHack[ 3 ] )
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_GRAYED );
			}
			else
			{
				EnableMenuItem( GetMenu( hWnd ), IDM_WATCH, MF_BYCOMMAND | MF_ENABLED );
			}

			NOTIFYICONDATA ni;
			InitNotifyIconData( hWnd, &ni, &ti[ 0 ] );

///* XP won't work NIF_INFO here...
/*
			if( g_bHack[ 2 ] && g_bHack[ 3 ] && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE && iDllVersion >= 5 )
			{
				ni.uFlags = NIF_ICON | NIF_INFO | NIF_TIP;
			}
*/
			Shell_NotifyIcon( NIM_MODIFY, &ni );

			if( g_hSettingsDlg )
				SendMessageTimeout( g_hSettingsDlg, WM_USER_REFRESH, 0, 0,
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, NULL );
			
			if( g_hListDlg )
				SendMessageTimeout( g_hListDlg, WM_USER_REFRESH, 0, 0,
									SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, NULL );

			SetFocus( hWnd );//*** v1.3.8: needed if a just-disabled button was focused

			if( !( lParam & STOPF_NO_INVALIDATE ) )
				InvalidateRect( hWnd, NULL, FALSE );
			break;
		}

		case WM_USER_NOTIFYICON:
		{
			static BOOL bClicked = FALSE;
			if( lParam == WM_LBUTTONDOWN )
			{
				bClicked = TRUE;
			}
			else if( lParam == WM_LBUTTONUP || lParam == WM_LBUTTONDBLCLK || lParam == NIN_KEYSELECT || lParam == NIN_SELECT )
			{
				if( bClicked || lParam != WM_LBUTTONUP )
				{
					ShowWindow( hWnd, SW_RESTORE );
					SetForegroundWindow( hWnd );
				}
				bClicked = FALSE;
			}
			else if( lParam == WM_RBUTTONUP )
			{
				bClicked = FALSE;

				HMENU hMenu = CreatePopupMenu();
				POINT pt;
				/*
				if( GetShell32Version() >= 5 )
				{
					AppendMenu( hMenu, MFT_STRING, IDM_TRAY_STATUS, TEXT( "&Status" ) );
					AppendMenu( hMenu, MFT_SEPARATOR, 0U, NULL );
				}*/

				TCHAR str[ 3 ][ 256 ];
#ifdef _UNICODE 
				if( IS_JAPANESE )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_26, -1, str[ 2 ], 255 );
				}
				else if( IS_FINNISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_26, -1, str[ 2 ], 255 );
				}
				else if( IS_SPANISH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_26, -1, str[ 2 ], 255 );
				}
				else if( IS_CHINESE_T )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_26, -1, str[ 2 ], 255 );
				}
				else if( IS_CHINESE_S )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_26, -1, str[ 2 ], 255 );
				}
				else if( IS_FRENCH )
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_26, -1, str[ 2 ], 255 );
				}
				else
				{
					MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_24, -1, str[ 0 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_5, -1, str[ 1 ], 255 );
					MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_26, -1, str[ 2 ], 255 );
				}
#else
				strcpy_s( str[ 0 ], 256, _T( "&Restore BES" ) );
				strcpy_s( str[ 1 ], 256, _T( "&Unlimit all" ) );
				strcpy_s( str[ 2 ], 256, _T( "E&xit" ) );
#endif
				AppendMenu( hMenu, MFT_STRING, IDM_SHOWWINDOW, str[ 0 ] );
				AppendMenu( hMenu, MFT_SEPARATOR, 0U, NULL );
				if( ! IsIconic( hWnd ) )
				{
					EnableMenuItem( hMenu, IDM_SHOWWINDOW, MF_BYCOMMAND | MF_GRAYED );
				}

				AppendMenu( hMenu, MFT_STRING, IDM_STOP_FROM_TRAY, str[ 1 ] );

				AppendMenu( hMenu, MFT_SEPARATOR, 0U, NULL );
				if( ! IsActive() && ! g_bHack[ 3 ] )
				{
					EnableMenuItem( hMenu, IDM_STOP_FROM_TRAY, MF_BYCOMMAND | MF_GRAYED );
				}
				
				AppendMenu( hMenu, MFT_STRING, IDM_EXIT_FROM_TRAY, str[ 2 ] );

				EnableMenuItem( hMenu, IDM_EXIT_FROM_TRAY,
					( (UINT) MF_BYCOMMAND | ( IsActive() ? MF_GRAYED : MF_ENABLED ) )
				);

				GetCursorPos( &pt );
				SetForegroundWindow( hWnd );
				TrackPopupMenu( hMenu, TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN,
					pt.x, pt.y, 0, hWnd, (LPRECT) NULL );
				DestroyMenu( hMenu );
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
				int iSlider = ParseArgs( strCommand, CCHPATH, strTargetPath, strTargetExe, strOptions );
				
				if( iSlider == IGNORE_ARGV )
				{
					bHandled = TRUE;
					break;
				}

				HMENU hMenu = GetMenu( hWnd );
				const bool fAllowMoreThan99 = 
							!!( MF_CHECKED & GetMenuState( hMenu, IDM_ALLOWMORETHAN99, MF_BYCOMMAND ) );
				
				if( 99 < iSlider && ! fAllowMoreThan99 )
					iSlider = 99;

				if( IsOptionSet( strOptions, _T("--watch-multi"), NULL )
					&&
					!( MF_CHECKED & GetMenuState( hMenu, IDM_WATCH_MULTI, MF_BYCOMMAND ) ) )
				{
					_toggle_opt_menu_item( hWnd, IDM_WATCH_MULTI, _T("WatchMulti") );
				}

				DWORD rgPID[ 3 ] = {0L,0L,0L};
				ptrdiff_t numOfPIDs = 0;
				while( numOfPIDs < 3 )
				{
					const DWORD pid = PathToProcessID( strTargetPath, rgPID, numOfPIDs );
					if( pid == (DWORD) -1 )
						break;

					if( ! IsProcessBES( pid ) )
						rgPID[ numOfPIDs++ ] = pid;
				}

				const BOOL bUnlimit = IsOptionSet( strOptions, _T("--unlimit"), _T("-u") );
				const BOOL bToggle = ! bUnlimit && IsOptionSet( strOptions, _T("--toggle"), _T("-t") );
				BOOL bRewatch = FALSE;
				
				for( ptrdiff_t n = 0; n < numOfPIDs; ++n )
				{
					for( ptrdiff_t i = 0; i < 3; ++i )
					{
						if( rgPID[ n ] == g_dwTargetProcessId[ i ] )
						{
							if( iSlider != 0 && g_Slider[ i ] != iSlider )
							{
								g_Slider[ i ] = iSlider;
								RECT minirect;
								SetRect( &minirect, 20, 20 + 90 * (int) i, 479, 40 + 90 * (int) i );
								InvalidateRect( hWnd, &minirect, FALSE );
								SetSliderIni( strTargetPath, iSlider );
							}

							if( g_bHack[ i ] ) // currently limited
							{
								if( bUnlimit || bToggle )
								{
									SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, 0 );

									if( i == 2 && g_bHack[ 3 ] )
										SendMessage( hWnd, WM_COMMAND, IDM_UNWATCH, 0 );
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
							
							break; // next n
						} // pid found
					} // for i
				} // for n
				
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
#ifdef UNICODE
					if( Lstrcmpi( strTargetPath, hp[ 2 ].lpTarget->szPath ) == 0 )
#else
					// In non-Unicode, a watched target path may be a full-path, an exe-only path,
					// or possibly a 15-character truncated version of a long exe name.
					if( Lstrcmpi( strTargetPath, hp[ 2 ].lpTarget->szPath ) == 0
						|| _strnicmp( strTargetExe, hp[ 2 ].lpTarget->szPath, 15 ) == 0 )
#endif
					{
						if( iSlider != 0 )
							g_Slider[ 2 ] = iSlider;

						if( bUnlimit || bToggle )
							SendMessage( hWnd, WM_COMMAND, IDM_UNWATCH, 0 );
					
						bHandled = TRUE;
					}
				}

				if( bUnlimit // unlimit it; don't watch
					|| ( bToggle && bHandled ) && ! bRewatch // toggle-handled; don't watch (in this case g_bHack[ 3 ] is true anyway)
					|| ( g_bHack[ 2 ] || g_bHack[ 3 ] ) // no free slot to watch
				) break;

				hp[ 2 ].lpTarget->dwProcessId = TARGET_PID_NOT_SET; // reset

				if( iSlider >= SLIDER_MIN && iSlider <= SLIDER_MAX )
					SetSliderIni( strTargetPath, iSlider );

#if ! defined(UNICODE)
				strcpy_s( strTargetPath, CCHPATH, strTargetExe );
#endif

				SetTargetPlus( hWnd, &hChildThread[ 3 ], &hp[ 2 ], strTargetPath, strTargetExe );
				bHandled = TRUE;
				break;
			}

			default:
				break;
			}

			SendMessage( hWnd, WM_USER_STOP, JUST_UPDATE_STATUS, STOPF_NO_INVALIDATE );

			return (LRESULT) bHandled;
			break;
		}
		
		case WM_COMMAND:
		{
			const UINT id = LOWORD( wParam );
			const UINT eventCode = HIWORD( wParam );

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
				{
					int iMyId = 0;

					if( !g_bHack[ 0 ] && WaitForSingleObject( hSemaphore[ 0 ], 100UL ) == WAIT_OBJECT_0 ) iMyId = 0;
					else if( !g_bHack[ 1 ] && WaitForSingleObject( hSemaphore[ 1 ], 100UL ) == WAIT_OBJECT_0 ) iMyId = 1;
					else if( !g_bHack[ 2 ] && WaitForSingleObject( hSemaphore[ 2 ], 100UL ) == WAIT_OBJECT_0 ) iMyId = 2;
					else
					{
						MessageBox( hWnd, TEXT( "Semaphore Error" ), APP_NAME, MB_OK | MB_ICONEXCLAMATION );
						break;
					}

					TARGETINFO tgtInfoSaved = *(hp[ iMyId ].lpTarget);

					const INT_PTR iReturn = DialogBoxParam( g_hInst, (LPCTSTR) IDD_XLIST, 
															hWnd, &xList, (LPARAM) &hp[ iMyId ] );

					if( iReturn == XLIST_CANCELED )
					{
						ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
						break;
					}
					else if( iReturn == XLIST_RESTART_0 || iReturn == XLIST_RESTART_1 || iReturn == XLIST_RESTART_2 )
					{
						ReleaseSemaphore( hSemaphore[ iMyId ], 1L, (LONG *) NULL );
						SendMessage( hWnd, WM_USER_RESTART, (WPARAM) iReturn, 0L );
						break;
					}
					else if( iReturn == XLIST_NEW_TARGET )
					{
						int iValue = GetSliderIni( hp[ iMyId ].lpTarget->szPath, hWnd );
						const bool fAllowMoreThan99 = 
										!!( MF_CHECKED & GetMenuState( GetMenu( hWnd ), 
										IDM_ALLOWMORETHAN99, MF_BYCOMMAND ) );
						if( 99 < iValue && ! fAllowMoreThan99 ) iValue = 99;

						g_Slider[ iMyId ] = iValue;

						TCHAR str[ 1024 ];
						_stprintf_s( str, _countof(str), _T( "GetSliderIni( \"%s\" ) = %d" ),
										hp[ iMyId ].lpTarget->szPath, g_Slider[ iMyId ] );
						WriteDebugLog( str );

						SendMessage( hWnd, WM_USER_HACK, (WPARAM) iMyId, (LPARAM) &hp[ iMyId ] );
						break;
					}
					else if( iReturn == XLIST_WATCH_THIS )
					{
						if( g_bHack[ 2 ] || g_bHack[ 3 ] )
						{
							MessageBox( hWnd, TEXT( "Busy. Can't start watching." ),
								APP_NAME, MB_ICONEXCLAMATION | MB_OK );
							ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
							break;
						}

						if( iMyId != 2 )
						{
							hp[ 2 ].iMyId = 2;
							hp[ 2 ].lpTarget->dwProcessId = hp[ iMyId ].lpTarget->dwProcessId;

							/*for( ptrdiff_t s = 0; 
								s < (ptrdiff_t) hp[ iMyId ].lpTarget->wThreadCount; ++s )
							{
								hp[ 2 ].lpTarget->dwThreadId[ s ] = hp[ iMyId ].lpTarget->dwThreadId[ s ];
							}*/
							hp[ 2 ].lpTarget->iIFF = hp[ iMyId ].lpTarget->iIFF;
							_tcscpy_s( hp[ 2 ].lpTarget->szExe, MAX_PATH * 2, hp[ iMyId ].lpTarget->szExe );
							_tcscpy_s( hp[ 2 ].lpTarget->szPath, MAX_PATH * 2, hp[ iMyId ].lpTarget->szPath );
							_tcscpy_s( hp[ 2 ].lpTarget->szText, MAX_WINDOWTEXT, hp[ iMyId ].lpTarget->szText );
							//hp[ 2 ].lpTarget->wThreadCount = hp[ iMyId ].lpTarget->wThreadCount;

							hp[ iMyId ].lpTarget->dwProcessId = TARGET_PID_NOT_SET;
							hp[ iMyId ].lpTarget->iIFF = IFF_UNKNOWN;
							_tcscpy_s( hp[ iMyId ].lpTarget->szExe, MAX_PATH * 2, TARGET_UNDEF );
							_tcscpy_s( hp[ iMyId ].lpTarget->szPath, MAX_PATH * 2, TARGET_UNDEF );
							hp[ iMyId ].lpTarget->szText[ 0 ] = _T('\0');
							//hp[ iMyId ].lpTarget->wThreadCount = 0;
							g_dwTargetProcessId[ iMyId ] = TARGET_PID_NOT_SET;
							_tcscpy_s( g_szTarget[ iMyId ], CCHPATHEX, TARGET_UNDEF );
							for( int i = 0 + iMyId * 4; i < 4 + iMyId * 4; i++ )
							{
								*lpszStatus[ i ] = _T('\0');
							}
						}

						ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );

						SetTargetPlus( hWnd, &hChildThread[ 3 ], &hp[ 2 ],
										hp[ 2 ].lpTarget->szPath, hp[ 2 ].lpTarget->szExe );
						break;
					}
					else if( iReturn == XLIST_UNFREEZE )
					{
						SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, (LPARAM) 0 );
						Sleep( 1000 );
						if( Unfreeze( hWnd, hp[ iMyId ].lpTarget->dwProcessId ) )
						{
							MessageBox( hWnd, TEXT( "Successful!" ),
										TEXT( "Unfreezing" ), MB_ICONINFORMATION | MB_OK );
						}
						else
						{
							MessageBox( hWnd, TEXT( "An error occurred.\r\nPlease retry." ),
										TEXT( "Unfreezing" ), MB_ICONEXCLAMATION | MB_OK );
						}

						*(hp[ iMyId ].lpTarget) = tgtInfoSaved; // 20140310

						ReleaseSemaphore( hSemaphore[ iMyId ], 1L, NULL );
						break;
					}
					break;
				}

				case IDM_WATCH:
				{
					SetTargetPlus( hWnd, &hChildThread[ 3 ], &hp[ 2 ] );
					break;
				}

				case IDM_UNWATCH:
				{
					WriteDebugLog( TEXT( "IDM_UNWATCH" ) );
					
					if( Unwatch( hSemaphore[ 3 ], hChildThread[ 3 ], g_bHack[ 3 ] ) )
					{
						EnableMenuItem( GetMenu( hWnd ), IDM_UNWATCH, MF_BYCOMMAND | MF_GRAYED );
					}
					break;
				}

				case IDM_STOP:
				{
					WriteDebugLog( TEXT( "IDM_STOP" ) );
					if( g_bHack[ 3 ] ) // Watching...
					{
						Unwatch( hSemaphore[ 3 ], hChildThread[ 3 ], g_bHack[ 3 ] );
					}

					for( int i = 0; i < 3; i++ )
					{
						if( g_bHack[ i ] )
						{
							SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, 0L );
						}
					}
					break;
				}

				case IDM_STOP_FROM_TRAY:
				{
					WriteDebugLog( TEXT( "IDM_STOP_FROM_TRAY" ) );
					if( g_bHack[ 3 ] ) // Watching...
					{
						Unwatch( hSemaphore[ 3 ], hChildThread[ 3 ], g_bHack[ 3 ] );
					}

					for( ptrdiff_t i = 0; i < 3; ++i )
					{
						if( g_bHack[ i ] )
							SendMessage( hWnd, WM_USER_STOP, (WPARAM) i, (LPARAM) STOP_FROM_TRAY );
					}

					break;
				}
				
				case IDM_SETTINGS:
				{
					DialogBoxParam( g_hInst, MAKEINTRESOURCE( IDD_SETTINGS ), hWnd, 
									&Settings, (LPARAM) hp[ 0 ].lpszStatus );
					UpdateStatus( hWnd ); // <-- 1.0 beta2
		 			break;
				}

				case IDM_REALTIME:
				{
					// toggle the mode...
					g_fRealTime = ! g_fRealTime;

					// Change the priority
					SetRealTimeMode( hWnd,
						g_fRealTime,
						lpszWindowText,
						_countof(lpszWindowText)
					);
					break;
				}

#if 0
				case IDM_DEBUGPRIVILEGE:
				{
					HMENU hMenu = GetMenu( hWnd );
					bool fEnable =
						!( MF_CHECKED & GetMenuState( hMenu, 
							IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) );
					CheckMenuItem( hMenu, 
						IDM_DEBUGPRIVILEGE, 
						(UINT)( fEnable ? ( MF_BYCOMMAND | MFS_CHECKED )
						: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );
					
					const TCHAR * strIniPath = GetIniPath();
					WritePrivateProfileString(
						TEXT( "Options" ), 
						TEXT( "DebugPrivilege" ),
						fEnable ? TEXT( "1" ) : TEXT( "0" ),
						strIniPath
					);
					break;
				}
#endif
				case IDM_ALLOWMORETHAN99:
				{
					HMENU hMenu = GetMenu( hWnd );
					const bool fAllowMoreThan99 =
								!( MF_CHECKED & GetMenuState( hMenu, id, MF_BYCOMMAND ) );

					CheckMenuItem( hMenu, id, 
									(UINT)( fAllowMoreThan99 ? ( MF_BYCOMMAND | MFS_CHECKED )
											: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );

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
					
					const TCHAR * strIniPath = GetIniPath();
					WritePrivateProfileString(
						TEXT( "Options" ), 
						TEXT( "AllowMoreThan99" ),
						fAllowMoreThan99 ? TEXT( "1" ) : TEXT( "0" ),
						strIniPath );

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
					_toggle_opt_menu_item( hWnd, id, _T("AllowMulti") );
					break;

				case IDM_ALWAYS_LISTALL:
					_toggle_opt_menu_item( hWnd, id, _T("ListAll") );
					break;

				case IDM_WATCH_MULTI:
					_toggle_opt_menu_item( hWnd, id, _T("WatchMulti") );
					break;

				case IDM_DISABLE_F1:
					_toggle_opt_menu_item( hWnd, id, _T("DisableF1") );
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
				{
					SendMessage( hWnd, WM_USER_NOTIFYICON, 0U, WM_LBUTTONDBLCLK );
					break;
				}

				case IDM_ABOUT:
				{
					DialogBoxParam( g_hInst, (LPCTSTR) IDD_ABOUTBOX, hWnd, &About, (LPARAM) hWnd );
		 			break;
				}
				
				case IDM_HOMEPAGE:
				{
					OpenBrowser( APP_HOME_URL );
					break;
				}
				
				case IDM_GHOSTCENTER:
				{
					//OpenBrowser( TEXT( "http://ngc.sherry.jp/" ) );
					OpenBrowser( TEXT( "http://www.aqrs.jp/ngc/" ) );
					//OpenBrowser( TEXT( "http://ghosttown.mikage.jp/" ) );
					break;
				}
				
				case IDM_UKAGAKA:
				{
					OpenBrowser( TEXT( "http://usada.sakura.vg/" ) );
					break;
				}
				
				case IDM_SSP:
				{
					OpenBrowser( TEXT( "http://ssp.shillest.net/" ) );
					break;
				}
/*
				case IDM_CROW:
				{
					OpenBrowser( TEXT( "http://crow.aqrs.jp/" ) );
					break;
				}
*/
				case IDM_ABOUT_SHORTCUTS:
				{
					AboutShortcuts( hWnd );
					break;
				}

				case IDM_HELP_CMDLINE:
					ShowCommandLineHelp( hWnd );
					break;

				case IDM_SKIN:
				{
					ChangeSkin( hWnd, hMemDC, SkinSize, hOrigBmp );
					break;
				}

				case IDM_SNAP:
				{
					SaveSnap( hWnd );
					break;
				}

				case IDM_EXIT_FROM_TRAY:
				{
					if( IsActive() )
					{
						//MessageBox( hWnd, TEXT( "Can't exit now because it's active." ),
						//	APP_NAME, MB_OK | MB_ICONEXCLAMATION );
						SendMessage( hWnd, WM_COMMAND, IDM_STOP, 0 );
					}

					Exit_CommandFromTaskbar( hWnd );
				    break;
				}
				
				case IDM_EXIT:
				{
					SendMessage( hWnd, WM_CLOSE, 0U, 0 );
				    break;
				}
				
				case IDM_EXIT_ANYWAY:
				{
					/*HFONT hMyFont = MyCreateFont( _T("Verdana"), 32, TRUE, FALSE );
					HDC hDC = GetDC( hWnd );
					HFONT hOldFont = ( HFONT ) SelectObject( hDC, hMyFont );
					SetBkMode( hDC, OPAQUE );
					SetBkColor( hDC, RGB( 0xFF, 0xFF, 0x00 ) );
					SetTextColor( hDC, RGB( 0xFF, 0x00, 0x00 ) );
					TCHAR str[ 100 ] = _T( " Forced Termination... " );
					TextOut( hDC, 50, 50, str, (int) _tcslen( str ) );
					SelectObject( hDC, hOldFont );
					ReleaseDC( hWnd, hDC );
					DeleteFont( hMyFont );*/

					ShowWindow( hWnd, SW_HIDE );
					
					SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, 0L );
					Sleep( 2000ul );
					SendMessage( hWnd, WM_CLOSE, 0U, 0L );
					break;
				}

				case IDM_GSHOW:
				{
					GShow( hWnd );
					break;
				}

				case IDM_MINIMIZE: // unofficial VK_M handler
				{
					ShowWindow( hWnd, SW_HIDE );
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

#endif
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
			iCursor = MainWindowUrlHit( lParam );
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
			
			if( MainWindowUrlHit( lParam ) )
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
			
			if( iMouseDown == 1 && MainWindowUrlHit( lParam ) )
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
			if( MainWindowUrlHit( lParam ) ) break;
			
			POINT pt = {0};
			GetCursorPos( &pt );
			ScreenToClient( hWnd, &pt );
			if( ChildWindowFromPoint( hWnd, pt ) != hWnd ) break;

			ChangeSkin( hWnd, hMemDC, SkinSize, hOrigBmp );
			break;
		}

		case WM_RBUTTONDBLCLK:
		{
			SendMessage( hWnd, WM_COMMAND, IDM_GSHOW, 0 );
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
					|| MainWindowUrlHit( MAKELPARAM( ptClient.x, ptClient.y ) ) )
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

			DrawSkin( hdc, hMemDC, SkinSize );

			HFONT hOldFont = NULL;
			
			if( _tcscmp( lpszStatus[ 0 ], APP_LONGNAME ) == 0 )
			{
				hOldFont = SelectFont( hdc, hFontItalic );
#ifdef _UNICODE
				_tcscpy_s( lpszStatus[ 2 ], cchStatus, _T( "Unicode Build / " ) );

				if( IS_JAPANESE )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE,
						IS_JAPANESEo ? S_JPNo_0001 : S_JPN_0001,
						-1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else if( IS_FINNISH )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_0001, -1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else if( IS_SPANISH )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_0001, -1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else if( IS_CHINESE_T )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_0001T, -1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else if( IS_CHINESE_S )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_0001S, -1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else if( IS_FRENCH )
				{
					WCHAR str[ 1024 ]=L"";
					MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_0001, -1, str, 1023 );
					_tcscat_s( lpszStatus[ 2 ], cchStatus, str );
				}
				else
				{
					_tcscat_s( lpszStatus[ 2 ], cchStatus, L"English" );
				}
#else
				strcpy_s( lpszStatus[ 2 ], cchStatus, _T( "Non-Unicode Build" ) );
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

			TextOut( hdc, x, y, lpszStatus[ 0 ], (int) _tcslen( lpszStatus[ 0 ] ) );
			y += 20;

			SelectFont( hdc, g_hFont );

			for( int i = 1; i < 12; i++ )
			{
				if( i % 4 == 0 ) SetTextColor( hdc, RGB( 0x99, 0xff, 0x66 ) );
				else if( i % 4 == 3 ) SetTextColor( hdc, RGB( 0x66, 0xee, 0xee ) );
				else SetTextColor( hdc, RGB( 0xff, 0xff, 0x99 ) );
				TextOut( hdc, x, y, lpszStatus[ i ], (int) _tcslen( lpszStatus[ i ] ) );
				y += 20;
				if( i % 4 == 3 ) y += 10;
			}
			TextOut( hdc, x + 20, y, lpszStatus[ 12 ], (int) _tcslen( lpszStatus[ 12 ] ) );
			SetTextColor( hdc, RGB( 0xff, 0xff, 0xcc ) );
			x += 40;
			y += 20;
			TextOut( hdc, x, y + 20, lpszStatus[ 13 ], (int) _tcslen( lpszStatus[ 13 ] ) );
			TextOut( hdc, x, y + 40, lpszStatus[ 14 ], (int) _tcslen( lpszStatus[ 14 ] ) );
			TextOut( hdc, x, y + 60, lpszStatus[ 15 ], (int) _tcslen( lpszStatus[ 15 ] ) );


			HFONT hFontURL = GetFontForURL( hdc );
			SelectFont( hdc, hFontURL );

			SetTextColor( hdc, iCursor == 0? RGB( 0x66, 0x99, 0x99 ) :
				iCursor == 1? RGB( 0x66, 0xff, 0xff ) : RGB( 0xff, 0xcc, 0x00 ) );
			TextOut( hdc, 425, 400, APP_HOME_URL, (int) _tcslen( APP_HOME_URL ) );

			SelectFont( hdc, hOldFont );
			DeleteFont( hFontURL );

			RestoreDC( hdc, iSaved );
			EndPaint( hWnd, &ps );
			break;
		}

		case WM_SYSKEYDOWN:
#define FIRST_PRESSED( lParam ) ( ! ( ((ULONG_PTR)lParam) & 0xC0000000 ) )
#define KEY_CONTEXT_1( lParam ) (     (lParam) & 0x20000000L   )

			// ALT + T (&Target): Valid unless the language is Finnish; if Finnish, ALT + T
			// is for the File menu ("&Tiedosto") by system def handling.
			if( wParam == 'T'
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
					PostMessage( hWnd, WM_COMMAND, IDM_ABOUT_SHORTCUTS, 0 );
				}
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
				ShowWindow( hWnd, SW_SHOWDEFAULT );
				SetForegroundWindow( hWnd );

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

			SendMessage( hWnd, WM_COMMAND, (WPARAM) IDM_STOP, 0L );

			if(
				WaitForSingleObject( hSemaphore[ 0 ], 1000 ) == WAIT_OBJECT_0 &&
				WaitForSingleObject( hSemaphore[ 1 ], 1000 ) == WAIT_OBJECT_0 &&
				WaitForSingleObject( hSemaphore[ 2 ], 1000 ) == WAIT_OBJECT_0 &&
				WaitForSingleObject( hSemaphore[ 3 ], 1000 ) == WAIT_OBJECT_0
			)
			{
				WriteDebugLog( TEXT( "[ All Semaphores OK ]" ) );
				for( int i = 0; i < 4; i++ )
				{
					ReleaseSemaphore( hSemaphore[ i ], 1L, (LPLONG) NULL );
				}
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
					hOrigBmp = NULL;
				}
				
				DeleteDC( hMemDC );
				hMemDC = NULL;
			}

			NOTIFYICONDATA ni;
			InitNotifyIconData( hWnd, &ni, &ti[ 0 ] );
			Shell_NotifyIcon( NIM_DELETE, &ni );

			DeleteFont( g_hFont );
			g_hFont = NULL;
			DeleteFont( hFontItalic );

			for( int i = 0; i < 4; i++ ) 
			{
				if( hChildThread[ i ] ) CloseHandle( hChildThread[ i ] );
				if( hSemaphore[ i ] ) CloseHandle( hSemaphore[ i ] );
			}

			PostQuitMessage( 0 );
			
			break;
		}
		
		default:
			if( uMessage == uMsgTaskbarCreated )
			{
				NOTIFYICONDATA ni;
				DWORD iDllVersion = InitNotifyIconData( hWnd, &ni, &ti[ 0 ] );
				Shell_NotifyIcon( NIM_ADD, &ni );
				if( iDllVersion >= 5 )
				{
					ni.uVersion = NOTIFYICON_VERSION;
					Shell_NotifyIcon( NIM_SETVERSION, &ni );
				}
				UpdateStatus( hWnd );
				break;
			}
			return DefWindowProc( hWnd, uMessage, wParam, lParam );
			break;
   }
   return 0;
}

VOID SetRealTimeMode( HWND hWnd, bool fRealTime, LPTSTR lpszWindowText, size_t cchWindowText )
{
	g_fRealTime = fRealTime;
	if( fRealTime )
	{
		SetPriorityClass( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
//		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
		CheckMenuItem( GetMenu( hWnd ), IDM_REALTIME, MF_BYCOMMAND | MFS_CHECKED );
/*
		for( i = 0; i < 4; i++ )
		{
			if( g_bHack[ i ] && phChildThread[ i ] != NULL )
			{
				SetThreadPriority( phChildThread[ i ], THREAD_PRIORITY_HIGHEST );
			}
		}
*/
		WriteDebugLog( TEXT( "Real-time mode: Yes" ) );
	}
	else
	{
		SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
//		SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_NORMAL );
		CheckMenuItem( GetMenu( hWnd ), IDM_REALTIME, MF_BYCOMMAND | MFS_UNCHECKED );
/*
		for( i = 0; i < 4; i++ )
		{
			if( g_bHack[ i ] && phChildThread[ i ] != NULL )
			{
				SetThreadPriority( phChildThread[ i ], THREAD_PRIORITY_NORMAL );
			}
		}
*/
		WriteDebugLog( TEXT( "Real-time mode: No" ) );
	}

	if( lpszWindowText != NULL )
	{
		_stprintf_s( lpszWindowText, cchWindowText, _T("%s - %s%s"),
						APP_LONGNAME,
						IsActive() ? _T("Active") : _T("Idle"),
						fRealTime ? _T(" (Real-time mode)") : _T("") );
		SetWindowText( hWnd, lpszWindowText );
	}
}

static int MainWindowUrlHit( LPARAM lParam )
{
	int x = (int) LOWORD( lParam );
	int y = (int) HIWORD( lParam );
	return ( x > 424 && x < 619 && y > 401 && y < 418 );
}

static inline HFONT _create_font( LPCTSTR lpszFace, int iSize, BOOL bBold, BOOL bItalic )
{
	return CreateFont( iSize,
				0, 0, 0,
				bBold ? FW_BOLD : FW_NORMAL,
				(DWORD) (!! bItalic ),
				0UL, 0UL,
				DEFAULT_CHARSET,
				//ANSI_CHARSET,
				OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				FF_SWISS | VARIABLE_PITCH,
				lpszFace
			);
}


static bool _toggle_opt_menu_item( HWND hWnd, UINT uMenuId, const TCHAR * pIniOptionName )
{
	HMENU hMenu = GetMenu( hWnd );
	UINT uMenuState = GetMenuState( hMenu, uMenuId, MF_BYCOMMAND );
	const bool fEnable = !( uMenuState & MFS_CHECKED ); // toggle

	WritePrivateProfileString(
		TEXT( "Options" ), 
		pIniOptionName,
		fEnable ? TEXT( "1" ) : TEXT( "0" ),
		GetIniPath() );
	
	CheckMenuItem( hMenu, uMenuId, 
					(UINT)( fEnable ? ( MF_BYCOMMAND | MFS_CHECKED )
					: ( MF_BYCOMMAND | MFS_UNCHECKED ) ) );

	return fEnable;
}