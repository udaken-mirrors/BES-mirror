#include "BattleEnc.h"


TCHAR g_lpszLogFileName[ MAX_PATH * 2 ];

extern BOOL g_bLogging;

BOOL OpenDebugLog()
{
	if( ! g_bLogging ) return FALSE;
	
	GetModuleFileName( NULL, g_lpszLogFileName, MAX_PATH * 2UL - 64UL );
	const int len = (int) _tcslen( g_lpszLogFileName );
	for( int i = len - 1; i >= 0; i-- )
	{
		if( g_lpszLogFileName[ i ] == TEXT( '\\' ) )
		{
			g_lpszLogFileName[ i ] = TEXT( '\0' );
			break;
		}
	}

	if( g_lpszLogFileName[ 0 ] == TEXT( '\0' ) )
	{
		GetCurrentDirectory( MAX_PATH * 2 - 64, g_lpszLogFileName );
	}

	TCHAR tmpstr[ 64 ] = TEXT( "" );
	SYSTEMTIME st;
	GetLocalTime( &st );
	_stprintf_s( tmpstr, _countof(tmpstr), _T( "\\bes-%04d%02d%02d.log" ), st.wYear, st.wMonth, st.wDay );


	_tcscat_s( g_lpszLogFileName, MAX_PATH * 2, tmpstr );

	FILE * fdebug = NULL;	
	_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	if( fdebug == NULL ) return FALSE;

#ifdef _UNICODE
	fseek( fdebug, 0L, SEEK_END );
	if( ftell( fdebug ) == 0L )
		fputwc( L'\xFEFF', fdebug );
#endif
	_fputts( _T( "-------- START --------\r\n" ), fdebug );

	PrintFileHeader( fdebug );

	fclose( fdebug );
	return TRUE;
}

BOOL PrintFileHeader( FILE * fp )
{
	_fputts( APP_LONGNAME, fp );

#ifdef _UNICODE
	_fputts( _T( " (Unicode)\r\n" ), fp );
#else
	_fputts( _T( " (ANSI)\r\n" ), fp );
#endif

	SYSTEMTIME st;
	GetLocalTime( &st );

	TIME_ZONE_INFORMATION tzi;
	GetTimeZoneInformation( &tzi );
	int tz = abs( (int) tzi.Bias );
	int tz_sign = ( tzi.Bias < 0 ) ? ( +1 ) : ( -1 ) ; // if Bias is negative, TZ is positive
	int tz_h = tz / 60;
	int tz_m = tz - tz_h * 60;

	_ftprintf( fp, _T( "  %4d-%02d-%02d %02d:%02d:%02d.%03d%s%02d:%02d PID=%08lx\r\n\r\n" ),
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		tz_sign > 0 ? _T( "+" ) : _T( "-" ), tz_h, tz_m,
		GetCurrentProcessId() );

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	GetVersionEx( &osvi );
	TCHAR tmpstr[ 256 ] = TEXT( "Unknown OS" );
/*
	if( osvi.dwMajorVersion == 4UL )
	{
		if( osvi.dwMinorVersion == 0UL )
			lstrcpy( tmpstr, TEXT( "Windows NT 4.0" ) );
		else if( osvi.dwMinorVersion == 90UL )
			lstrcpy( tmpstr, TEXT( "Windows Me" ) );
	}
*/
	if( osvi.dwMajorVersion == 5UL )
	{
		if( osvi.dwMinorVersion == 0UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Windows 2000" ) );
		else if( osvi.dwMinorVersion == 1UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Windows XP" ) );
		else if( osvi.dwMinorVersion == 2UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Windows Server 2003 R2, Windows Server 2003, or Windows XP Professional x64 Edition" ) );
	}
	else if( osvi.dwMajorVersion == 6UL )
	{
		if( osvi.dwMinorVersion == 0UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Windows Vista or Windows Server 2008" ) );
		else if( osvi.dwMinorVersion == 1UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Windows 7 or Windows Server 2008 R2" ) );
	}


	_ftprintf( fp, _T( "%s ( OS Version: %lu.%lu Build %lu%s%s )\r\n" ),
		tmpstr,
		osvi.dwMajorVersion,
		osvi.dwMinorVersion,
		osvi.dwBuildNumber,
		osvi.szCSDVersion[ 0 ] ? _T(", ") : _T(""),
		osvi.szCSDVersion
	);

	LANGID lang = GetSystemDefaultLangID();
	VerLanguageName( (DWORD) lang, tmpstr, 255UL );
	_ftprintf( fp, _T( "  System Language: %u (%s)\r\n" ), lang, tmpstr );
	LCID lcid = GetSystemDefaultLCID();
	GetLocaleInfo( lcid, LOCALE_SNATIVELANGNAME, tmpstr, 255UL );
	_ftprintf( fp, _T( "  System Locale:   %lu (%s)\r\n" ), lcid, tmpstr );

	lang = GetUserDefaultLangID();
	VerLanguageName( (DWORD) lang, tmpstr, 255UL );
	_ftprintf( fp, _T( "  User Language:   %u (%s)\r\n" ), lang, tmpstr );

	lcid = GetUserDefaultLCID();
	GetLocaleInfo( lcid, LOCALE_SNATIVELANGNAME, tmpstr, 255UL );
	_ftprintf( fp, _T( "  User Locale:     %lu (%s)\r\n" ), lcid, tmpstr );
	_fputts( _T( "\r\n" ), fp );

	return TRUE;
}



BOOL CloseDebugLog()
{
	if( !g_bLogging ) return FALSE;

	FILE * fdebug = NULL;	
	_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	if( fdebug == NULL )
	{
		Sleep( 3000UL );
		_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	}

	if( fdebug == NULL )
	{
		MessageBox( NULL, TEXT( "CloseDebugLog() failed." ),
			APP_NAME, MB_ICONEXCLAMATION | MB_OK );
		return FALSE;
	}

	_fputts( _T( "\r\n\r\n" ), fdebug );
	SYSTEMTIME st;
	GetLocalTime( &st );

	_ftprintf( fdebug, _T( "[ %04d-%02d-%02d %02d:%02d:%02d.%03d PID=%08lx] " ),
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		GetCurrentProcessId() );

	_fputts( _T( "-------- END --------\r\n\r\n\r\n" ), fdebug );
	fclose( fdebug );
	return TRUE;
}


BOOL WriteDebugLog( LPCTSTR str )
{
	if( ! g_bLogging || ! str ) return FALSE;

	FILE * fdebug = NULL;	
	_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	if( fdebug == NULL ) return FALSE;
	if( *str )
	{
		SYSTEMTIME st;
		GetLocalTime( &st );
		_ftprintf( fdebug, _T( "[ %04d-%02d-%02d %02d:%02d:%02d.%03d PID=%08lx ] " ),
			st.wYear, st.wMonth, st.wDay,
			st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
			GetCurrentProcessId()
		);
		_fputts( str, fdebug );
	}
	_fputts( _T( "\r\n" ), fdebug );
	fclose( fdebug );
	return TRUE;
}


