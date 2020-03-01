#include "BattleEnc.h"

extern BOOL g_bSelNextOnHide;

extern BOOL g_bLogging;

extern volatile int g_Slider[ 4 ];

extern TCHAR g_lpszEnemy[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
extern int g_numOfEnemies;
extern TCHAR g_lpszFriend[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
extern int g_numOfFriends;
extern volatile UINT g_uUnit;


static void WriteIni_Worker( TCHAR * lpBuffer, size_t cchBuffer, 
							bool fFriend, const TCHAR * strIniPath );


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



BOOL ReadIni( BOOL * pbAllowMulti )
{
	const TCHAR * strIniPath = GetIniPath();

	g_bLogging = !! GetPrivateProfileInt( TEXT( "Options" ), TEXT( "Logging" ), FALSE, strIniPath );
	EnableLoggingIni( g_bLogging );

	if( pbAllowMulti )
	{
		*pbAllowMulti = !! GetPrivateProfileInt( TEXT( "Options" ), TEXT( "AllowMulti" ), FALSE, strIniPath );
	}

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

	int i, j;
	TCHAR lpszKey[ 100 ];
	g_numOfEnemies = 0;
	for( i = 0; i < MAX_PROCESS_CNT; ++i )
	{
		_stprintf_s( lpszKey, _countof(lpszKey), _T( "Enemy%d" ), i );
		GetPrivateProfileString(
			TEXT( "Enemy" ),
			lpszKey,
			TEXT(""),
			g_lpszEnemy[ g_numOfEnemies ],
			MAX_PATH * 2,
			strIniPath
		);
		if( *g_lpszEnemy[ g_numOfEnemies ] == _T('\0') ) break;
		else if( IsAbsFoe( g_lpszEnemy[ g_numOfEnemies ] ) ) continue;
		else if( _tcschr( g_lpszEnemy[ g_numOfEnemies ], _T( '\\' ) ) != NULL ) continue;

		PathToExeEx( g_lpszEnemy[ g_numOfEnemies ], MAX_PATH + 1 );

		for( j = 0; j < g_numOfEnemies; ++j )
		{
			if( Lstrcmpi( g_lpszEnemy[ j ], g_lpszEnemy[ g_numOfEnemies ] ) == 0 ) goto NEXT_FOE;
		}

		++g_numOfEnemies;

		if( g_numOfEnemies == MAX_PROCESS_CNT )//*
			break;

		NEXT_FOE:
		{
			;
		}
	}

	g_numOfFriends = 0;
	for( i = 0; i < MAX_PROCESS_CNT; i++ )
	{
		_stprintf_s( lpszKey, _countof(lpszKey), _T( "Friend%d" ), i );
		GetPrivateProfileString(
			TEXT( "Friend" ),
			lpszKey,
			TEXT(""),
			g_lpszFriend[ g_numOfFriends ],
			MAX_PATH * 2,
			strIniPath
		);
	
		if( *g_lpszFriend[ g_numOfFriends ] == _T('\0') ) break;
		else if( IsAbsFoe( g_lpszEnemy[ g_numOfFriends ] ) ) continue;
		else if( _tcschr( g_lpszEnemy[ g_numOfFriends ], _T( '\\' ) ) != NULL ) continue;

		PathToExeEx( g_lpszFriend[ g_numOfFriends ], MAX_PATH + 1 );

		for( j = 0; j < g_numOfFriends; ++j )
		{
			if( Lstrcmpi( g_lpszFriend[ j ], g_lpszFriend[ g_numOfFriends ] ) == 0 ) goto NEXT_FRIEND;
		}
		
		for( j = 0; j < g_numOfEnemies; ++j )
		{
			if( Lstrcmpi( g_lpszEnemy[ j ], g_lpszFriend[ g_numOfFriends ] ) == 0 ) goto NEXT_FRIEND;
		}

		++g_numOfFriends;

		if( g_numOfFriends == MAX_PROCESS_CNT )//*
			break;

		NEXT_FRIEND:
		{
			;
		}
	}
	return TRUE;
}

BOOL WriteIni( bool fRealTime )
{
	// The maximum profile section size is 32,767 characters. Our cchBuffer is 133,120 cch.
	const size_t cchBuffer = (size_t)( MAX_PATH * 2 ) * MAX_PROCESS_CNT;
	TCHAR * lpBuffer = (TCHAR*) HeapAlloc( GetProcessHeap(), 0L, cchBuffer * sizeof(TCHAR) );
	if( ! lpBuffer )
		return FALSE;

	const TCHAR * strIniPath = GetIniPath();

	// Delete old keys/values
	WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider0" ),	NULL, strIniPath );
	WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider1" ),	NULL, strIniPath );
	// The key "Slider2" will be removed if ver <= v1.5.1; v1.5.2 uses this key as the "clean" marker.
	//WritePrivateProfileString( TEXT( "Slider" ), TEXT( "Slider2" ),	NULL, strIniPath );
	//WritePrivateProfileString( TEXT( "Options" ), TEXT( "RealTime" ), NULL, strIniPath );

	if( GetPrivateProfileInt( _T("Slider"), _T("Slider2"), 0, strIniPath ) < 1520 )
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

	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "RealTime" ),
		fRealTime ? TEXT( "1" ) : TEXT( "0" ),
		strIniPath
	);

	TCHAR strUnit[ 32 ];
	_stprintf_s( strUnit, _countof(strUnit), _T( "%u" ), g_uUnit );
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "Unit" ),
		strUnit,
		strIniPath
	);

	TCHAR lpszLangId[ 100 ];
	_stprintf_s( lpszLangId, _countof(lpszLangId), _T( "%d" ), (int) GetLanguage() );
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "Language" ),
		lpszLangId,
		strIniPath
	);
	
	WriteIni_Worker( lpBuffer, cchBuffer, false, strIniPath );
	WriteIni_Worker( lpBuffer, cchBuffer, true, strIniPath );

	HeapFree( GetProcessHeap(), 0L, lpBuffer );
	return TRUE;
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
		else
			*q = *p;

		++p;
		++q;
	}
	*q = _T('\0');
}

// v0.1.6.0 (20120427) : We only handle up to MAX_PROCESS_CNT (256) items.
// Therefore, we should somehow forget about old friends/enemies.
// So, we remember how old they are.
void WriteIni_Time( const TCHAR * pExe )
{
	TCHAR strExeNameLower[ MAX_PATH * 2 ];
	_bes_lower( pExe, strExeNameLower, MAX_PATH * 2 );

	FILETIME ft = {0};
	GetSystemTimeAsFileTime( &ft );
	TCHAR strTime[ 64 ];
	
	_stprintf_s( strTime, _countof(strTime), 
				_T("%I64u"), 
				(UINT64) ft.dwLowDateTime | (UINT64) ft.dwHighDateTime << 32 );

	WritePrivateProfileString(
		TEXT( "Time" ), 
		strExeNameLower,
		strTime,
		GetIniPath()
	);
}
static INT64 ReadIni_Time( const TCHAR * pExe )
{
	TCHAR strExeNameLower[ MAX_PATH * 2 ];
	_bes_lower( pExe, strExeNameLower, MAX_PATH * 2 );

	TCHAR strTime[ 64 ] = _T("");
	GetPrivateProfileString( _T("Time"), strExeNameLower, _T(""), strTime, 64, GetIniPath() );
	
	// Technically, (UINT64) _tcstoui64 is better, but VC6 doesns't have it.
	// In reality, this should be just fine. At least we can go to the year 30827.
	return _ttoi64( strTime );
}
typedef struct tagItemSorter {
	INT64 time;
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
	const TCHAR (*list)[ MAX_PATH * 2 ] = fFriend ? g_lpszFriend : g_lpszEnemy ;
	const ptrdiff_t cItems = fFriend ? g_numOfFriends : g_numOfEnemies ;
	TCHAR * ptr = lpBuffer;

	if( cItems )
	{
		LPITEM_SORTER sorter = (LPITEM_SORTER) HeapAlloc( GetProcessHeap(), 0L, 
														sizeof(ITEM_SORTER) * (size_t) cItems );
		if( ! sorter )
			return;

		ptrdiff_t i = 0;
		for( ; i < cItems; ++i )
		{
			sorter[ i ].number = (int) i;
			sorter[ i ].time = ReadIni_Time( list[ i ] );
#ifdef _DEBUG
			sorter[ i ].lpsz = &list[ i ][ 0 ];
#endif
		}
		qsort( sorter, (size_t) cItems, sizeof(ITEM_SORTER), &item_comp );


#if MAX_PROCESS_CNT < 256
# error MAX_PROCESS_CNT should be at least 256
#endif
		// Save only a limited number of items after sorted
		const ptrdiff_t numOfItemsToSave = __min( MAX_PROCESS_CNT - 56, cItems );

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
	
	if( ptr < lpBuffer + cchBuffer )
	{
		ptr[ 0 ] = _T('\0'); // additional NUL (after the previous NUL)
		if( ptr == lpBuffer )
			ptr[ 1 ] = _T('\0'); // if the previous NUL doesn't exist
	}
	else
	{
		lpBuffer[ cchBuffer - 2 ] = _T('\0');
		lpBuffer[ cchBuffer - 1 ] = _T('\0');
	}
	
	WritePrivateProfileSection( strSection, lpBuffer, strIniPath );
}


BOOL SetSliderIni( LPCTSTR lpszString, const int iSlider )
{
	if( iSlider < SLIDER_MIN || iSlider > SLIDER_MAX || _tcslen( lpszString ) > 1000 ) return FALSE;

	WriteDebugLog( TEXT( "SetSliderIni" ) );
	WriteDebugLog( lpszString );


	TCHAR strExeName[ MAX_PATH * 2 ] = _T( "" );
/* - 1.1b7
	int len = lstrlen( lpszTarget );
	int start = -1;
	int end = -1;
	for( int i = len - 1; i >= 5; i-- )
	{
		if(
			lpszTarget[ i ] == TEXT( ' ' ) &&
			( lpszTarget[ i - 1 ] == TEXT( 'e' ) || lpszTarget[ i - 1 ] == TEXT( 'E' ) ) &&
			( lpszTarget[ i - 2 ] == TEXT( 'x' ) || lpszTarget[ i - 2 ] == TEXT( 'X' ) ) &&
			( lpszTarget[ i - 3 ] == TEXT( 'e' ) || lpszTarget[ i - 3 ] == TEXT( 'E' ) ) &&
			lpszTarget[ i - 4 ] == TEXT( '.' )
		)
		{
			end = i;
			break;
		}
	}

	for( ; i >=0; i-- )
	{
		if( lpszTarget[ i ] == TEXT( '\\' ) )
		{
			start = i + 1;
			break;
		}
	}

	if( start == -1 ) start = 0;

	if( end == -1 || end - start <= 0 )
	{
		TCHAR dbg[1000];
		wsprintf( dbg, TEXT("DEBUG: %s %d %d %d"), lpszTarget, start, end, end-start );
		WriteDebugLog( dbg ) ;

		return FALSE;
	}



	lstrcpy( lpszExeName, &lpszTarget[ start ] );
	lpszExeName[ end - start ] = TEXT( '\0' );
*/
// +1.1b7
	PathToExe( lpszString, strExeName, MAX_PATH * 2 );


#if !defined( _UNICODE )
	if( strlen( strExeName ) >= 19 )
	{
		strExeName[ 15 ] = '\0';
		PathToExeEx( strExeName, MAX_PATH * 2 );
	}
#endif


	TCHAR strExeNameLower[ MAX_PATH * 2 ];
	_bes_lower( strExeName, strExeNameLower, MAX_PATH * 2 );

	const TCHAR * strIniPath = GetIniPath();

	TCHAR strNumber[ 100 ];
	_stprintf_s( strNumber, _countof(strNumber), _T( "%d" ), iSlider );

	WritePrivateProfileString(
		TEXT( "Slider" ), 
		strExeNameLower,
		strNumber,
		strIniPath
	);

	// to flush the cache
	WritePrivateProfileString(
		NULL,
		NULL,
		NULL,
		strIniPath
	);

	return TRUE;
}

int GetSliderIni( LPCTSTR lpszTargetPath, HWND hWnd )
{
	TCHAR strExeName[ MAX_PATH * 2 ];
	TCHAR strExeNameLower[ MAX_PATH * 2 ];

	if( lpszTargetPath == NULL ) return 33;

	const int len = (int) _tcslen( lpszTargetPath );
	int start = 0;
	int i;
	for( i = len - 1; i >= 0; i-- )
	{
		if( lpszTargetPath[ i ] == TEXT( '\\' ) )
		{
			start = i + 1;
			break;
		}
	}

	_tcscpy_s( strExeName, _countof(strExeName), &lpszTargetPath[ start ] );
#if !defined( _UNICODE )
	if( strlen( strExeName ) >= 19 )
	{
		strExeName[ 15 ] = '\0';
		PathToExeEx( strExeName, MAX_PATH * 2 );
	}
#endif

	_bes_lower( strExeName, strExeNameLower, MAX_PATH * 2 );

	const TCHAR * strIniPath = GetIniPath();
#if 0
	// to flush the cache --- Just in case!
	WritePrivateProfileString(
		NULL,
		NULL,
		NULL,
		strIniPath
	);
#endif
	int iSlider = (int) GetPrivateProfileInt( TEXT( "Slider" ), strExeNameLower, 33, strIniPath );
	if( iSlider == 0 ) iSlider = 1; // for backward compat.
	else if( iSlider < SLIDER_MIN || SLIDER_MAX < iSlider ) iSlider = 33;
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
	const TCHAR * strIniPath = GetIniPath();
	LONG x = (LONG) GetPrivateProfileInt( TEXT( "Window" ), TEXT( "PosX" ), CW_USEDEFAULT, strIniPath );
    LONG y = (LONG) GetPrivateProfileInt( TEXT( "Window" ), TEXT( "PosY" ), CW_USEDEFAULT, strIniPath );

	RECT area = {0};
	SystemParametersInfo( SPI_GETWORKAREA, 0, &area, 0 );
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



