/* 
 *	Copyright (C) 2005-2019 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

extern BOOL g_bSelNextOnHide;

extern BOOL g_bLogging;

extern volatile int g_Slider[ MAX_SLOTS ];

extern TCHAR * g_lpszEnemy[ MAX_ENEMY_CNT ];
extern ptrdiff_t g_numOfEnemies;
extern TCHAR * g_lpszFriend[ MAX_FRIEND_CNT ];
extern ptrdiff_t g_numOfFriends;
extern volatile UINT g_uUnit;
extern bool g_fRealTime;
extern bool g_fLowerPriv;

static UINT64 ReadIni_Time( const TCHAR * pExe );
static void WriteIni_Worker( TCHAR * lpBuffer, size_t cchBuffer, 
							bool fFriend, const TCHAR * strIniPath );
static void PathToExe_InPlace( TCHAR * pszPath );
static BOOL PathToExeEx( LPTSTR strPath, int iBufferSize );

const TCHAR * GetIniPath( void )
{
	static TCHAR lpszIniFilePath[ MAX_PATH * 2 ] = TEXT("");

	//if( _tcslen( lpszIniFilePath ) == 0 )
	if( *lpszIniFilePath == _T('\0') )
	{
		TCHAR szBuf[ MAX_PATH * 2 ] = TEXT("");

		GetModuleFileName( NULL, szBuf, (DWORD) _countof(szBuf) );

		int len = (int) _tcslen( szBuf );
		
		for( int i = len - 1; i >= 0; i-- )
		{
			if( szBuf[ i ] == TEXT( '\\' ) )
			{
				szBuf[ i ] = TEXT( '\0' );
				_tcscpy_s( lpszIniFilePath, _countof(lpszIniFilePath), szBuf ); // _tcscpy
				break;
			}
		}

		if( lpszIniFilePath[ 0 ] == TEXT( '\0' ) )
		{
			GetCurrentDirectory( (DWORD) _countof(lpszIniFilePath), lpszIniFilePath );
		}

		_tcscat_s( lpszIniFilePath, _countof(lpszIniFilePath), _T( "\\bes.ini" ) );

#ifdef _UNICODE // @2011-12-20
		// To use a Unicode .ini file, we need to put a BOM, not just using ~W functions.
		HANDLE hFile = CreateFile( lpszIniFilePath, GENERIC_READ | GENERIC_WRITE, 0L, NULL,
									OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		if( hFile != INVALID_HANDLE_VALUE )
		{
			const WCHAR bom = L'\xfeff';
			WCHAR wcFirst;
			DWORD cbRead, cbWritten;
			
			const DWORD cbFile = GetFileSize( hFile, NULL );
			if( cbFile == 0L )
			{
				SetFilePointer( hFile, 0L, NULL, FILE_BEGIN ); // just in case
				WriteFile( hFile, &bom, sizeof(bom), &cbWritten, NULL );
				
				// dummy (to avoid a future line-break just after BOM)
				WriteFile( hFile, L"[Options]\r\n", (DWORD)( 11 * sizeof(wchar_t) ), &cbWritten, NULL );
			}
			else if( cbFile != INVALID_FILE_SIZE
						&& ReadFile( hFile, &wcFirst, sizeof(WCHAR), &cbRead, NULL )
						&& cbRead == sizeof(WCHAR)
						&& wcFirst != bom )
			{
				// Convert old INI that doesn't have a BOM
				char * lpAnsi = (char*) HeapAlloc( GetProcessHeap(), 0L, cbFile );
				if( lpAnsi )
				{
					SetFilePointer( hFile, 0L, NULL, FILE_BEGIN );
					if( ReadFile( hFile, lpAnsi, cbFile, &cbRead, NULL ) && cbRead == cbFile )
					{
						int cchWideChar
							= MultiByteToWideChar( CP_ACP, 0L, lpAnsi, (int) cbFile, NULL, 0 );
						if( cchWideChar )
						{
							wchar_t * lpWideChar = (wchar_t*) HeapAlloc( GetProcessHeap(), 0L,
																(size_t) cchWideChar * sizeof(wchar_t) );
							if( lpWideChar )
							{
								if( MultiByteToWideChar( CP_ACP, 0L, lpAnsi, (int) cbFile,
														lpWideChar, cchWideChar ) == cchWideChar )
								{
									SetFilePointer( hFile, 0L, NULL, FILE_BEGIN );
									WriteFile( hFile, &bom, sizeof(bom), &cbWritten, NULL );
									WriteFile( hFile, lpWideChar,
												(DWORD)( cchWideChar * sizeof(wchar_t) ),
												&cbWritten, NULL );
									SetEndOfFile( hFile );
								}

								HeapFree( GetProcessHeap(), 0L, lpWideChar );
							}
						}
					}
					
					HeapFree( GetProcessHeap(), 0L, lpAnsi );
				}
			}

			CloseHandle( hFile );
		}
#endif

	} // if lpszIniFilePath is not initialized

	return lpszIniFilePath;
}



void ReadIni( BOOL& bAllowMulti, BOOL& bLogging )
{
	const TCHAR * strIniPath = GetIniPath();

	bLogging = !! GetPrivateProfileInt( TEXT( "Options" ), TEXT( "Logging" ), FALSE, strIniPath );
	bAllowMulti = !! GetPrivateProfileInt( TEXT( "Options" ), TEXT( "AllowMulti" ), FALSE, strIniPath );
	g_fRealTime = !! GetPrivateProfileInt( TEXT("Options"), TEXT("RealTime"), FALSE, GetIniPath() );
	g_fLowerPriv = !! GetPrivateProfileInt( TEXT("Options"), TEXT("LowerPriv"), FALSE, GetIniPath() );

	g_bSelNextOnHide = !! GetPrivateProfileInt( TEXT( "Options" ), TEXT( "SelChange" ), FALSE, strIniPath );
	const UINT unit
		= GetPrivateProfileInt( TEXT( "Options" ), TEXT( "Unit" ), UNIT_DEF, strIniPath );
	if( UNIT_MIN <= unit && unit <= UNIT_MAX ) g_uUnit = unit;
	else g_uUnit = UNIT_DEF;


//0x04 	LANG_CHINESE
//0x01 	SUBLANG_CHINESE_TRADITIONAL 	Chinese (Traditional)
//0x02 	SUBLANG_CHINESE_SIMPLIFIED 	Chinese (Simplified)
//	WORD wDefLanguage = (WORD) PRIMARYLANGID( GetSystemDefaultLangID() );
	WORD wDefLanguage = (WORD) PRIMARYLANGID( GetUserDefaultLangID() );

	if( wDefLanguage == LANG_CHINESE )
	{
		WORD wSubLangId = (WORD) SUBLANGID( GetSystemDefaultLangID() );
		wDefLanguage = ( wSubLangId == SUBLANG_CHINESE_TRADITIONAL )? LANG_CHINESE_T : LANG_CHINESE_S ;
	}
	else if( wDefLanguage != LANG_ENGLISH && wDefLanguage != LANG_FINNISH && wDefLanguage != LANG_JAPANESE
		&& wDefLanguage != LANG_SPANISH )
	{
		wDefLanguage = LANG_ENGLISH;
	}

	WORD wLanguage = (WORD) GetPrivateProfileInt( TEXT( "Options" ), TEXT( "Language" ), (int) wDefLanguage, strIniPath );

	if( wLanguage != LANG_ENGLISH && wLanguage != LANG_FINNISH && wLanguage != LANG_JAPANESE && wLanguage != LANG_JAPANESEo
		&& wLanguage != LANG_CHINESE_T && wLanguage != LANG_CHINESE_S && wDefLanguage != LANG_SPANISH
		&& wLanguage != LANG_FRENCH )
	{
		wLanguage = wDefLanguage;
	}
	
	SetLanguage( wLanguage );

	g_Slider[ 0 ] = SLIDER_DEF;
	g_Slider[ 1 ] = SLIDER_DEF;
	g_Slider[ 2 ] = SLIDER_DEF;
	g_Slider[ 3 ] = 0;
	for( ptrdiff_t e = 4; e < MAX_SLOTS; ++e ) g_Slider[ e ] = SLIDER_DEF;

	ptrdiff_t i, j;
	TCHAR szKeyName[ 32 ];
	TCHAR szBuffer[ CCHPATH ];
	g_numOfEnemies = 0;
	for( i = 0; i < MAX_ENEMY_CNT; ++i )
	{
		_stprintf_s( szKeyName, _countof(szKeyName), _T("Enemy%d"), (int) i );
		
		if( ! GetPrivateProfileString( TEXT("Enemy"), szKeyName, NULL,
										szBuffer, CCHPATH, strIniPath ) ) break;
		
		if( IsAbsFoe( szBuffer ) || _tcschr( szBuffer, _T('\\') )) continue;

		if( ! PathToExeEx( szBuffer, CCHPATH ) ) continue;

		for( j = 0; j < g_numOfEnemies; ++j )
			if( _tcsicmp( g_lpszEnemy[ j ], szBuffer ) == 0 ) break; // dup
		
		if( j < g_numOfEnemies ) continue;

		size_t cchMem = _tcslen( szBuffer ) + 1;
		
		g_lpszEnemy[ g_numOfEnemies ]
			= (TCHAR *) HeapAlloc( GetProcessHeap(), 0, cchMem * sizeof(TCHAR) );
		if( ! g_lpszEnemy[ g_numOfEnemies ] ) break;

		_tcscpy_s( g_lpszEnemy[ g_numOfEnemies++ ], cchMem, szBuffer );

		if( g_numOfEnemies == MAX_ENEMY_CNT ) break;
	}

	g_numOfFriends = 0;
	for( i = 0; i < MAX_FRIEND_CNT; ++i )
	{
		_stprintf_s( szKeyName, _countof(szKeyName), _T("Friend%d"), (int) i );
		
		if( ! GetPrivateProfileString( TEXT("Friend"), szKeyName, NULL,
										szBuffer, CCHPATH, strIniPath )) break;
//		else if( IsAbsFoe( g_lpszEnemy[ g_numOfFriends ] ) ) continue;
//		else if( _tcschr( g_lpszEnemy[ g_numOfFriends ], _T( '\\' ) ) != NULL ) continue;
		// @20140328
		else if( IsAbsFoe( szBuffer ) || _tcschr( szBuffer, _T('\\') ) ) continue;

		if( ! PathToExeEx( szBuffer, CCHPATH ) ) continue;

		// dup check
		for( j = 0; j < g_numOfFriends; ++j )
			if( _tcsicmp( g_lpszFriend[ j ], szBuffer ) == 0 ) break;
		if( j < g_numOfFriends ) continue;
		
		// if an enemy AND a friend at the same time, consider it as an enemy
		for( j = 0; j < g_numOfEnemies; ++j )
			if( _tcsicmp( g_lpszEnemy[ j ], szBuffer ) == 0 ) break;
		if( j < g_numOfFriends ) continue;

		size_t cchMem = _tcslen( szBuffer ) + 1;
		g_lpszFriend[ g_numOfFriends ]
						= (TCHAR *) HeapAlloc( GetProcessHeap(), 0, cchMem * sizeof(TCHAR));
		if( ! g_lpszFriend[ g_numOfFriends ] ) break;

		_tcscpy_s( g_lpszFriend[ g_numOfFriends++ ], cchMem, szBuffer );

		if( g_numOfFriends == MAX_FRIEND_CNT ) break;
	}

}

BOOL UpdateIFF_Ini( BOOL bCleanSliderSection )
{
	// The maximum profile section size is 32,767 characters. Our cchBuffer is 133,120 cch.
	size_t cchBuffer = CCHPATH * MAX_ENEMY_CNT;
	TCHAR * lpBuffer = (TCHAR*) HeapAlloc( GetProcessHeap(), 0, cchBuffer * sizeof(TCHAR) );
	if( ! lpBuffer ) return FALSE;

	const TCHAR * strIniPath = GetIniPath();
	WriteIni_Worker( lpBuffer, cchBuffer, false, strIniPath );
	WriteIni_Worker( lpBuffer, cchBuffer, true, strIniPath );
	HeapFree( GetProcessHeap(), 0, lpBuffer );
	lpBuffer = NULL;

	// 2019-05-23 Resave the [Time] section and delete "unknown" entries (not enemy nor friend).
	const ptrdiff_t cEntries = g_numOfEnemies + g_numOfFriends;
	cchBuffer = (size_t)( cEntries * ( CCHPATH + 64 ) );
	lpBuffer = (TCHAR*) HeapAlloc( GetProcessHeap(), 0, cchBuffer * sizeof(TCHAR) );

	if( ! lpBuffer ) return FALSE;

	TCHAR * ptr = lpBuffer;
	TCHAR szKeyName[ 64 ];
	TCHAR szExe[ CCHPATH ];
	UINT64 u64;
	int icch;
	BOOL bOK = TRUE;

	for( ptrdiff_t i = 0; i < g_numOfEnemies && bOK; ++i )
	{
		_stprintf_s( szKeyName, _countof(szKeyName), _T("Enemy%d"), (int) i );
		if( GetPrivateProfileString( TEXT("Enemy"), szKeyName, NULL, szExe, CCHPATH, strIniPath ) )
		{
			u64 = ReadIni_Time( szExe );
			icch = _stprintf_s( ptr, cchBuffer - ( ptr - lpBuffer ), _T("%s=%I64u"), szExe, u64 );
			if( 0 < icch ) ptr += ( icch + 1 );
			else bOK = FALSE;
		}
	}

	for( ptrdiff_t i = 0; i < g_numOfFriends && bOK; ++i )
	{
		_stprintf_s( szKeyName, _countof(szKeyName), _T("Friend%d"), (int) i );
		if( GetPrivateProfileString( TEXT("Friend"), szKeyName, NULL, szExe, CCHPATH, strIniPath ) )
		{
			u64 = ReadIni_Time( szExe );
			icch = _stprintf_s( ptr, cchBuffer - ( ptr - lpBuffer ), _T("%s=%I64u"), szExe, u64 );
			if( 0 < icch ) ptr += ( icch + 1 );
			else bOK = FALSE;
		}
	}

	if( bOK )
	{
		if( cchBuffer - ( ptr - lpBuffer ) >= 2 )
		{
			ptr[ 0 ] = _T('\0');
			ptr[ 1 ] = _T('\0');
			bOK = WritePrivateProfileSection( TEXT("Time"), lpBuffer, strIniPath );
		}
		else bOK = FALSE;
	}

	HeapFree( GetProcessHeap(), 0, lpBuffer );
	lpBuffer = NULL;

	if( ! bCleanSliderSection || ! bOK ) return bOK;

	// Resave the [Slider] section, deleting non-timed entries (except the special key "Slider2").
	cchBuffer = CCHPATH * MAX_ENEMY_CNT;
	lpBuffer = (TCHAR*) HeapAlloc( GetProcessHeap(), 0, cchBuffer * sizeof(TCHAR) );
	if( ! lpBuffer ) return FALSE;

	DWORD dwcch = GetPrivateProfileString( TEXT("Slider"), NULL, NULL, lpBuffer, (DWORD) cchBuffer, strIniPath );
	if( 0 < dwcch && dwcch < (DWORD) cchBuffer - 2 )
	{
		ptr = lpBuffer;
		while( *ptr ) {
			if( ReadIni_Time( ptr ) == (UINT64) 0 && _tcscmp( ptr, _T("Slider2") ) != 0 ) {
				WritePrivateProfileString( TEXT("Slider"), ptr, NULL, strIniPath );
			}
			ptr += _tcslen( ptr ) + 1;
		}
	}
	else bOK = FALSE;

	HeapFree( GetProcessHeap(), 0, lpBuffer );
	lpBuffer = NULL;

	return bOK;
}

BOOL WriteIni( bool fRealTime )
{
	const TCHAR * strIniPath = GetIniPath();

	// Delete old keys/values
	WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider0" ),	NULL, strIniPath );
	WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider1" ),	NULL, strIniPath );
	// The key "Slider2" will be removed if ver <= v1.5.1; v1.5.2 uses this key as the "clean" marker.
	//WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider2" ),	NULL, strIniPath );
	//WritePrivateProfileString( TEXT( "Options" ), TEXT( "RealTime" ), NULL, strIniPath );

	// The maximum profile section size is 32,767 characters. Our cchBuffer is 133,120 cch.
	size_t cchBuffer = CCHPATH * MAX_ENEMY_CNT;
	TCHAR * lpBuffer = (TCHAR*) HeapAlloc( GetProcessHeap(), 0L, cchBuffer * sizeof(TCHAR) );

	if( lpBuffer && GetPrivateProfileInt( _T("Slider"), _T("Slider2"), 0, strIniPath ) < 1520 )
	{
		GetPrivateProfileSection( _T("Slider"), lpBuffer, cchBuffer, strIniPath );
		TCHAR * line = lpBuffer;
		while( *line )
		{
			TCHAR * const separator = _tcsrchr( line, _T('=') );
			if( ! separator )
				break;
			
			if( 11 < separator - line
				&& ! memcmp( separator - 11, _T(" (watching)"), 11 * sizeof(TCHAR) ) )
			{
				*separator = _T('\0');
				WritePrivateProfileString( _T("Slider"), line, NULL, strIniPath );
				*separator = _T('=');

				*( separator - 11 ) = _T('\0');
				if( ! GetPrivateProfileInt( _T("Slider"), line, 0, strIniPath ) )
					WritePrivateProfileString( _T("Slider"), line, separator + 1, strIniPath );
				*( separator - 11 ) = _T(' ');
			}

			line += _tcslen( line ) + 1;
		}
		WritePrivateProfileString( _T("Slider"), _T("Slider2"), _T("1520"), strIniPath );
	}

	if( lpBuffer ) // no early return if ! lpBuffer - FIX @ 2019-05-20
	{
		HeapFree( GetProcessHeap(), 0L, lpBuffer );
		lpBuffer = NULL;
	}

	// Read in ReadIni
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "RealTime" ),
		fRealTime ? TEXT( "1" ) : TEXT( "0" ),
		strIniPath
	);

	TCHAR strUnit[ 32 ]; // Read in ReadIni
	_stprintf_s( strUnit, _countof(strUnit), _T( "%u" ), g_uUnit );
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "Unit" ),
		strUnit,
		strIniPath
	);

	TCHAR lpszLangId[ 100 ]; // Read in ReadIni
	_stprintf_s( lpszLangId, _countof(lpszLangId), _T( "%d" ), (int) GetLanguage() );
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "Language" ),
		lpszLangId,
		strIniPath
	);

	BOOL bRet = UpdateIFF_Ini( TRUE );

	// to flush the cache - TODO: do we need this?
	WritePrivateProfileString(
		NULL,
		NULL,
		NULL,
		strIniPath
	);

	return bRet;
}

void FreePointers( void )
{
	for( ptrdiff_t i = 0; i < MAX_ENEMY_CNT /*==MAX_FRIEND_CNT*/; ++i )
	{
		if( g_lpszEnemy[ i ] )
		{
			HeapFree( GetProcessHeap(), 0, g_lpszEnemy[ i ] );
			g_lpszEnemy[ i ] = NULL;
		}
		
		if( g_lpszFriend[ i ] )
		{
			HeapFree( GetProcessHeap(), 0, g_lpszFriend[ i ] );
			g_lpszFriend[ i ] = NULL;
		}
	}
}

static void _bes_lower( const TCHAR * p, TCHAR * q, ptrdiff_t cchBuf )
{
	ptrdiff_t len = (ptrdiff_t) _tcslen( p );
	if( cchBuf <= len )
		len = cchBuf - 1;

	const TCHAR * pEnd = p + len;
	while( p < pEnd )
	{
		if( _istascii( (UTchar_) *p ) && _istupper( (UTchar_) *p ) )
			*q = (TCHAR) _totlower( *p );
		else if( *p == _T('=') ) // escape '=' as ':'
			*q = _T(':');
		else if( *p == _T('[') ) // escape '[' as '<' @20140404 v1.7.0.21
			*q = _T('<');
		else if( *p == _T(']') ) // escape ']' as '>' @20140404 v1.7.0.21
			*q = _T('>');
		else
			*q = *p;

		++p;
		++q;
	}
	*q = _T('\0');
}


static void GetCurrentTimeAsString( TCHAR * strTime, size_t cchTime ) {
	FILETIME ft = {0};
	GetSystemTimeAsFileTime( &ft );
	_stprintf_s( strTime, cchTime, _T("%I64u"), 
				(UINT64) ft.dwLowDateTime | (UINT64) ft.dwHighDateTime << 32 );
}

// v0.1.6.0 (20120427) : We only handle up to MAX_PROCESS_CNT (256) items.
// Therefore, we should somehow forget old friends/enemies.
// So, we remember how old they are.
void WriteIni_Time( const TCHAR * pszPath )
{
	if( ! pszPath ) return;

	const TCHAR * pExe = _tcsrchr( pszPath, _T('\\') );
	if( pExe ) ++pExe;
	else pExe = pszPath;

	if( _tcschr( pExe, _T('*') ) || ! pExe[ 0 ] ) return;

	TCHAR strExeNameLower[ CCHPATH ] = _T("");
	_bes_lower( pExe, strExeNameLower, CCHPATH );

	TCHAR strTime[ 64 ] = _T("");
	GetCurrentTimeAsString( strTime, _countof(strTime) );
	
	WritePrivateProfileString(
		TEXT( "Time" ), 
		strExeNameLower,
		strTime,
		GetIniPath()
	);
}
static UINT64 ReadIni_Time( const TCHAR * pExe )
{
	TCHAR strExeNameLower[ CCHPATH ];
	_bes_lower( pExe, strExeNameLower, CCHPATH );

	TCHAR strTime[ 64 ] = _T("");
	GetPrivateProfileString( TEXT("Time"), strExeNameLower, TEXT("0"), 
								strTime, (DWORD) _countof(strTime), GetIniPath() );
	
	// Technically, _tcstoui64 is better, but VC6 doesns't have it.
	// In reality, this should be just fine. At least we can go to the year 30827.
	return (UINT64) _ttoi64( strTime );
}
typedef struct tagItemSorter {
	UINT64 time;
	int number;
#ifdef _DEBUG
	const TCHAR * lpsz;
#else
	int dummy;
#endif
} ITEM_SORTER, *LPITEM_SORTER;
static int item_comp( const void * pv1, const void * pv2 )
{
	const ITEM_SORTER& item1 = *static_cast<const ITEM_SORTER *>( pv1 );
	const ITEM_SORTER& item2 = *static_cast<const ITEM_SORTER *>( pv2 );
	// time: bigger = more recent = more important = before
	if( item1.time > item2.time )
		return -1;
	else if( item1.time < item2.time )
		return +1;
	else // number: smaller = older = more important = before; item1<item2 means (-1)
		return item1.number - item2.number;
}

static void WriteIni_Worker( TCHAR * lpBuffer, size_t cchBuffer, 
							bool fFriend, const TCHAR * strIniPath )
{
	const TCHAR * strSection = fFriend ? _T("Friend") : _T("Enemy") ;
	//const TCHAR (*list)[ MAX_PATH * 2 ] = fFriend ? g_lpszFriend : g_lpszEnemy ;
	const TCHAR * const * list = fFriend ? g_lpszFriend : g_lpszEnemy ;

	const ptrdiff_t cItems = fFriend ? g_numOfFriends : g_numOfEnemies ;
	TCHAR * ptr = lpBuffer;

	if( cItems )
	{
		LPITEM_SORTER sorter = (LPITEM_SORTER) HeapAlloc( GetProcessHeap(), 0, 
															sizeof(ITEM_SORTER) * (SIZE_T) cItems );
		if( ! sorter )
			return;

		ptrdiff_t i = 0;
		for( ; i < cItems; ++i )
		{
			sorter[ i ].number = (int) i;
			sorter[ i ].time = ReadIni_Time( list[ i ] );
#ifdef _DEBUG
			sorter[ i ].lpsz = list[ i ];
#endif
		}
		qsort( sorter, (size_t) cItems, sizeof(ITEM_SORTER), &item_comp );


#if (MAX_ENEMY_CNT < 256) || (MAX_FRIEND_CNT != MAX_ENEMY_CNT)
# error MAX_ENEMY_CNT must be 256 or greater, and equal to MAX_FRIEND_CNT
#endif

		// Save only a limited number of items after sorted
#ifdef _DEBUG
		const ptrdiff_t numOfItemsToSave = __min( 7, cItems );
#else
		const ptrdiff_t numOfItemsToSave = __min( MAX_ENEMY_CNT - 56, cItems );
#endif

		//_CrtSetDebugFillThreshold(0);
		for( i = 0; i < numOfItemsToSave; ++i )
		{
			const ptrdiff_t icch = _stprintf_s( ptr,
												cchBuffer - ( ptr - lpBuffer ),
												_T("%s%d=%s"),
												strSection,
												(int) i,
												list[ sorter[ i ].number ] );
			if( 0 < icch )
				ptr += ( icch + 1 );
			else
				break;
		}

		HeapFree( GetProcessHeap(), 0L, sorter );
	}

	if( cchBuffer - ( ptr - lpBuffer ) >= 2 )
	{
		ptr[ 0 ] = _T('\0'); // additional NUL (after the previous NUL)
		ptr[ 1 ] = _T('\0'); // in case the previous NUL doesn't exist (ptr == lpBuffer)
		WritePrivateProfileSection( strSection, lpBuffer, strIniPath );
	}
}


void SetSliderIni( const TCHAR * pszString, LRESULT iSlider )
{
	if( iSlider < SLIDER_MIN || iSlider > SLIDER_MAX || ! pszString ) return;

	WriteDebugLog( TEXT( "SetSliderIni" ) );
	WriteDebugLog( pszString );

	TCHAR strExeName[ CCHPATH ] = _T( "" );
// +1.1b7
	PathToExe( pszString, strExeName, _countof(strExeName) );
	if( strExeName[ 0 ] == _T('\0')
		|| _tcschr( strExeName, _T('*') ) != NULL
		|| _tcsnicmp( strExeName, _T("pid:"), 4 ) == 0 ) return;

#if 0 //!defined( _UNICODE ) // ANSIFIX6
	if( strlen( strExeName ) >= 19 )
	{
		strExeName[ 15 ] = '\0';
		PathToExeEx( strExeName, MAX_PATH * 2 );
	}
#endif

	TCHAR strExeNameLower[ CCHPATH ];
	_bes_lower( strExeName, strExeNameLower, CCHPATH );

	const TCHAR * strIniPath = GetIniPath();

	TCHAR strNumber[ 16 ];
	_stprintf_s( strNumber, _countof(strNumber), _T("%d"), (int) iSlider );

	WritePrivateProfileString(
		TEXT( "Slider" ), 
		strExeNameLower,
		strNumber,
		strIniPath
	);

	// to flush the cache - TODO: do we need this?
	WritePrivateProfileString(
		NULL,
		NULL,
		NULL,
		strIniPath
	);
}

int GetSliderIni( LPCTSTR lpszTargetPath, HWND hWnd, int iDef )
{
	TCHAR strExeName[ CCHPATH ] = _T("");
	TCHAR strExeNameLower[ CCHPATH ] = _T("");

	if( iDef < SLIDER_MIN || SLIDER_MAX < iDef ) iDef = SLIDER_DEF;

	if( ! lpszTargetPath ) return iDef;

	const TCHAR * pBkSlash = _tcsrchr( lpszTargetPath, _T('\\') );
	_tcscpy_s( strExeName, _countof(strExeName), pBkSlash ? pBkSlash + 1 : lpszTargetPath );

#if 0 // ANSIFIX7
	if( strlen( strExeName ) >= 19 )
	{
		strExeName[ 15 ] = '\0';
		PathToExeEx( strExeName, CCHPATH );
	}
#endif

	_bes_lower( strExeName, strExeNameLower, CCHPATH );

	const TCHAR * strIniPath = GetIniPath();

	int iSlider = (int) GetPrivateProfileInt( TEXT( "Slider" ), strExeNameLower, iDef, strIniPath );
	if( iSlider == 0 ) iSlider = 1; // for backward compat.
	else if( iSlider < SLIDER_MIN || SLIDER_MAX < iSlider ) iSlider = iDef;
	else if( 99 < iSlider )
	{
		UINT uMenuState = GetMenuState( GetMenu( hWnd ), IDM_ALLOWMORETHAN99, MF_BYCOMMAND );
		if( !( uMenuState & MFS_CHECKED ) )
			iSlider = 99;
	}
	return iSlider;
}

VOID SetWindowPosIni( HWND hWnd )
{
	RECT rect;
	if( ! GetWindowRect( hWnd, &rect ) ) return;

	const TCHAR * strIniPath = GetIniPath();

	TCHAR lptstrX[ 100 ];
	TCHAR lptstrY[ 100 ];

	int x = (int) rect.left;
	if( x < 0 ) x = 0;
	int y = (int) rect.top;
	if( y < 0 ) y = 0;
	_stprintf_s( lptstrX, _countof(lptstrX), _T( "%d" ), x );
	_stprintf_s( lptstrY, _countof(lptstrY), _T( "%d" ), y );

	WritePrivateProfileString(
		TEXT( "Window" ), 
		TEXT( "posX" ),
		lptstrX,
		strIniPath
	);

	WritePrivateProfileString(
		TEXT( "Window" ), 
		TEXT( "posY" ),
		lptstrY,
		strIniPath
	);
}

VOID GetWindowPosIni( POINT * ppt, RECT * prcWin )
{
	RECT area = {0};
	SystemParametersInfo( SPI_GETWORKAREA, 0, &area, 0 );

	const TCHAR * strIniPath = GetIniPath();
	LONG x = (LONG) GetPrivateProfileInt( TEXT( "Window" ), TEXT( "PosX" ), area.left + 20, strIniPath );
    LONG y = (LONG) GetPrivateProfileInt( TEXT( "Window" ), TEXT( "PosY" ), area.top + 20, strIniPath );

	SetRect( prcWin, 0, 0, 640, 480 );
	AdjustWindowRect( prcWin, WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE, TRUE );
	LONG maxX = area.right - ( prcWin->right - prcWin->left );
	LONG maxY = area.bottom - ( prcWin->bottom - prcWin->top );

	if( x != CW_USEDEFAULT )
	{
		if( x > maxX ) x = maxX;
		if( x < area.left ) x = area.left;
	}
	
	if( y != CW_USEDEFAULT )
	{
		if( y > maxY ) y = maxY;
		if( y < area.top ) y = area.top;
	}

	ppt->x = x, ppt->y = y;
}



BOOL GetSWIniPath( LPTSTR strIniPath, size_t cchIniPath );
VOID SaveCmdShow( HWND hWnd, int nCmdShow )
{
	TCHAR lpIniPath[ MAX_PATH * 2 ] = TEXT( "" );
	if( ! GetSWIniPath( lpIniPath, _countof(lpIniPath) ) )
	{
		GetTempPath( MAX_PATH, lpIniPath );
		_tcscat_s( lpIniPath, _countof(lpIniPath), _T( "bes_sw.ini" ) );
	}

	TCHAR strHwnd[ 100 ];
	_stprintf_s( strHwnd, _countof(strHwnd), _T( "%p" ), hWnd );
	TCHAR strCmdShow[ 100 ];
	_stprintf_s( strCmdShow, _countof(strCmdShow), _T( "%d" ), nCmdShow );

	WritePrivateProfileString(
		TEXT( "nCmdShow" ),
		strHwnd,
		( nCmdShow == BES_DELETE_KEY )? NULL : strCmdShow,
		lpIniPath
	);

}

BOOL GetSWIniPath( LPTSTR strIniPath, size_t cchIniPath )
{
	_tcscpy_s( strIniPath, cchIniPath, GetIniPath() );

	int len = (int) _tcslen( strIniPath );
	int i = len - 1;
	for( ; i >= 0; i-- )
	{
		if( strIniPath[ i ] == _T( '\\' ) )
		{
			strIniPath[ i + 1 ] = _T( '\0' );
			break;
		}
	}

	if( i == 0 || i == len - 1 )
		return FALSE;

	_tcscat_s( strIniPath, cchIniPath, _T( "bes_sw.ini" ) );
	return TRUE;
}


int GetCmdShow( HWND hWnd )
{
	TCHAR strIniPath[ MAX_PATH * 2 ] = TEXT("");
	if( ! GetSWIniPath( strIniPath, _countof(strIniPath) ) )
	{
		GetTempPath( MAX_PATH, strIniPath );
		_tcscat_s( strIniPath, _countof(strIniPath), _T( "bes_sw.ini" ) );
	}

	TCHAR strHwnd[ 100 ];
	_stprintf_s( strHwnd, _countof(strHwnd), _T( "%p" ), hWnd );
	return (int) GetPrivateProfileInt(
		TEXT( "nCmdShow" ),
		strHwnd,
		BES_ERROR, // if not found
		strIniPath
	);
}

VOID InitSWIni( VOID )
{
	TCHAR lpIniPath[ MAX_PATH * 2 ] = TEXT("");
	if( ! GetSWIniPath( lpIniPath, _countof(lpIniPath) ) )
	{
		GetTempPath( MAX_PATH, lpIniPath );
		_tcscat_s( lpIniPath, _countof(lpIniPath), _T( "bes_sw.ini" ) );
	}

	LARGE_INTEGER liCount, liFreq;
	if( QueryPerformanceCounter(&liCount) && QueryPerformanceFrequency(&liFreq) )
	{
		LONGLONG secTick = liCount.QuadPart / liFreq.QuadPart;
		FILETIME ft;
		GetSystemTimeAsFileTime( &ft );
		LONGLONG secNow
			= ( (LONGLONG) ft.dwHighDateTime << 32 | ft.dwLowDateTime ) / 10000000
			- 11644473600i64; // ft @ 1970-01-01, like C "time"
		LONGLONG secBoot = secNow - secTick;

		TCHAR strPrevBoot[ 64 ] = _T("");
		GetPrivateProfileString( _T("Signature"), _T("Boot64"), _T("0"),
								strPrevBoot, (DWORD) _countof(strPrevBoot), lpIniPath );
		
		LONGLONG secPrevBoot = _ttoi64( strPrevBoot );

		if( 16 < secBoot - secPrevBoot || secBoot - secPrevBoot < -16 )
		{
			if( secPrevBoot != 0 )
			{
				WritePrivateProfileString( _T( "nCmdShow" ), NULL, NULL, lpIniPath );
				WritePrivateProfileString( _T("Signature"), _T("Boot"), NULL, lpIniPath );
			}

			TCHAR strBoot[ 64 ];
			_stprintf_s( strBoot, _countof(strBoot), _T("%I64d"), secBoot );
			WritePrivateProfileString( _T("Signature"), _T("Boot64"), strBoot, lpIniPath );
		}

		return;
	}

	int iPrevBootTime = (int) GetPrivateProfileInt( TEXT( "Signature" ), TEXT( "Boot" ), 0, lpIniPath );

	int iNow = (int) (time_t) time( ( time_t * ) NULL );

	int iBootTime = iNow - (int)( GetTickCount() / 1000UL );


//	if( abs( iBootTime - iPrevBootTime ) > 4 ) // -v1.3.8
	if( abs( iBootTime - iPrevBootTime ) > 16 )
	{
		TCHAR strBootTime[ 100 ];
		_stprintf_s( strBootTime, _countof(strBootTime), _T( "%u" ), (UINT) iBootTime );

		WritePrivateProfileString(
			TEXT( "Signature" ),
			TEXT( "Boot" ),
			strBootTime,
			lpIniPath
		);

		WritePrivateProfileString( TEXT( "nCmdShow" ), NULL, NULL, lpIniPath );
	}
}



static void PathToExe_InPlace( TCHAR * pszPath )
{
	/*
	C:\path\to\some.exeZ
	1234567890123456789  : len = 19
	012345678901234567*  : strPath[ i ] for i = len - 1 = 18
	0123456789*          : strPath[ 10 ] == _T('\\') --> i = 10
	           123456789 : cchToCopy = len - i  = 19 - 10 = 9 (incl. term NUL)
	
	
	*/

  	/*int len = (int) _tcslen( strPath );
	for( int i = len - 1; i >= 0; i-- )
	{
		if( strPath[ i ] == TEXT( '\\' ) )
		{
			//lstrcpy( strPath, &strPath[ i + 1 ] ); // @fix 20111207
			memmove( strPath, &strPath[ i + 1 ], (size_t)( len - i ) * sizeof(TCHAR) );
			break;
		}
	}*/
	//OutputDebugStringW(L"\r\n[in] ");
	//OutputDebugStringW(strPath);
	const TCHAR * pntr = _tcsrchr( pszPath, _T('\\') );
	if( pntr && pntr[ 1 ] )
		memmove( pszPath, pntr + 1, ( _tcslen( pntr + 1 ) + 1 ) * sizeof(TCHAR) );

	//OutputDebugStringW(L"\r\n[out] ");
	//OutputDebugStringW(strPath);
	//OutputDebugStringW(L"\r\n");

}


static BOOL PathToExeEx( LPTSTR strPath, int iBufferSize )
{
	PathToExe_InPlace( strPath );

	const int len = (int) _tcslen( strPath );

	if(	len < 15 || _tcsicmp( &strPath[ len - 4 ], _T( ".exe" ) ) == 0	)
	{
		return TRUE;
	}

	int s = 0;
	if(	_tcsicmp( &strPath[ len - 3 ], _T(".ex") ) == 0 )
	{
		s = 3;
	}
	else if( strPath[ len - 2 ] == _T('.')
				&& ( strPath[ len - 1 ] == _T('e') || strPath[ len - 1 ] == _T('E') ) )
	{
		s = 2;
	}
	else if( strPath[ len - 1 ] == _T('.') )
	{
		s = 1;
	}

	if( s )
	{
		--s;
		_tcscpy_s( &strPath[ len - s ], (rsize_t)( iBufferSize - ( len - s ) ), _T("exe") );
		return TRUE;
	}

	return ( _tcsncat_s( strPath, (rsize_t) iBufferSize, _T( "~.exe" ), _TRUNCATE ) == 0 );
}
