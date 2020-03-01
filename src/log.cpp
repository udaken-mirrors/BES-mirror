/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"
#include <share.h>

TCHAR g_lpszLogFileName[ CCHPATH ];

extern BOOL g_bLogging;

BOOL OpenDebugLog()
{
#ifdef _DEBUG
	OutputDebugString(_T("OpenDebugLog\n"));
#endif
	GetModuleFileName( NULL, g_lpszLogFileName, CCHPATH - 64 );
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
		GetCurrentDirectory( CCHPATH - 64, g_lpszLogFileName );
	}

	TCHAR tmpstr[ 64 ] = TEXT( "" );
	SYSTEMTIME st;
	GetLocalTime( &st );
	_stprintf_s( tmpstr, _countof(tmpstr), _T( "\\bes-%04d%02d%02d.log" ), st.wYear, st.wMonth, st.wDay );


	_tcscat_s( g_lpszLogFileName, CCHPATH, tmpstr );

//	FILE * fdebug = NULL;	
//	_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	FILE * fdebug = _tfsopen( g_lpszLogFileName, _T("ab"), _SH_DENYWR );

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

BOOL VerifyOSVer( DWORD dwMajor, DWORD dwMinor, int iSP )
{
	OSVERSIONINFOEX osvi = {0};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	osvi.dwMajorVersion = dwMajor;
	osvi.dwMinorVersion = dwMinor;
	osvi.wServicePackMajor = (WORD) iSP;
	osvi.wServicePackMinor = (WORD) 0;
	ULONGLONG qwMask = 0;
	qwMask = VerSetConditionMask( qwMask, VER_MAJORVERSION, VER_GREATER_EQUAL );
	qwMask = VerSetConditionMask( qwMask, VER_MINORVERSION, VER_GREATER_EQUAL );
	qwMask = VerSetConditionMask( qwMask, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL );
	qwMask = VerSetConditionMask( qwMask, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL );

	return VerifyVersionInfo( &osvi, 
		VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR,
		(DWORDLONG) qwMask );
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

	_ftprintf( fp, _T( "  %4d-%02d-%02d %02d:%02d:%02d.%03d%s%02d:%02d PID 0x%lx TID 0x%lx\r\n\r\n" ),
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		tz_sign > 0 ? _T( "+" ) : _T( "-" ), tz_h, tz_m,
		GetCurrentProcessId(),
		GetCurrentThreadId() );



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
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "2000" ) );
		else if( osvi.dwMinorVersion == 1UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "XP" ) );
		else if( osvi.dwMinorVersion == 2UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Server 2003 R2, Windows Server 2003, or Windows XP Professional x64 Edition" ) );
	}
	else if( osvi.dwMajorVersion == 6UL )
	{
		if( osvi.dwMinorVersion == 0UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "Vista or Windows Server 2008" ) );
		else if( osvi.dwMinorVersion == 1UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "7 or Windows Server 2008 R2" ) );
		else if( osvi.dwMinorVersion == 2UL )
			_tcscpy_s( tmpstr, _countof(tmpstr), _T( "8, Windows Server 2012, or newer" ) );
	}

	_ftprintf( fp, _T( "Windows %s ( OS Version: %lu.%lu Build %lu%s%s )\r\n" ),
		tmpstr,
		osvi.dwMajorVersion,
		osvi.dwMinorVersion,
		osvi.dwBuildNumber,
		osvi.szCSDVersion[ 0 ] ? _T(", ") : _T(""),
		osvi.szCSDVersion
	);

	if( VerifyOSVer(6, 3, 0) ) _tcscpy_s( tmpstr, _countof(tmpstr), _T("8.1") );
	else if( VerifyOSVer(6, 2, 0) ) _tcscpy_s( tmpstr, _countof(tmpstr), _T("8") );
	else if( VerifyOSVer(6, 1, 0) ) _tcscpy_s( tmpstr, _countof(tmpstr), _T("7") );
	else if( VerifyOSVer(6, 0, 0) ) _tcscpy_s( tmpstr, _countof(tmpstr), _T("Vista") );
	else if( VerifyOSVer(5, 1, 3) ) _tcscpy_s( tmpstr, _countof(tmpstr), _T("XP SP3") );
	_ftprintf( fp, _T( "Verified: Windows %s or newer\r\n" ), tmpstr );

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

	//FILE * fdebug = NULL;	
	//_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	FILE * fdebug = _tfsopen( g_lpszLogFileName, _T("ab"), _SH_DENYWR );
/*	if( fdebug == NULL )
	{
		Sleep( 3000 );
		_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	}*/

	if( ! fdebug )
	{
#ifdef _DEBUG
		OutputDebugString( TEXT( "\tCloseDebugLog() failed!\n" ) );
#endif
		return FALSE;
	}

	_fputts( _T( "\r\n\r\n" ), fdebug );
	SYSTEMTIME st;
	GetLocalTime( &st );

	_ftprintf( fdebug, _T( "[ %04d-%02d-%02d %02d:%02d:%02d.%03d PID 0x%lx TID 0x%lx] " ),
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		GetCurrentProcessId(),
		GetCurrentThreadId() );

	_fputts( _T( "-------- END --------\r\n\r\n\r\n" ), fdebug );
	fclose( fdebug );
	return TRUE;
}


void WriteDebugLog( const TCHAR * str )
{
	if( ! g_bLogging || ! str ) return;

	TCHAR szBuffer[ 512 ];
	SYSTEMTIME st;
	GetLocalTime( &st );
	int icch =
		_sntprintf_s( szBuffer, _countof(szBuffer), _TRUNCATE,
					_T("[ %04d-%02d-%02d %02d:%02d:%02d.%03d P 0x%lx T 0x%lx ] %s\r\n"),
					(int) st.wYear,
					(int) st.wMonth,
					(int) st.wDay,
					(int) st.wHour,
					(int) st.wMinute,
					(int) st.wSecond,
					(int) st.wMilliseconds,
					GetCurrentProcessId(),
					GetCurrentThreadId(),
					str );
	if( icch == -1 )
	{
		icch = _countof(szBuffer) - 1;
	}
#ifdef _DEBUG
	OutputDebugString(szBuffer);
#endif

//	FILE * fdebug = NULL;	
//	_tfopen_s( &fdebug, g_lpszLogFileName, _T( "ab" ) );
	///FILE * fdebug = _tfsopen( g_lpszLogFileName, _T("ab"), _SH_DENYWR );

	LONG lPosHigh = 0L;
	DWORD cbToWrite = (DWORD) icch * sizeof(TCHAR);
	DWORD dwWritten;
	HANDLE hFile = CreateFile( g_lpszLogFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
								OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
	if( hFile != INVALID_HANDLE_VALUE )
	{
		SetFilePointer( hFile, 0L, &lPosHigh, FILE_END );
		WriteFile( hFile, (LPCVOID) szBuffer, cbToWrite, &dwWritten, NULL );
		CloseHandle( hFile );
	}
/*	if( fdebug )
	{
		_fputts( szBuffer, fdebug );
		fclose( fdebug );
	}*/
#ifdef _DEBUG
	else
	{
		OutputDebugString(_T("\t\t\t!fdebug="));
		OutputDebugString(g_lpszLogFileName);
		OutputDebugString(_T("\n"));
		OutputDebugString(str);
		OutputDebugString(_T("\n\n"));
	}
#endif
}


