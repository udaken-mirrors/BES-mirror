/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#if 0
#include "BattleEnc.h"
#define MAX_SSTP 1024 * 5

static HWND GetSakuraHwnd( LPCTSTR lpszAgentName );

BOOL DirectSSTP( HWND hWnd, const char * hcSakura, const char * hcKero, const char * hcCharset )
{
	TCHAR lpszAgentName[ 128 ] = TEXT( "ssp" );
	HANDLE hMutex = OpenMutex( MUTEX_ALL_ACCESS, FALSE, lpszAgentName );
	if( hMutex == NULL )
	{
//		WriteDebugLog( TEXT( "SSTP : SSP Not found." ) );
		_tcscpy_s( lpszAgentName, _countof(lpszAgentName), _T( "sakura" ) );
		hMutex = OpenMutex( MUTEX_ALL_ACCESS, FALSE, lpszAgentName );
		if( hMutex == NULL )
		{
//			WriteDebugLog( TEXT( "SSTP : Sakura Not found." ) );
			return FALSE;
		}
		else
		{
//			WriteDebugLog( TEXT( "SSTP : Sakura found!" ) );
			CloseHandle( hMutex );
		}
	} 
	else
	{
//		WriteDebugLog( TEXT( "SSTP : SSP found!" ) );
		CloseHandle( hMutex );
	}

	HWND hwndSakura = GetSakuraHwnd( lpszAgentName );
	if( hwndSakura == NULL )
	{
		return FALSE;
	}

	SYSTEMTIME st;
	GetSystemTime( &st );
	int wSurface = ( (int) st.wMinute * 60 + (int) st.wSecond ) % 9;

	char hcSSTP[ MAX_SSTP + 1024 ];
	int cbWritten = sprintf_s( hcSSTP, _countof(hcSSTP),
		"SEND SSTP/1.4\r\n"
		"Sender: BES\r\n"
		"Script: \\c\\0\\s[%d]%hs\\_w[500]\\1\\s[10]%hs\\e\r\n"
		"Option: nodescript\r\n"
#ifdef _WIN64
		"HWnd: %I64u\r\n"
#else
		"HWnd: %lu\r\n"
#endif
		"Charset: %s\r\n"
		"\r\n",
			wSurface,
			hcSakura,
			hcKero,
			(ULONG_PTR) hWnd,
			hcCharset
		);

	if( 0 < cbWritten )
	{
		COPYDATASTRUCT cds;
		cds.dwData = 9801UL;
		cds.cbData = (DWORD) cbWritten;
		cds.lpData = (PVOID) hcSSTP;
		
		SendMessage( hwndSakura, WM_COPYDATA, (WPARAM) hWnd, (LPARAM) &cds );
	}
	return TRUE;
}



/*
BOOL DirectSSTP( HWND hWnd, const char * hcSakura, const char * hcKero )
{
	return DirectSSTP( hWnd, hcSakura, hcKero, "Shift_JIS" );
}


BOOL DirectSSTP( HWND hWnd, const char * hcScript )
{
	return DirectSSTP( hWnd, hcScript, "", "Shift_JIS" );
}

VOID DirectSSTP( LPCTSTR str )
{
	static HWND hWnd;
	if( ! hWnd ) hWnd = FindWindow( APP_CLASS, NULL );
	if( ! hWnd ) return;
	if( _tcslen( str ) > 500 ) return;
	char lpszScriptA[ 1024 ];

#ifdef _UNICODE
	char lpszBufferA[ 1024 ] = "";
	BOOL bError = FALSE;
	WideCharToMultiByte(
		CP_ACP,
		WC_NO_BEST_FIT_CHARS,
		str,
		-1,
		lpszBufferA,
		1023,
		NULL,
		&bError
	);
	if( bError ) return;
	sprintf_s( lpszScriptA, _countof(lpszScriptA), "\\_q[ BES Debugger ]\\n\\n%s", lpszBufferA );
#else
	sprintf_s( lpszScriptA, _countof(lpszScriptA), "\\_q[ BES Debugger ]\\n\\n%s", str );
#endif
	DirectSSTP( hWnd, lpszScriptA, "", "Shift_JIS" );
}
*/

static HWND GetSakuraHwnd( LPCTSTR lpszAgentName )
{
	HANDLE hMutex = OpenMutex( MUTEX_ALL_ACCESS, FALSE, lpszAgentName );
	if ( hMutex == NULL )
	{
		return NULL;
	} 
	CloseHandle(hMutex);

	HANDLE hMapFile = OpenFileMapping( FILE_MAP_READ, FALSE, TEXT( "Sakura" ) );

	if( hMapFile == NULL )
	{
		return NULL;
	}

	char * pBuf = (char *) MapViewOfFile( hMapFile, FILE_MAP_READ, 0, 0, 0 );
	if( pBuf == NULL )
	{
		CloseHandle( hMapFile );
		return NULL;
	}

	HWND hwndSakura = NULL;
	__try
	{
		MEMORY_BASIC_INFORMATION memInfo = {0};
		VirtualQuery( pBuf, &memInfo, sizeof(memInfo) );
		if( 256 < memInfo.RegionSize )
		{
			const char * p = pBuf + 4;
			const char * pEnd = p + memInfo.RegionSize - 32; // p+6+21 must be valid
			while( p < pEnd )
			{
				if( *p == '.' && memcmp( p + 1, "hwnd\x01", 5 ) == 0 )
				{
#ifdef _WIN64
					if( memchr( p + 6, '\r', 20 + 1 ) )
						hwndSakura = (HWND)(INT_PTR) _atoi64( p + 6 );
#else
					if( memchr( p + 6, '\r', 10 + 1 ) )
						hwndSakura = (HWND)(INT_PTR) atoi( p + 6 );
#endif
					break;
				}
				++p;
			}
		}
	}
	__except( GetExceptionCode() == EXCEPTION_IN_PAGE_ERROR
				? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH )
	{
		;
	}

	UnmapViewOfFile( pBuf );
	CloseHandle( hMapFile );
	return hwndSakura;

/*
	char hcSakuraHwnd[ 128 ] = "";
	for( int p = 0; p < 1000; p++ )
	{
		if( pBuf[ p ] == 'h' &&
			pBuf[ p+1 ] == 'w' &&
			pBuf[ p+2 ] == 'n' &&
			pBuf[ p+3 ] == 'd' &&
			pBuf[ p+4 ] == 0x01 )
		{
			for( int q = p + 5; pBuf[ q ] >= '0' && pBuf[ q ] <= '9'; q++ )
			{
				char tmpstr[ 10 ];
				wsprintfA( tmpstr, "%hc", pBuf[ q ] );
				lstrcatA( hcSakuraHwnd, tmpstr );
			}
			break;
		}
	}

	UnmapViewOfFile( pBuf );
	CloseHandle( hMapFile );

	if( lstrcmpA( hcSakuraHwnd, "" ) == 0 )
	{
		return NULL;
	}

	return (HWND)(INT_PTR) _atoi64( hcSakuraHwnd );
*/
}


BOOL SSTP_Aviutl( HWND hWnd, LPCTSTR lpTitle )
{
#define SSTP_AVIUTL_BUF_SIZE 1024

	if( (int) _tcslen( lpTitle ) > SSTP_AVIUTL_BUF_SIZE / 2 ) return FALSE;

	if( LANG_JAPANESE != PRIMARYLANGID( GetSystemDefaultLangID() ) ) return FALSE;

	SYSTEMTIME st;
	GetSystemTime( &st );
	if( (int) st.wMilliseconds > 350 ) return FALSE;


	char lpSjisStr[ SSTP_AVIUTL_BUF_SIZE ] = "";
	char lpSjisSakuraScript[ SSTP_AVIUTL_BUF_SIZE ] = "";

#ifdef _UNICODE
	BOOL bError = FALSE;
	WideCharToMultiByte(
		CP_SJIS,
		WC_NO_BEST_FIT_CHARS,
		lpTitle,
		-1,
		lpSjisStr,
		SSTP_AVIUTL_BUF_SIZE,
		"",
		&bError
	);
	if( bError ) return FALSE;
#else
	strcpy_s( lpSjisStr, _countof(lpSjisStr), lpTitle );
#endif


	char * p = strchr( lpSjisStr, '%' );
	char * q = NULL;
	if( p == NULL )
	{
		size_t len = strlen( lpSjisStr );
		p = strstr( lpSjisStr, " (" );
		if( p == NULL ) return FALSE;
		*p = '\0';
		if( strlen( lpSjisStr ) == len ) return FALSE;
		q = strchr( p + 1 , '/' );
		if( q == NULL ) return FALSE;

		int iFrames = atoi( q + 1 );
		if( iFrames == 0 ) return FALSE;
		int s1 = (int) floor( iFrames * 1001.0 / 30000.0 );
		int m1 = s1 / 60;
		s1 -= ( m1 * 60 );
		int s2 = (int) floor( iFrames * 1001.0 / 24000.0 );
		int m2 = s2 / 60;
		s2 -= ( m2 * 60 );
		sprintf_s( lpSjisSakuraScript, _countof(lpSjisSakuraScript), SSTP_AVIUTL_0, lpSjisStr, iFrames,
			m1, s1, m2, s2
		);
	}
	else
	{
		p = strstr( lpSjisStr, "] " );
		if( p == NULL ) return FALSE;
		q = strchr( lpSjisStr, '/' );
		if( q == NULL )	return FALSE;

		int iFrames = atoi( q + 1 );
		if( iFrames == 0 ) return FALSE;
		sprintf_s( lpSjisSakuraScript, _countof(lpSjisSakuraScript), SSTP_AVIUTL_1, p + 2, iFrames );
	}
	
	return DirectSSTP( hWnd, lpSjisSakuraScript, "", "Shift_JIS" );
}

#endif
