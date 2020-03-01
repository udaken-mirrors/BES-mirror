/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"
//5,24,26
#define BES_TRAY_MENU( x ) (\
	IDM_STOP_FROM_TRAY - IDM_MENU_BASE ==  ( x ) ||\
	IDM_SHOWWINDOW     - IDM_MENU_BASE ==  ( x ) ||\
	IDM_EXIT_FROM_TRAY - IDM_MENU_BASE ==  ( x ) )
	//IDM_CROW           - IDM_MENU_BASE ==  ( x ) ||\
	//IDM_ONLINEHELP     - IDM_MENU_BASE ==  ( x )
//)


HWND CreateTooltip( const HINSTANCE hInst, const HWND hwndBase, LPCTSTR str )
{

	HWND hwndToolTip = CreateWindowEx(
		WS_EX_TOPMOST,
		TOOLTIPS_CLASS,
		NULL,
		WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		hwndBase,
		NULL,
		hInst,
		NULL
	);

	// needed?
	SetWindowPos(
		hwndToolTip,
		HWND_TOPMOST,
		0,
		0,
		0,
		0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
	);

	TCHAR lpszText[ 256 ] = _T("");
//	if( _tcslen( str ) < 256 )
	_tcscpy_s( lpszText, _countof(lpszText), str );
	RECT rectClient;
	GetClientRect ( hwndBase, &rectClient );
	TOOLINFO ti = {0};
	ti.cbSize = sizeof( TOOLINFO );
//	ti.uFlags = TTF_SUBCLASS;
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;//v1.3.8
	ti.hwnd = hwndBase;
	ti.hinst = hInst;
//	ti.uId = 0U;
	ti.uId = (UINT_PTR) hwndBase;//v1.3.8
	ti.lpszText = lpszText;
	ti.rect = rectClient;

	SendMessage( hwndToolTip, TTM_ADDTOOL, 0U, (LPARAM) &ti );

	// v1.3.8***
	SendMessage( hwndToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 500 );
	SendMessage( hwndToolTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 20*1000 );
	SendMessage( hwndToolTip, TTM_SETDELAYTIME, TTDT_RESHOW,  500 );
/*
	int i=SendMessage( hwndToolTip,TTM_GETDELAYTIME,TTDT_INITIAL,0);
	int a=SendMessage( hwndToolTip,TTM_GETDELAYTIME,TTDT_AUTOPOP,0);
	int r=SendMessage( hwndToolTip,TTM_GETDELAYTIME,TTDT_RESHOW,0);
	char test[100];
	sprintf(test,"a=%d i=%d r=%d\r\n",a,i,r);
	OutputDebugStringA(test);
*/
	return hwndToolTip;
}

VOID UpdateTooltip( const HINSTANCE hInst, const HWND hwndBase, LPCTSTR str, const HWND hwndToolTip )
{
	TCHAR lpszText[ 256 ] = _T( "" );
	if( _tcslen( str ) < 256 )
		_tcscpy_s( lpszText, _countof(lpszText), str );
	TOOLINFO ti = {0};
	ti.cbSize = sizeof( TOOLINFO );
	ti.hwnd = hwndBase;
	ti.hinst = hInst;
	ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND; //@2011-12-16
	ti.uId = (UINT_PTR) hwndBase; //@2011-12-16
	ti.lpszText = lpszText;
	SendMessage( hwndToolTip, TTM_UPDATETIPTEXT, 0U, (LPARAM) &ti );
}

VOID InitToolTipsEng( TCHAR (*szTips)[ 64 ] )
{
	_tcscpy_s( szTips[ 0 ], 64, _T( "Select the target process" ) );
	_tcscpy_s( szTips[ 1 ], 64, _T( "Stop limiting and/or watching totally" ) );
	_tcscpy_s( szTips[ 2 ], 64, _T( "Control how much to limit the process" ) );
	_tcscpy_s( szTips[ 3 ], 64, _T( "End the program" ) );
}

#ifdef _UNICODE
VOID InitToolTipsFin( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FIN0, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FIN1, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FIN2, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FIN3, -1, str[ 3 ], 64 );
}

VOID InitToolTipsSpa( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_SPA0, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_SPA1, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_SPA2, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_SPA3, -1, str[ 3 ], 64 );
}

VOID InitToolTipsChiT( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI0T, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI1T, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI2T, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI3T, -1, str[ 3 ], 64 );
}

VOID InitToolTipsChiS( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI0S, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI1S, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI2S, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_CHI3S, -1, str[ 3 ], 64 );
}

VOID InitToolTipsJpn( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_JPN0, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_JPN1, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_JPN2, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_JPN3, -1, str[ 3 ], 64 );
}

VOID InitToolTipsFre( TCHAR (*str)[ 64 ] )
{
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FRE0, -1, str[ 0 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FRE1, -1, str[ 1 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FRE2, -1, str[ 2 ], 64 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_TOOLTIP_FRE3, -1, str[ 3 ], 64 );
}


VOID InitMenuFin( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FIN0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FIN1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FIN2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FIN3, -1, str[ 3 ], 256 );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}

	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_21, -1, str[ 21 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FIN_30, -1, str[ 30 ], 256 );
	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );
	
//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}

VOID InitMenuSpa( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_SPA0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_SPA1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_SPA2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_SPA3, -1, str[ 3 ], 256 );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}

	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_21, -1, str[ 21 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, SPA_30, -1, str[ 30 ], 256 );
	
	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );	
//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}





VOID InitMenuFre( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FRE0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FRE1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FRE2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_FRE3, -1, str[ 3 ], 256 );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}

	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_21, -1, str[ 21 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_28, -1, str[ 28 ], 256 );
	
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, FRE_30, -1, str[ 30 ], 256 );
	
	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );	
	
//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}




VOID InitMenuJpn( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_JPN0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_JPN1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_JPN2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_JPN3, -1, str[ 3 ], 256 );
	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}


	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_21, -1, str[ 21 ], 256 );

	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_30, -1, str[ 30 ], 256 );

	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 
		4U,
		MF_BYPOSITION | MF_POPUP | MF_ENABLED | MF_STRING,
		(UINT_PTR) GetSubMenu( hSubMenu, 4 ),
		str[ 29 ]
	);
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );	

//---1.3.1
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, JPN_DEBUGPRIV, -1, str[ 31 ], 256 );
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND, IDM_DEBUGPRIVILEGE, str[ 31 ] );
//---
	
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}

VOID InitMenuChiT( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI0T, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI1T, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI2T, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI3T, -1, str[ 3 ], 256 );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}

	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_20, -1, str[ 20 ], 256 );
	
//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_21, -1, str[ 21 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHT_30, -1, str[ 30 ], 256 );

	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );	
	
//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}

VOID InitMenuChiS( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI0S, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI1S, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI2S, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, S_MENU_CHI3S, -1, str[ 3 ], 256 );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );

	}

	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_21, -1, str[ 21 ], 256 );

	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, CHS_30, -1, str[ 30 ], 256 );

	HMENU hSubMenu;
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );	
	
//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}

#endif




VOID InitMenuEng( const HWND hWnd )
{
	HMENU hMenu = GetMenu( hWnd );

	LONG flagRealtime = ( ( GetMenuState( hMenu, IDM_REALTIME, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
	LONG flagLogging = ( ( GetMenuState( hMenu, IDM_LOGGING, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#ifdef _UNICODE
	//LONG flagPriv = ( ( GetMenuState( hMenu, IDM_DEBUGPRIVILEGE, MF_BYCOMMAND ) ) & MFS_CHECKED ) ? MFS_CHECKED : MFS_UNCHECKED ;
#endif

	TCHAR * lpMem
		= (TCHAR*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, 50 * 256 * sizeof(TCHAR) );
	if( ! lpMem ) return;

	//TCHAR str[ 50 ][ 256 ];
	TCHAR * str[ 50 ];
	TCHAR * lp = lpMem;
	for( INT_PTR z = 0; z < 50; ++z, lp += 256 ) str[ z ] = lp;

	_tcscpy_s( str[ 0 ], 256, _T( "&File" ) );
	_tcscpy_s( str[ 1 ], 256, _T( "&Do" ) );
	_tcscpy_s( str[ 2 ], 256, _T( "&Options" ) );
	_tcscpy_s( str[ 3 ], 256, _T( "&Help" ) );

	for( UINT u = 0U; u < 4U; u++ )
	{
		ModifyMenu( hMenu, u, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hMenu, (int) u ), str[ u ] );
	}

#ifdef _UNICODE
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_0, -1, str[ 0 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_1, -1, str[ 1 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_2, -1, str[ 2 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_3, -1, str[ 3 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_4, -1, str[ 4 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_5, -1, str[ 5 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_6, -1, str[ 6 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_7, -1, str[ 7 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_8, -1, str[ 8 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_9, -1, str[ 9 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_10, -1, str[ 10 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_11, -1, str[ 11 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_12, -1, str[ 12 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_13, -1, str[ 13 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_14, -1, str[ 14 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_15, -1, str[ 15 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_16, -1, str[ 16 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_17, -1, str[ 17 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_18, -1, str[ 18 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_19, -1, str[ 19 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_20, -1, str[ 20 ], 256 );
	//
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_21, -1, str[ 21 ], 256 );

	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_22, -1, str[ 22 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_23, -1, str[ 23 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_24, -1, str[ 24 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_25, -1, str[ 25 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_26, -1, str[ 26 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_27, -1, str[ 27 ], 256 );
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_28, -1, str[ 28 ], 256 );
	for( int i = 0; i <= 28; i++ )
	{
		if( ! str[ i ][ 0 ] ) continue;
		UINT uMenuId = (UINT) ( IDM_MENU_BASE  +  i );
		ModifyMenu( hMenu, uMenuId, MF_BYCOMMAND, uMenuId, str[ i ] );
	}
	MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_29, -1, str[ 29 ], 256 );
	//MultiByteToWideChar( CP_UTF8, MB_CUTE, ENG_30, -1, str[ 30 ], 256 );
	
	HMENU hSubMenu;
	
	// Lang
	hSubMenu = GetSubMenu( hMenu, 2 );
	ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 29 ] );
	
	// Ukagaka
	//hSubMenu = GetSubMenu( hMenu, 3 );
	//ModifyMenu( hSubMenu, 4U, MF_BYPOSITION | MF_POPUP, (UINT_PTR) GetSubMenu( hSubMenu, 4 ), str[ 30 ] );
#endif


//---1.3.1
	//ModifyMenu( hMenu, IDM_DEBUGPRIVILEGE, MF_STRING | MF_BYCOMMAND, 
	//	IDM_DEBUGPRIVILEGE, ENG_DEBUGPRIV );
//---
	DrawMenuBar( hWnd );
	UpdateStatus( hWnd );
	CheckMenuItem( hMenu, IDM_REALTIME, (UINT) MF_BYCOMMAND | flagRealtime );
	CheckMenuItem( hMenu, IDM_LOGGING, (UINT) MF_BYCOMMAND | flagLogging );
#ifdef _UNICODE
	//CheckMenuItem( hMenu, IDM_DEBUGPRIVILEGE, (UINT) MF_BYCOMMAND | flagPriv );
#endif

	HeapFree( GetProcessHeap(), 0L, lpMem );
}
