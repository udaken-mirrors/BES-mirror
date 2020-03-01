/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

extern volatile int g_Slider[ MAX_SLOTS ];
extern volatile BOOL g_bHack[ MAX_SLOTS ];
extern bool g_fHide;
//extern volatile DWORD g_dwTargetProcessId[ MAX_SLOTS ];
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );
// NOTIFYICONDATA_V2_SIZE (936 for W, 488 for A)

// This will work too
//#define	NOTIFYICONDATA_V2_SIZE_fixed     FIELD_OFFSET(NOTIFYICONDATA,dwInfoFlags) + sizeof(DWORD)

#ifdef _UNICODE
# define NOTIFYICONDATA_V2_SIZE_fixed    ((DWORD)936)
#else
# define NOTIFYICONDATA_V2_SIZE_fixed    ((DWORD)488)
#endif




/*
	const DWORD iDllVersion = GetShell32Version();
	DWORD dwSize = (DWORD) ( 
							( 5 <= iDllVersion ) ?
#if (NTDDI_VERSION >= NTDDI_WINXP)
							NOTIFYICONDATA_V2_SIZE
#else
							sizeof( NOTIFYICONDATA )
#endif
							: NOTIFYICONDATA_V1_SIZE
						);
*/

static TCHAR * _percent2str( int slider, TCHAR * str, size_t cch )
{
	if( slider <= 99 )
		_stprintf_s( str, cch, _T("-%d%%"), slider );
#ifdef ALLOW100
	else if( slider == SLIDER_MAX )
		_stprintf_s( str, cch, _T("-%d%%"), 100 );
#endif
	else
		_stprintf_s( str, cch, _T("-%.1f%%"), 99.0 + ( slider - 99 ) * 0.1 );

	return str;
}

static const TCHAR * getExe( const TARGETINFO& ti )
{
	const TCHAR * q = ti.disp_path;
	if( ! q ) q = ti.lpPath;
	if( ! q ) return NULL;
	const TCHAR * pExe = _tcsrchr( q, _T('\\') );
	if( pExe ) ++pExe;
	else pExe = q;
	return pExe;
}

void SendNotifyIconData( HWND hWnd, const TARGETINFO * ti, DWORD dwMessage )
{
	if( g_fHide ) return;

	NOTIFYICONDATA ni = {0};
	ni.cbSize = NOTIFYICONDATA_V2_SIZE_fixed;
	ni.hWnd = hWnd;
	ni.uID = NI_ICONID;
	ni.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	ni.uCallbackMessage = WM_USER_NOTIFYICON;
	ni.hIcon = (HICON) GetClassLongPtr_Floral( hWnd, GCL_HICONSM );
	const size_t cchTip = 128; // (5<=DllVersion) ? 128:64 and Win2K ver=5.0

	if( ! ti )
	{
		_tcscpy_s( ni.szTip, cchTip, _T("BES") );
	}
	else if( IsActive() )
	{
		_tcscpy_s( ni.szTip, cchTip, _T("BES - Active") );

		for( ptrdiff_t i = 0; i < MAX_SLOTS; ++i )
		{
			if( i != 3 && g_bHack[ i ] )
			{
				TCHAR percent[ 16 ];
				_percent2str( g_Slider[ i ], percent, _countof(percent) );

				const TCHAR * pExe = getExe( ti[ i ] );
				if( ! pExe ) break;
				const TCHAR * pDot = _tcsrchr( pExe, _T('.') );
				int cch = ( pDot && pExe < pDot ) ? (int)( pDot - pExe ) : (int) _tcslen( pExe );

				TCHAR szLine[ 64 ];
				if( cch < 16 )
					_stprintf_s( szLine, _countof(szLine), _T("\n%.*s %s"), cch, pExe, percent );
				else
					_stprintf_s( szLine, _countof(szLine), _T("\n%.12s... %s"), pExe, percent );

				if( _tcsncat_s( ni.szTip, cchTip - 3, szLine, _TRUNCATE ) != 0 )
				{
					_tcscat_s( ni.szTip, cchTip, _T("...") );
					break;
				}
			}
		}
	}
	else if( g_bHack[ 3 ] )
	{
		// We can store up to 128 cch (szTip[128]), but 128 is too long for one line.
		// 60-ish cch, with trailing "..." if necessary, is better.
		// NOTE: _sntprintf_s returns -1 if truncation occurred.  While this behavior with _TRUNCATE is 
		// documented, the return value on truncation *without* _TRUNCATE is not well documented.
		// To avoild any potential problems, let's just use _TRUNCATE here.
		const TCHAR * pExe = getExe( ti[ 2 ] );
		if( pExe )
		{
			const TCHAR * pDot = _tcsrchr( pExe, _T('.') );
			int cch = ( pDot && pExe < pDot ) ? (int)( pDot - pExe ) : (int) _tcslen( pExe );
			//if( _sntprintf_s( ni.szTip, cchTip, 60, _T("BES - Watching %.*s"), cch, pExe ) == -1 )
			if( _sntprintf_s( ni.szTip, 60 + 1, _TRUNCATE, _T("BES - Watching %.*s"), cch, pExe ) == -1 )
			{
				_tcscat_s( ni.szTip, cchTip, _T("...") );
			}
		}
	}
	else
	{
		_tcscpy_s( ni.szTip, cchTip, _T("BES - Idle") );
	}

	if( dwMessage == NIM_MODIFY )
	{
		Shell_NotifyIcon( NIM_MODIFY, &ni );
	}
	else if( dwMessage == NIM_ADD )
	{
		Shell_NotifyIcon( NIM_ADD, &ni );
		ni.uVersion = NOTIFYICON_VERSION;
		Shell_NotifyIcon( NIM_SETVERSION, &ni );
	}
}
#if 0
DWORD GetShell32Version( VOID )
{
	static DWORD versionCached = 0L;
	if( versionCached ) return versionCached;

	TCHAR lpPath[ MAX_PATH * 2 ] = _T("");
	GetSystemDirectory( lpPath, (UINT) _countof(lpPath) );
	_tcscat_s( lpPath, _countof(lpPath), _T( "\\Shell32.dll" ) );
	HMODULE hModule = LoadLibrary( lpPath );
	if( ! hModule )
		return 0L;

//#pragma warning (push)
//#pragma warning (disable: 4191) // unsafe cast
	// Cast to (void*) once, and you're not getting C4191 anymore.
	DLLGETVERSIONPROC pfnDllGetVersion = (DLLGETVERSIONPROC)
											(void*) GetProcAddress( hModule, "DllGetVersion" );
//#pragma warning (pop)

	if( pfnDllGetVersion )
	{
		DLLVERSIONINFO dvi;
		dvi.cbSize = (DWORD) sizeof( dvi );
		const HRESULT hr = (*pfnDllGetVersion)( &dvi );

		if( SUCCEEDED( hr ) )
		{
			versionCached = dvi.dwMajorVersion;
		}
	}

	FreeLibrary( hModule );

	return versionCached;
}
#endif

void NotifyIcon_OnContextMenu( HWND hWnd )
{
	HMENU hMenu = CreatePopupMenu();
	POINT pt;

	TCHAR str[ 3 ][ 256 ];
#ifdef _UNICODE 
	if( IS_JAPANESE )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_26, -1, str[ 2 ], 256 );
	}
	else if( IS_FINNISH )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_26, -1, str[ 2 ], 256 );
	}
	else if( IS_SPANISH )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_26, -1, str[ 2 ], 256 );
	}
	else if( IS_CHINESE_T )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_26, -1, str[ 2 ], 256 );
	}
	else if( IS_CHINESE_S )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_26, -1, str[ 2 ], 256 );
	}
	else if( IS_FRENCH )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_26, -1, str[ 2 ], 256 );
	}
	else
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_24, -1, str[ 0 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_5, -1, str[ 1 ], 256 );
		MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_26, -1, str[ 2 ], 256 );
	}
#else
	strcpy_s( str[ 0 ], 256, _T( "&Restore BES" ) );
	strcpy_s( str[ 1 ], 256, _T( "&Unlimit all" ) );
	strcpy_s( str[ 2 ], 256, _T( "E&xit" ) );
#endif

	if( IsWindowVisible( hWnd ) )
		AppendMenu( hMenu, MFT_STRING, IDM_MINIMIZE, TEXT("&Minimize BES") );
	else
		AppendMenu( hMenu, MFT_STRING, IDM_SHOWWINDOW, str[ 0 ] );

	AppendMenu( hMenu, MFT_SEPARATOR, 0, NULL );

		//EnableMenuItem( hMenu, IDM_SHOWWINDOW, MF_BYCOMMAND | MF_GRAYED );

	AppendMenu( hMenu, MFT_STRING, IDM_STOP_FROM_TRAY, str[ 1 ] );

	AppendMenu( hMenu, MFT_SEPARATOR, 0U, NULL );
	if( ! IsActive() && ! g_bHack[ 3 ] )
		EnableMenuItem( hMenu, IDM_STOP_FROM_TRAY, MF_BYCOMMAND | MF_GRAYED );
	
	AppendMenu( hMenu, MFT_STRING, IDM_EXIT_FROM_TRAY, str[ 2 ] );

	EnableMenuItem( hMenu, IDM_EXIT_FROM_TRAY,
		( (UINT) MF_BYCOMMAND | ( IsActive() ? MF_GRAYED : MF_ENABLED ) )
	);

	GetCursorPos( &pt );
	SetForegroundWindow( hWnd );
	int cmd = TrackPopupMenu(
					hMenu,
					TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN,
					pt.x, pt.y, 0, hWnd, NULL );

	if( cmd )
	{
		SendMessage( hWnd, WM_COMMAND, (WPARAM) cmd, 0 );
	}
	else
	{
		NOTIFYICONDATA ni={0};
		ni.cbSize = NOTIFYICONDATA_V2_SIZE_fixed;
		ni.hWnd = hWnd;
		ni.uID = NI_ICONID;
		ni.uFlags = 0;
		Shell_NotifyIcon( NIM_SETFOCUS, &ni );
	}
	DestroyMenu( hMenu );
}

void DeleteNotifyIcon( HWND hWnd )
{
	NOTIFYICONDATA ni;
	ni.cbSize = NOTIFYICONDATA_V2_SIZE_fixed;
	ni.hWnd = hWnd;
	ni.uID = NI_ICONID;
	ni.uFlags = 0;
	Shell_NotifyIcon( NIM_DELETE, &ni );
}

VOID Exit_CommandFromTaskbar( HWND hWnd )
{
	ShowWindow( hWnd, SW_HIDE );
	
	DeleteNotifyIcon( hWnd );

	SendMessage( hWnd, WM_COMMAND, (WPARAM)( IsActive() ? IDM_EXIT_ANYWAY : IDM_EXIT ), 0 );
}

