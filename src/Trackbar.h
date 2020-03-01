/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#ifdef STRICT
#define WNDPROC_FARPROC WNDPROC
#else
#define WNDPROC_FARPROC FARPROC
#endif

inline WNDPROC_FARPROC SetSubclass( HWND hwndCtrl, WNDPROC lpSubProc )
{
	return (WNDPROC_FARPROC) SetWindowLongPtr_Floral( 
			hwndCtrl, GWLP_WNDPROC, (LONG_PTR) lpSubProc );
}



static WNDPROC_FARPROC PrevTrackbarProc[ 3 ];


//#ifndef WM_MOUSEWHEEL
//#define WM_MOUSEWHEEL                   0x020A
//#endif
#if !(_WIN32_WINNT >= 0x0400)
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif


LRESULT TB_OnMouseWheel( HWND hTrack, WPARAM wParam )
{
	const int delta = ( GET_WHEEL_DELTA_WPARAM( wParam ) <= 0 ) ? +1 : -1 ;
	const int iMax = (int) SendMessage( hTrack, TBM_GETRANGEMAX, 0, 0 );
	const int iMin = (int) SendMessage( hTrack, TBM_GETRANGEMIN, 0, 0 );
	int iPos = (int) SendMessage( hTrack, TBM_GETPOS, 0, 0 );
	iPos += delta;
	
	if( iMin <= iPos && iPos <= iMax )
	{
		SendMessage( hTrack, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) iPos );
		
		// Not very intuitive, but going left is "down", like "scroll down"
		// means "scroll forward" (positive)
		// and "scroll up" means "scroll back" (negative)
		SendMessage( GetParent( hTrack ), WM_HSCROLL,
						(WPARAM)( 0 < delta ? TB_LINEDOWN : TB_LINEUP ),
						(LPARAM) hTrack );
		SendMessage( GetParent( hTrack ), WM_HSCROLL, TB_ENDTRACK, (LPARAM) hTrack );
	}
	return (LRESULT) FALSE;
}

bool TB_OnLButtonDown( HWND hTB );
static LRESULT CALLBACK TrackbarSubProc0( HWND hTrack,
								  UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	if( uMessage == WM_MOUSEWHEEL ) return TB_OnMouseWheel( hTrack, wParam );
	else if( uMessage == WM_LBUTTONDOWN && TB_OnLButtonDown( hTrack ) ) return TRUE;
	else if( uMessage == WM_KILLFOCUS )
		SendMessage( GetParent( hTrack ), WM_USER_REFRESH, 0, WM_KILLFOCUS );
	
	return CallWindowProc( PrevTrackbarProc[ 0 ], hTrack, uMessage, wParam, lParam );
}
static LRESULT CALLBACK TrackbarSubProc1( HWND hTrack,
								  UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	if( uMessage == WM_MOUSEWHEEL ) return TB_OnMouseWheel( hTrack, wParam );
	else if( uMessage == WM_LBUTTONDOWN && TB_OnLButtonDown( hTrack ) ) return TRUE;
	else if( uMessage == WM_KILLFOCUS )
		SendMessage( GetParent( hTrack ), WM_USER_REFRESH, 1, WM_KILLFOCUS );

	return CallWindowProc( PrevTrackbarProc[ 1 ], hTrack, uMessage, wParam, lParam );
}
static LRESULT CALLBACK TrackbarSubProc2( HWND hTrack,
								  UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	if( uMessage == WM_MOUSEWHEEL ) return TB_OnMouseWheel( hTrack, wParam );
	else if( uMessage == WM_LBUTTONDOWN && TB_OnLButtonDown( hTrack ) ) return TRUE;
	else if( uMessage == WM_KILLFOCUS )
		SendMessage( GetParent( hTrack ), WM_USER_REFRESH, 2, WM_KILLFOCUS );

	return CallWindowProc( PrevTrackbarProc[ 2 ], hTrack, uMessage, wParam, lParam );
}


static WNDPROC MyTrackbarProc[ 3 ]
 = { TrackbarSubProc0, TrackbarSubProc1, TrackbarSubProc2 };


inline void 
UnsetSubclass( HWND hDlg, int idControl, WNDPROC_FARPROC& lpPrevProc )
{
	if( lpPrevProc )
	{
//		LONG_PTR rv =
		SetWindowLongPtr_Floral( GetDlgItem( hDlg, idControl ), 
			GWLP_WNDPROC, (LONG_PTR) lpPrevProc );
#if 0
		char test[100];
		sprintf(test,"Unset rv=%p (%p,%p,%p)\r\n",
			rv,TrackbarSubProc0,TrackbarSubProc1,TrackbarSubProc2);
		OutputDebugStringA(test);
#endif
		lpPrevProc = NULL;
	}
}
