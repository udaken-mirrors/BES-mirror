#include "BattleEnc.h"

extern volatile int g_Slider[ 4 ];
extern volatile BOOL g_bHack[ 4 ];
extern volatile DWORD g_dwTargetProcessId[ 4 ];
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );

static inline bool IsActive( void )
{
	return ( g_bHack[ 0 ]
			|| g_bHack[ 1 ]
			|| g_bHack[ 2 ] && g_dwTargetProcessId[ 2 ] != WATCHING_IDLE );
}

DWORD InitNotifyIconData( HWND hWnd, NOTIFYICONDATA * lpni, TARGETINFO * ti )
{
	TCHAR str[ 1024 ] = TEXT( "" );
	TCHAR tmpstr[ 1000 ];
	int i;

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

	ZeroMemory( lpni, dwSize );
	lpni->cbSize = dwSize;
	lpni->hWnd = hWnd;
	lpni->uID = 0U;
	lpni->hIcon = (HICON) GetClassLongPtr_Floral( hWnd, GCL_HICONSM );

	/*wsprintf(str,TEXT("szTip cch=%d size=%d vi=%d\r\n"),
		sizeof(lpni->szTip)/sizeof(lpni->szTip[0]),sizeof(NOTIFYICONDATA),
		NOTIFYICONDATA_V1_SIZE);
	OutputDebugString(str);*/

	const size_t cchTip = (size_t)( 5 <= iDllVersion ? 128 : 64 );
	if( IsActive() )
	{
		// szTip TCHARs
		int maxlen = (int) cchTip - 1;
		_tcscpy_s( lpni->szTip, cchTip, _T( "BES - Active" ) );

		int lines = 1;
		for( i = 0; i < 3; i++ )
		{
			if( g_bHack[ i ] ) lines++;
		}

		if( lines != 0 ) maxlen = maxlen / lines - 5;

		for( i = 0; i < 3; i++ )
		{
			if( g_bHack[ i ] )
			{
				_tcscpy_s( tmpstr, _countof(tmpstr), ti[ i ].szExe );

				TCHAR * pos2 = _tcsstr( tmpstr, _T( ".ex" ) );
				if( pos2 == NULL ) pos2 = _tcsstr( tmpstr, _T( ".EX" ) );
				if( pos2 == NULL ) pos2 = _tcsstr( tmpstr, _T( ".e" ) );
				if( pos2 == NULL ) pos2 = _tcsstr( tmpstr, _T( ".E" ) );

				if( pos2 != NULL )
				{
					pos2[ 0 ] = _T( '\0' );
				}
				if( (int) _tcslen( tmpstr ) > maxlen )
				{
#ifdef _UNICODE
					WCHAR wc = tmpstr[ maxlen - 3 ];
					if( SURROGATE_LO( wc ) ) tmpstr[ maxlen - 4 ] = _T( '\0' );
					else tmpstr[ maxlen - 3 ] = _T( '\0' );
					wcscat_s( tmpstr, _countof(tmpstr), L"\x2026" ); // 'HORIZONTAL ELLIPSIS' (U+2026)
#else
					tmpstr[ maxlen - 3 ] = _T( '\0' );
					strcat_s( tmpstr, _countof(tmpstr), "..." );
#endif
					
				}

				TCHAR szPercentage[ 100 ];
				_stprintf_s( str, _countof(str), _T( "\r\n%s %s" ), tmpstr, 
					GetPercentageString( g_Slider[ i ], szPercentage, _countof(szPercentage) )
				);

				if( _tcslen( lpni->szTip ) + _tcslen( str ) < (int) cchTip ) //###
					_tcscat_s( lpni->szTip, cchTip, str );
				else
					break;
			}
		}
	}
	else if( g_bHack[ 3 ] )//---v1.3.8.2
	{
		_stprintf_s( str, _countof(str), // 1024 TCHARs
			_T( "BES - Watching %s" ), 
			ti[ 2 ].szExe // ~ MAX_PATH TCHARs
		);
		
		// Make it small enough for szTip in case it's too long
		// Programatically (5 <= iDllVersion? 127:63) is possible, but
		// 127 TCHARs would be visually too long. Using always 63 is fine.
		str[ 63 ] = _T('\0');
		//lstrcpy( lpni->szTip, str );
		_tcscpy_s( lpni->szTip, cchTip, str );
	}//---
	else
	{
		_tcscpy_s( lpni->szTip, cchTip, _T( "BES - Idle" ) );
	}

	if( g_bHack[ 3 ] )
	{
		if( g_dwTargetProcessId[ 2 ] == WATCHING_IDLE )
		{
			_stprintf_s( str, _countof(str), _T( "Watching %s...\r\n\r\n(Target not found)" ), ti[ 2 ].szExe );
		}
		else
		{
			_stprintf_s( str, _countof(str), _T( "Watching %s...\r\n\r\nTarget found!" ), ti[ 2 ].szExe );
		}
	}

	if( 5 <= iDllVersion )
	{
		// lstrcpy( lpni->szInfo, str );
		if( _tcslen( str ) < _countof(lpni->szInfo) )
			_tcscpy_s( lpni->szInfo, _countof(lpni->szInfo), str );
		lpni->uTimeout = 10U * 1000U;
		_tcscpy_s( lpni->szInfoTitle, _countof(lpni->szInfoTitle), APP_NAME );
		lpni->dwInfoFlags = NIIF_INFO;
	}
	lpni->uCallbackMessage = WM_USER_NOTIFYICON;
	lpni->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;

	return iDllVersion;
}

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
