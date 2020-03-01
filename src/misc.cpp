/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

extern volatile BOOL g_bHack[ MAX_SLOTS ];
//extern volatile DWORD g_dwTargetProcessId[ MAX_SLOTS ];
extern HFONT g_hFont;

BOOL IsOptionSet( const TCHAR * pCmdLine, const TCHAR * pOption, const TCHAR * pOption2 )
{
	if( pOption )
	{
		const TCHAR * start = _tcsstr( pCmdLine, pOption );
	
		if( start && ( pCmdLine == start || *(start-1) == _T(' ') ) )
		{
			const TCHAR * end = start + _tcslen( pOption );
			if( *end == _T('\0') || *end == _T(' ') )
				return TRUE;
		}
	}
	
	if( pOption2 )
	{
		const TCHAR * start = _tcsstr( pCmdLine, pOption2 );
	
		if( start && ( pCmdLine == start || *(start-1) == _T(' ') ) )
		{
			const TCHAR * end = start + _tcslen( pOption2 );
			if( *end == _T('\0') || *end == _T(' ') )
				return TRUE;
		}
	}

	return FALSE;
}

/*
HFONT UpdateFont( HFONT hFont )
{
	if( hFont ) DeleteFont( hFont );

	if( IS_CHINESE )
	{
		return MyCreateFont( TEXT( "Tahoma" ), 17, TRUE, FALSE );
	}
	
	HFONT h = NULL;
	h = MyCreateFont( TEXT( "Lucida Sans Unicode" ), 17, TRUE, FALSE );
	if( ! h ) h = MyCreateFont( TEXT( "Arial Unicode MS" ), 20, TRUE, FALSE );
	if( ! h && IsWindowsXPOrLater() ) hFont = MyCreateFont( TEXT( "Verdana" ), 16, TRUE, FALSE );
	return (HFONT) h;
}
*/



int UrlHitTest( LPARAM lParam, const RECT& urlrect )
{
	POINT pt;
	pt.x = (int) LOWORD( lParam );
	pt.y = (int) HIWORD( lParam );
	return PtInRect( &urlrect, pt ) ? 1 : 0 ;
}
#define AboutBoxUrlHit UrlHitTest

HFONT GetFontForURL( HDC hdc, float font_size )
{
	double k = 96.0 / GetDeviceCaps( hdc, LOGPIXELSY );
	int a = (int) ceil( ( k * font_size ) * GetDeviceCaps( hdc, LOGPIXELSY ) / 72.0 );
	//return CreateFont( -MulDiv( (int) font_size, GetDeviceCaps( hdc, LOGPIXELSY ), 72 ),
	return CreateFont( -a,
						0, 0, 0,
						FW_NORMAL,
						FALSE, // bItalic
						TRUE, // Underline
						FALSE, ANSI_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS,
						DEFAULT_QUALITY,
						FF_SWISS | VARIABLE_PITCH,
						TEXT("Verdana") );
}

#define SetFont(id,hfont) \
	SendDlgItemMessage( hDlg, id, WM_SETFONT, (WPARAM) hfont, MAKELPARAM( FALSE, 0 ) )

INT_PTR CALLBACK About( HWND hDlg, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	extern HWND g_hAboutDlg;
	static RECT urlrect = { 90, 140, 320, 170 };
	static HCURSOR hCursor[ 3 ];
	static int iCursor = 0;
	static int iMouseDown = 0;

	static HFONT hBoldFont = NULL;
	static HFONT hMonoFont = NULL;

	/**static TCHAR * lpWindowText = NULL;**/
	switch ( uMessage )
	{
		case WM_INITDIALOG:
		{
			HDC hDC = GetDC( hDlg );
			
			hBoldFont = MyCreateFont( hDC, TEXT( "Georgia" ), 14, TRUE, TRUE );
			
			const int iLogPixelSY = GetDeviceCaps( hDC, LOGPIXELSY );
			
			ReleaseDC( hDlg, hDC );

			hMonoFont = CreateFont( -MulDiv( 9, iLogPixelSY, 72 ),
										0,
										0,
										0,
										FW_NORMAL,
										FALSE,
										FALSE,
										FALSE,
										ANSI_CHARSET,
										OUT_DEFAULT_PRECIS,
										CLIP_DEFAULT_PRECIS,
										DEFAULT_QUALITY,
										FF_MODERN | FIXED_PITCH,
										TEXT("Courier New") );
			
			SetDlgItemText( hDlg, IDC_APP_NAME, APP_LONGNAME );
			
			SetFont( IDC_APP_NAME, hBoldFont );
			SetFont( IDC_DESCRIPTION, g_hFont );

			SetFont( IDC_DATETIME, hMonoFont );
			SetFont( IDC_COPYRIGHT, hMonoFont );
			SetFont( IDC_DESCRIPTION2, hMonoFont );

			if( ! IS_JAPANESE )
			{
				SetFont( IDC_EDITBOX_GPL, hMonoFont );
			}

			TCHAR str[ 1024 ] = _T("");
			_stprintf_s( str, _countof(str), _T( "Compiled on: %hs, %hs" ), __TIME__, __DATE__ );
			SetDlgItemText( hDlg, IDC_DATETIME, str );
			SetDlgItemText( hDlg, IDC_COPYRIGHT, APP_COPYRIGHT );

			SendDlgItemMessage( hDlg, IDC_EDITBOX_GPL, EM_SETMARGINS,
				(WPARAM) ( EC_LEFTMARGIN | EC_RIGHTMARGIN ),
				MAKELPARAM( 5 , 10 )
			);

#ifndef _MSC_VER
#define COMPILER_ TEXT("")
#elif _MSC_VER == 1600
#define COMPILER_ TEXT("(VC2010)")
#elif _MSC_VER == 1500
#define COMPILER_ TEXT("(VC2008)")
#elif _MSC_VER == 1400
#define COMPILER_ TEXT("(VC2005)")
#elif _MSC_VER == 1200
#define COMPILER_ TEXT("(VC6)")
#else
#define COMPILER_ TEXT("")
#endif
#if defined(_M_IX86_FP) && (_M_IX86_FP == 2)
#define CPU_ TEXT(" SSE2")
#else
#define CPU_ TEXT(" No-SSE2")
#endif

#ifdef _UNICODE
			_stprintf_s( str, _countof(str), L"Unicode Build %s%s",
#else
			_stprintf_s( str, _countof(str), "ANSI (Non-Unicode) Build %s%s",
#endif
						COMPILER_, CPU_ );

			SetDlgItemText( hDlg, IDC_DESCRIPTION2 , str );
			
			TCHAR lpWindowText[ MAX_WINDOWTEXT ] = _T("");
#ifdef _UNICODE
			if( IS_JAPANESE )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_9000, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_DESC, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );

				SendDlgItemMessage( hDlg, IDC_EDITBOX_GPL, WM_SETFONT, (WPARAM) g_hFont, MAKELPARAM( FALSE, 0 ) );
				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_JPN_GPL, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, str );
			}
			else if( IS_FINNISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_9000, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FIN_DESC, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
			}
			else if( IS_SPANISH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_9000, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_SPA_DESC, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
			}
			else if( IS_CHINESE_T )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_9000T, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_DESC_T, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
			}
			else if( IS_CHINESE_S )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_9000S, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_CHI_DESC_S, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
			}
			else if( IS_FRENCH )
			{
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_9000, -1, lpWindowText, MAX_WINDOWTEXT );
				SetWindowText( hDlg, lpWindowText );

				str[ 0 ] = TEXT( '\0' );
				MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FRE_DESC, -1, str, (int) _countof(str) );
				SetDlgItemText( hDlg, IDC_DESCRIPTION, str );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
			}
			else
#endif
			{
				SetDlgItemText( hDlg, IDC_DESCRIPTION, S_ENG_DESC );
				SetDlgItemText( hDlg, IDC_EDITBOX_GPL, TEXT_GPL );
				_tcscpy_s( lpWindowText, MAX_WINDOWTEXT, _T( "About BES" ) );
				SetWindowText( hDlg, lpWindowText );
			}
#if 0
			char hcSstpVersion[ 1024 ] = "";

#ifdef _UNICODE
			WideCharToMultiByte(
					CP_ACP,
					0,
					APP_NAME,
					-1,
					hcSstpVersion,
					(int) _countof(hcSstpVersion),
					" ",
					NULL
			);
#else
			strcpy_s( hcSstpVersion, _countof(hcSstpVersion), APP_NAME );
#endif
			strcat_s( hcSstpVersion, _countof(hcSstpVersion), "\\nCompiled on: " );
			strcat_s( hcSstpVersion, _countof(hcSstpVersion), __TIME__ );
			strcat_s( hcSstpVersion, _countof(hcSstpVersion), ", " );
			strcat_s( hcSstpVersion, _countof(hcSstpVersion), __DATE__ );

#ifdef _UNICODE
			if( IS_JAPANESE && ( LANG_JAPANESE == PRIMARYLANGID( GetSystemDefaultLangID() ) ) )
#else
			if( LANG_JAPANESE == PRIMARYLANGID( GetSystemDefaultLangID() ) )
#endif
			{
				DirectSSTP( (HWND) lParam, S_ABOUT_SJIS, hcSstpVersion, "Shift_JIS" );
			}
			else
			{
				DirectSSTP( (HWND) lParam, "About 'Battle Encoder Shirase'...", hcSstpVersion, "ASCII" );
			}
#endif


			RECT rcRef, rcRef2;
			GetClientRect( GetDlgItem( hDlg, IDC_DESCRIPTION ), &rcRef );
			MapWindowRect( GetDlgItem( hDlg, IDC_DESCRIPTION ), hDlg, &rcRef );
			GetClientRect( GetDlgItem( hDlg, IDC_DESCRIPTION2 ), &rcRef2 );
			MapWindowRect( GetDlgItem( hDlg, IDC_DESCRIPTION2 ), hDlg, &rcRef2 );
			SetRect( &urlrect, rcRef2.left + 2, rcRef.bottom + 2, rcRef.right, rcRef2.top );

			hCursor[ 0 ] = LoadCursor( ( HINSTANCE ) NULL, IDC_ARROW );
			hCursor[ 1 ] = LoadCursor( ( HINSTANCE ) NULL, IDC_HAND );
			hCursor[ 2 ] = hCursor[ 1 ];
			SetCursor( hCursor[ 0 ] );
			PostMessage( hDlg, WM_USER_CAPTION, 0, 0 );
			g_hAboutDlg = hDlg;
			break;
		}
		
		case WM_COMMAND:
		{
			if( LOWORD( wParam ) == IDOK || LOWORD( wParam ) == IDCANCEL ) 
			{
				EndDialog( hDlg, -1 );
				return TRUE;
			}
			break;
		}
/**
		case WM_GETICON:
		case WM_USER_CAPTION:
		{
			if( wParam == 0 && lpWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, MAX_WINDOWTEXT );
				if( _tcscmp( strCurrentText, lpWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpWindowText );
			}
			
			if( uMessage == WM_USER_CAPTION )
				break;

			return FALSE;
			break;
		}
		case WM_NCUAHDRAWCAPTION:
		{
			if( lpWindowText )
			{
				TCHAR strCurrentText[ MAX_WINDOWTEXT ] = _T("");
				GetWindowText( hDlg, strCurrentText, MAX_WINDOWTEXT );
				if( _tcscmp( strCurrentText, lpWindowText ) != 0 )
					SendMessage( hDlg, WM_SETTEXT, 0, (LPARAM) lpWindowText );
			}
			return FALSE;
			break;
		}
**/
		
		case WM_MOUSEMOVE:
		{
			if( iMouseDown == -1 ) break;

			int iPrevCursor = iCursor;
			iCursor = AboutBoxUrlHit( lParam, urlrect );

			// Reset Cursor for each WM_MOUSEMOVE, silly but...
			SetCursor( hCursor[ iCursor ] );

			if( iCursor == 1 && iMouseDown == 1 ) iCursor = 2;

			
			if( iPrevCursor != iCursor )
			{
/*
				// In theory, we have to change Cursor only when Prev!=Now, but
				// something is wrong here and that doesn't work.
				SetCursor( hCursor[ iCursor ] );
*/
//				const RECT urlrect = URL_RECT;
				InvalidateRect( hDlg, &urlrect, TRUE );
			}
			break;
		}

		case WM_LBUTTONDOWN:
		{
			HWND hwndDefBtn = GetDlgItem( hDlg, IDOK );
			if( GetFocus() != hwndDefBtn )
				PostMessage( hDlg, WM_NEXTDLGCTL, (WPARAM) hwndDefBtn, MAKELPARAM( TRUE, 0 ) );

			SetCapture( hDlg );

			if( AboutBoxUrlHit( lParam, urlrect ) )
			{
				iMouseDown = 1;
				SetCursor( hCursor[ 1 ] );
				iCursor = 2;
//				const RECT urlrect = URL_RECT;
				InvalidateRect( hDlg, &urlrect, TRUE );
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
			
			if( iMouseDown == 1 && AboutBoxUrlHit( lParam, urlrect ) )
			{
				OpenBrowser( APP_HOME_URL );
			}

			iCursor = 0;
			SetCursor( hCursor[ 0 ] );
			iMouseDown = 0;
			break;
		}

		case WM_ACTIVATE:
		{
			if( wParam == WA_INACTIVE )
			{
				iMouseDown = -1;
				iCursor = 0;
				SetCursor( hCursor[ 0 ] );
//				const RECT urlrect = URL_RECT;
				InvalidateRect( hDlg, &urlrect, TRUE );
			}
			else
			{
				iMouseDown = 0;
			}
			break;
		}
		
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint( hDlg, &ps );
			const int iSaved = SaveDC( hdc );
			
			HFONT hfontURL = GetFontForURL( hdc, 9.0 );
			HFONT hOldFont = SelectFont( hdc, hfontURL );

			SetBkMode( hdc, TRANSPARENT );
			SetTextColor( hdc,
				iCursor == 2 ? RGB( 0xff, 0x00, 0x00 ) :
				iCursor == 1 ? RGB( 0x00, 0x80, 0x00 ) : RGB( 0x00, 0x00, 0xff ) );
			
			//"http://mion.faireal.net/BES/"
			// 1234567890123456789012345678
//			TextOut( hdc, 100, 148, APP_HOME_URL, 28 );
			SetTextAlign( hdc, TA_TOP | TA_LEFT );
			TextOut( hdc, urlrect.left, urlrect.top, APP_HOME_URL, 28 );
			SIZE size;
			GetTextExtentPoint32( hdc, APP_HOME_URL, 28, &size );
			urlrect.right = urlrect.left + size.cx;
			urlrect.bottom = urlrect.top + size.cy;

			SelectObject( hdc, hOldFont );
			DeleteFont( hfontURL );
			
			RestoreDC( hdc, iSaved );
			EndPaint( hDlg, &ps );
			break;
		}

		case WM_CTLCOLORSTATIC:
		{
			if( (HWND) lParam == GetDlgItem( hDlg, IDC_APP_NAME ) )
			{
				HDC hDC = (HDC) wParam;
				SetBkMode( hDC, TRANSPARENT );
				SetBkColor( hDC, GetSysColor( COLOR_3DFACE ) );
				SetTextColor( hDC, RGB( 0,0,0xa0 ) );
				return (INT_PTR)(HBRUSH) GetSysColorBrush( COLOR_3DFACE );
			}
			return (INT_PTR)(HBRUSH) NULL;
			break;
		}
		
		case WM_DESTROY:
		{
			g_hAboutDlg = NULL;
			SetFont( IDC_APP_NAME, NULL );
			SetFont( IDC_DESCRIPTION, NULL );

			SetFont( IDC_DATETIME, NULL );
			SetFont( IDC_COPYRIGHT, NULL );
			SetFont( IDC_DESCRIPTION2, NULL );

			if( hBoldFont )
			{
				DeleteFont( hBoldFont );
				hBoldFont = NULL;
			}

			if( hMonoFont )
			{
				DeleteFont( hMonoFont );
				hMonoFont = NULL;
			}

			/**if( lpWindowText )
			{
				HeapFree( GetProcessHeap(), 0L, lpWindowText );
				lpWindowText = NULL;
			}**/
			break;
		}
		
		default:
			return FALSE;
	}

	return TRUE;
}

static int get_slider_int( const TCHAR * ptr, bool fAllowMoreThan99, int * aiParams, int * piMode );

ptrdiff_t ParseJobList( const TCHAR * lpszCmdLine,
					   TCHAR ** arTargetPaths,
					   int * arSliders,
					   int * arCycle,
					   int * arDelay,
					   int * arMode,
					   ptrdiff_t array_len,
					   bool fAllowMoreThan99 )
{
	if( ! lpszCmdLine || ! arTargetPaths || ! arSliders || ! array_len ) return 0;

	const TCHAR * ptr = lpszCmdLine;
	while( *ptr == _T(' ') ) ++ptr;
	if( *ptr == _T('\"') ) // quoted app-name
	{
		ptr = _tcschr( ptr + 1, _T( '\"' ) ); // unquote
		if( ! ptr ) return 0;
		++ptr; // char after the closing quot. mark
		if( *ptr != _T(' ') ) return 0;
	}
	else // app-name not quoted
	{
		ptr = _tcschr( ptr, _T(' ') ); // space after app-name
		if( ! ptr ) return 0;
	}

	ptrdiff_t num = 0;
	while( num < array_len )
	{
		const TCHAR * pQuote = _tcschr( ptr, _T('\"') );
		if( ! pQuote ) break;
		const TCHAR * pUnquote = _tcschr( pQuote + 1, _T('\"') );
		if( ! pUnquote ) break;
	
		size_t cchPath = (size_t)( pUnquote - ( pQuote + 1 ) );
		arTargetPaths[ num ] = TcharAlloc( cchPath + 1 );

		if( arTargetPaths[ num ] == NULL ) // not allocated
		{
			for( ptrdiff_t n = num - 1; n >= 0; --n ) // free everything (v1.7.5)
			{
				MemFree( arTargetPaths[ n ] );
				arTargetPaths[ n ] = NULL;
			}

			num = 0;
			break;
		}

		_tcsncpy_s( arTargetPaths[ num ], cchPath + 1, pQuote + 1, cchPath );
		arTargetPaths[ num ][ cchPath ] = _T('\0'); // just in case

		// Find the number (if any)
		ptr = pUnquote + 1;
		while( *ptr != _T('\0')
				&& *ptr != _T('\"')
				&& !( _T('0') <= *ptr && *ptr <= _T('9') ) // not a number
				&& !( *ptr == _T('-') && _T('0') <= ptr[ 1 ] && ptr[ 1 ] <= _T('9') ) // not a negative number
		){ ++ptr; }

		if( *ptr == _T('\0') ) // no number
		{
			arSliders[ num++ ] = -1; // def value, meaning 'undefined'
			break;
		}
		else if( *ptr == _T('\"') ) // no number
		{
			arSliders[ num++ ] = -1; // def value, meaning 'undefined'
			--ptr; // rewind so this quote will be the next pQuote
		}
		else
		{
			int aiParams[ 2 ] = { 0, 0 };
			int mode = DEF_MODE;
			arSliders[ num ] = get_slider_int( ptr, fAllowMoreThan99, &aiParams[ 0 ], &mode );
			arCycle[ num ] = aiParams[ 0 ];
			arDelay[ num ] = aiParams[ 1 ];
			arMode[ num ] = mode;
			++num;
		}
	}

	return num;
}

int ParseArgs(
	const TCHAR * lpszCmdLine, 
	size_t cchBuf,
	TCHAR * lpszTargetLongPath,
	TCHAR * lpszTargetLongExe,
	TCHAR * lpszOptions,
	bool fAllowMoreThan99,
	int * aiParams,
	int * piMode )
{
	if( ! lpszCmdLine )
		return IGNORE_ARGV;

	if( lpszTargetLongPath )
		*lpszTargetLongPath = _T('\0');
	
	if( lpszTargetLongExe )
		*lpszTargetLongExe = _T('\0');
	
	if( lpszOptions )
		*lpszOptions = _T('\0');

	WriteDebugLog( _T("CmdLine=") );
	WriteDebugLog( lpszCmdLine );

	// Technically, the command line length limit is 32767 cch @fix20111130

	const TCHAR * ptr = lpszCmdLine;
	while( *ptr == _T(' ') ) ++ptr;

	TCHAR strTargetPath[ MAX_PATH * 2 ] = _T( "" );
	if( *ptr == _T('\"') ) // quoted app-name
	{
		ptr = _tcschr( ptr + 1, _T( '\"' ) ); // unquote
		
		if( ! ptr ) return IGNORE_ARGV;

		++ptr; // char after the closing quot. mark

		if( *ptr != _T(' ') ) return IGNORE_ARGV;
	}
	else // app-name not quoted
	{
		ptr = _tcschr( ptr, _T(' ') ); // space after app-name
		
		if( ! ptr ) return IGNORE_ARGV;
	}

	// skip space(s) after the app-name
	while( *ptr == _T(' ') ) ++ptr;

	size_t cchTargetPath = 0;
	if( *ptr == _T('\"') ) // target name is quoted
	{
		const TCHAR * pEndOfName = _tcschr( ++ptr, _T( '\"' ) ); // closing quot. mark
		
		if( ! pEndOfName )
			return IGNORE_ARGV;

		cchTargetPath = (size_t)( pEndOfName - ptr );
		if( _countof(strTargetPath) <= cchTargetPath )
			return IGNORE_ARGV;
		
		_tcsncpy_s( strTargetPath, _countof(strTargetPath), ptr, cchTargetPath );

		ptr = pEndOfName + 1; // char after the closing quot. mark

		//if( *ptr != _T(' ') ) // incl. *ptr==NUL
		//	return 0; // def percentage
	}
	else if( *ptr == _T('-') || *ptr == _T('/') ) // unquoted - or / is treated as a switch
	{
		; // --minimize, --unlimit, etc.
	}
	else // target name is not quoted
	{
		const TCHAR * pEndOfName = _tcschr( ptr, _T(' ') );
		
		if( ! pEndOfName )
			pEndOfName = ptr + _tcslen( ptr );

		cchTargetPath = (size_t)( pEndOfName - ptr );

		if( _countof(strTargetPath) <= cchTargetPath )
			return IGNORE_ARGV;

		_tcsncpy_s( strTargetPath, _countof(strTargetPath), ptr, cchTargetPath );

		ptr = pEndOfName; // space or NUL
	}

	if( *strTargetPath )
	{
		WriteDebugLog( _T("Target=") );
		WriteDebugLog( strTargetPath );

		// Refuse it if it's only one letter [20140331]
		if( ! strTargetPath[ 1 ] ) return IGNORE_ARGV;

		if( lpszTargetLongPath )
		{
			TCHAR * star = _tcschr( strTargetPath, _T('*') );
			if( star )
			{
				bool fOK = false;
				if( strTargetPath < star )
				{
					TCHAR szHead[ CCHPATH ];
					_tcsncpy_s( szHead, _countof(szHead), strTargetPath, (rsize_t)( star - strTargetPath ) );
					TCHAR * pBkSlash = _tcsrchr( szHead, _T('\\') );
					if( pBkSlash )
					{
						*pBkSlash = _T('\0');
						DWORD dwcch = GetLongPathName( szHead, lpszTargetLongPath, (DWORD) cchBuf );
						if( dwcch && dwcch < cchBuf )
						{
							ptrdiff_t ofs = pBkSlash - szHead; // offset to the BackSlash above
							_tcscat_s( lpszTargetLongPath, cchBuf, strTargetPath + ofs );
							fOK = true;
						}
					}
				}

				if( ! fOK )
					_tcscpy_s( lpszTargetLongPath, cchBuf, strTargetPath );
			}
			else
			{
				DWORD dwcch = GetLongPathName( strTargetPath, lpszTargetLongPath, (DWORD) cchBuf );
				if( ! dwcch || cchBuf <= dwcch )
					_tcscpy_s( lpszTargetLongPath, cchBuf, strTargetPath );
			}

			if( lpszTargetLongExe )
			{
				PathToExe( lpszTargetLongPath, lpszTargetLongExe, cchBuf );
				// strcpy_s( lpszTargetLongPath, cchBuf, lpszTargetLongExe ); // ANSIFIX
			}
		}
	}

	while( *ptr == _T(' ') ) ++ptr; // skip spaces before Options

	if( ! *ptr ) // no optitons
		return -1;

	WriteDebugLog( _T("Options=") );
	WriteDebugLog( ptr );

	if( lpszOptions )
		_tcsncpy_s( lpszOptions, cchBuf, ptr, _TRUNCATE );

	while( *ptr ) // looking for a number after a space
	{
		if( lpszCmdLine < ptr && *(ptr-1) == _T(' ') )
		{
			if( _T('0') <= *ptr && *ptr <= _T('9')
				||
				*ptr == _T('-') && _T('0') <= *(ptr+1) && *(ptr+1) <= _T('9') )
			{
				break;
			}
		}

		++ptr;
	}

	int iSlider = -1;
	if( *ptr ) // a number found after a space
		iSlider = get_slider_int( ptr, fAllowMoreThan99, aiParams, piMode );

	return iSlider;
}

static int get_slider_int( const TCHAR * ptr, bool fAllowMoreThan99, int * aiParams, int * piMode )
{
	int iSlider = -1;
/*
#if defined(_MSC_VER) && _MSC_VER < 1400
	double percent = _tcstod( ptr, NULL ); // VC6 doesn't have _wtof
#else
	double percent = _tstof( ptr );
#endif
*/
	TCHAR * endptr;
	double percent = _tcstod( ptr, &endptr );

	if( aiParams != NULL ) // aiParams[0]: unit (Sleep/Awake cycle), aiParams[1]: delay (initial delay before starting cycles) 2016-11-01
	{
		aiParams[ 0 ] = -1; // undefined/invalid
		aiParams[ 1 ] = -1;
		if( piMode ) *piMode = DEF_MODE;
		if( *endptr == _T(';') )
		{
			if( _T('0') <= endptr[ 1 ] && endptr[ 1 ] <= _T('9') )
			{
				double d = _tcstod( endptr + 1, &endptr );
				if( UNIT_MIN <= d && d <= UNIT_MAX || d == 0.0 ) aiParams[ 0 ] = (int) d;
				if( *endptr == _T(';') && _T('0') <= endptr[ 1 ] && endptr[ 1 ] <= _T('9') ) {
					d = _tcstod( endptr + 1, &endptr );
					if( DELAY_MIN <= d && d <= DELAY_MAX ) aiParams[ 1 ] = (int) d;
					if( *endptr == _T(';') && piMode ) {
						long l = _tcstol( endptr + 1, &endptr, 10 );
						if( l == 0 || l == 1 ) *piMode = (int) l; 
					}
				}
			}
			else if( endptr[ 1 ] == _T(';') && _T('0') <= endptr[ 2 ] && endptr[ 2 ] <= _T('9') )
			{
				double d = _tcstod( endptr + 2, &endptr );
				if( DELAY_MIN <= d && d <= DELAY_MAX ) aiParams[ 1 ] = (int) d;
				if( *endptr == _T(';') && piMode ) {
					long l = _tcstol( endptr + 1, &endptr, 10 );
					if( l == 0 || l == 1 ) *piMode = (int) l; 
				}
			}
		}
	}

	// "bes taraget.exe 25" and "bes target.exe -25" mean the same thing (v1.6.0)
	if( percent < 0.0 ) percent = -percent;

	if( 0.0 < percent && percent < 99.0 )
	{
		iSlider = (int)( percent + 0.5 );
		if( iSlider < SLIDER_MIN ) iSlider = SLIDER_MIN;
	}
	else if( 99.0 <= percent && percent < 100.0 )
	{
		if( fAllowMoreThan99 )
		{
			int decimal = 0;
			if( *ptr == _T('-') ) ++ptr;
			while( *ptr == _T('0') ) ++ptr;
			
			if( ptr[ 0 ] == _T('9')
				&& ptr[ 1 ] == _T('9')
				&& ( ptr[ 2 ] == _T('.') || ptr[ 2 ] == _T(',') )
				&& ( _T('0') <= ptr[ 3 ] && ptr[ 3 ] <= _T('9') )
			)
			{
				decimal = ptr[ 3 ] - _T('0');
				if( _T('5') <= ptr[ 4 ] && ptr[ 4 ] <= _T('9') ) ++decimal;
			}
			else
			{
				decimal = (int)(( percent - 99.0 ) * 10.0 + 0.5 );
			}

			iSlider = 99 + decimal;
			if( iSlider > SLIDER_MAX ) iSlider = SLIDER_MAX;
		}
		else iSlider = 99;
	}
#if ALLOW100
	else if( percent == 100.0 )
	{
		iSlider = (fAllowMoreThan99) ? SLIDER_MAX : 99 ;
	}
#endif

	return iSlider;
}

void ShowCommandLineHelp( HWND hWnd )
{
	UINT uType = (UINT)( hWnd
						? MB_ICONINFORMATION | MB_OK
						: MB_ICONINFORMATION | MB_TOPMOST | MB_OK );
	MessageBox( hWnd,
				TEXT( "* Commandline Help *\r\n" )
				TEXT( "\r\n" )
				_T("\"C:\\path\\to\\BES.exe\" \"C:\\path\\to\\target.exe\" [N] [OPTIONS]\r\n")
#ifdef _UNICODE
				_T("\r\n\t= Limit <target.exe>; reduce its CPU time by N% (N=1\x2013") _T("99).\r\n")
#else
				_T("\r\n\t= Limit <target.exe>; reduce its CPU time by N% (N=1...99).\r\n")
#endif
				_T("\r\n")
				_T("[OPTIONS]\r\n")
#ifdef _UNICODE
				_T("--minimize, -m\r\n\t= Don\x2019t show BES\x2019s window.\r\n")
#else
				_T("--minimize, -m\r\n\t= Don't show BES's window.\r\n")
#endif
				_T("--unlimit, -u\r\n\t= Stop limiting.\r\n")
				_T("--toggle, -t\r\n\t= Limit if not limited; unlimit otherwise.\r\n")
				_T("--exitnow, -e, -x\r\n\t= Exit now.\r\n")
				_T("--allow-multi\r\n\t= Allow multiple instances of BES.\r\n")
			//	_T("--disallow-multi\r\n\t= Disallow multiple instances of BES.\r\n")
			//	_T("--watch-multi\r\n\t= Watch multiple instances of the target EXE.\r\n")
				_T("--add, -a\r\n\t= Add a target to the watch list,\r\n\t or (if already in the list) update its parameters.\r\n") // 2014-03-31 (1.7.0.18)
				//_T("--hide\r\n\t= Hide the window of BES and its tray icon (use this carefully).\r\n") // 2015-10-22 (1.7.0.26)
				//_T("--reshow\r\n\t= Reshow the hidden window and tray icon.\r\n") // 2015-10-22 (1.7.0.26)
				//_T("--selfish\r\n\t= Always assume there are no other instances of BES (use this carefully).\r\n") // 2015-11-26 (1.7.0.27)
				_T("--delay, -D\r\n\t= Wait for 1 second before starting BES.\r\n") // 2015-11-26 (1.7.0.27)
				_T("--help, -h, /?\r\n\t= Show this help.\r\n")
				_T("--version\r\n\t= Show the version.\r\n")
				_T("\r\n")
				_T("BES.exe -J \"target1.exe\" N1 \"target2.exe\" N2, etc.\r\n\t= Multiple targets in one command line.\r\n") // 2015-11-26 (1.7.0.27)
				_T("\r\n")
				_T("You can use N;C;D instead of N:\r\n e.g. BES.exe \"target.exe\" 30;40;1000\r\n") // 2016-11-02 (1.7.3.2)
				_T("\t= Sleep/Awake Cycle: 40 ms, Initial Delay: 1000 ms\r\n") // 2016-11-02 (1.7.3.2)
				_T("\r\n")
				_T("You can use \"PID:xxxx\" instead of \"path\\to\\target.exe\"\r\n") // 2017-03-17 (1.7.5)
				,
				APP_NAME, uType );
}




VOID AboutShortcuts( HWND hWnd )
{
	const TCHAR lpszMessage[] = 
		TEXT( "* Keyborad Shortcuts *\r\n" )
		TEXT( "\r\n" )
		TEXT( "[Enter]\t\t: Select Target\r\n" )
		TEXT( "[T]\t\t: Select Target\r\n" )
		TEXT( "[Ins]\t\t: Select Target\r\n" )
		TEXT( "[W]\t\t: Watch\r\n" )
		TEXT( "\r\n" )
		TEXT( "[C]\t\t: Control Limiter(s)\r\n" )
		TEXT( "\r\n" )
		TEXT( "[Esc]\t\t: Unlimit All (when active)\r\n" )
		TEXT( "[U]\t\t: Unlimit All (when active)\r\n" )
		TEXT( "[Delete]\t\t: Unlimit All (when active)\r\n" )
		TEXT( "\r\n" )
		TEXT( "[Esc]\t\t: Exit (when idle)\r\n" )
		TEXT( "[X]\t\t: Exit (when idle)\r\n" )
		TEXT( "\r\n" )
		TEXT( "[A]\t\t: About\r\n" )
		TEXT( "[F1]\t\t: This Help (you can disable this shortcut)\r\n" )
		TEXT( "[F2]\t\t: Commandline Help\r\n" )
		TEXT( "\r\n" )
#ifdef _UNICODE
		TEXT( "[F5]\t\t: Refresh the List (in the \x201CTarget\x201D dialog box)\r\n" )
#else
		TEXT( "[F5]\t\t: Refresh the List (in the \"Target\" dialog box)\r\n" )
#endif
		TEXT( "\r\n" )
		TEXT( "[B]\t\t: Boss key aka Minimize (Send to System Tray)\r\n" )
		;

	MessageBox(
		hWnd,
		lpszMessage,
		APP_NAME,
		MB_OK | MB_ICONINFORMATION
	);
}

void ShowWatchList( HWND hWnd ) // 2017-11-03
{
	extern PATHINFO g_arPathInfo[ MAX_WATCH ];
	extern ptrdiff_t g_nPathInfo;
	extern HANDLE hReconSema;

	const TCHAR * pszTitle = _T("BES Watch List");

	if( WaitForSingleObject( hReconSema, 3000 ) != WAIT_OBJECT_0 ) {
		MessageBox( hWnd, _T("Busy!"), _T("Sema Error"), MB_ICONEXCLAMATION | MB_OK );
		return;
	}

	const ptrdiff_t nItems = g_nPathInfo;
	if( nItems <= 0 ) {
		ReleaseSemaphore( hReconSema, 1L, NULL );
		MessageBox( hWnd, _T("No items."), pszTitle, MB_ICONINFORMATION | MB_OK );
		return;
	}

	TCHAR ** lppTmp = (TCHAR **) MemAlloc( sizeof(TCHAR*), (size_t) nItems );
	if( ! lppTmp ) {
		ReleaseSemaphore( hReconSema, 1L, NULL );
		return;
	}

	int icchWritten = 0;
	ptrdiff_t i;
	for( i = 0; i < nItems; ++i )
	{
		const size_t cch = _tcslen( g_arPathInfo[ i ].pszPath ) + 150;
		lppTmp[ i ] = TcharAlloc( cch );
		if( ! lppTmp[ i ] ) break;

		icchWritten += _sntprintf_s( lppTmp[ i ], cch, _TRUNCATE,
			_T("[%d] %s\n\t-%d%% : Cycle=%d : Delay=%d : Mode=%d\n\n"),
									(int) i + 1,
									g_arPathInfo[ i ].pszPath,
									g_arPathInfo[ i ].slider,
									(int) g_arPathInfo[ i ].wCycle,
									(int) g_arPathInfo[ i ].wDelay,
									(int) g_arPathInfo[ i ].mode );
	}
	ReleaseSemaphore( hReconSema, 1L, NULL );

	if( i == nItems ) {
		const size_t cchMessage = (size_t) icchWritten + 1;
		TCHAR * lpMessage = TcharAlloc( cchMessage );
		if( lpMessage )
		{
			lpMessage[ 0 ] = _T('\0');
			for( ptrdiff_t j = 0; j < nItems; ++j ) _tcscat_s( lpMessage, cchMessage, lppTmp[ j ] );

			MessageBox( hWnd, lpMessage, pszTitle, MB_ICONINFORMATION | MB_OK );

			MemFree( lpMessage );
		}
	}

	for( i = nItems - 1; i >= 0; --i ) if( lppTmp[ i ] ) MemFree( lppTmp[ i ] );
	MemFree( lppTmp );
}

typedef struct tagGSHOW_PARAMS {
	HWND hWnd;
	LPCTSTR lpTarget;
} GSHOW_PARAMS;

BOOL CALLBACK EnumWindowsProc(      
    HWND hwndTarget,
    LPARAM lParam
)
{
	TCHAR lpText[ 256 ] = TEXT("");
	GetWindowText( hwndTarget, lpText, 256 );

	GSHOW_PARAMS * lpParams = (GSHOW_PARAMS * ) lParam;
	if( ! lpParams->lpTarget[ 0 ] ) return FALSE;
	
	if( hwndTarget != lpParams->hWnd )
	{
		if( _tcsicmp( lpText, lpParams->lpTarget ) == 0 )
		{
			if( IsIconic( hwndTarget ) )
			{
				ShowWindow( hwndTarget, SW_RESTORE );
				SetForegroundWindow( lpParams->hWnd );
			}

			if( IsWindowVisible( hwndTarget ) )
			{
				ShowWindow( hwndTarget, SW_HIDE );
			}
			else
			{
				ShowWindow( hwndTarget, SW_RESTORE );
				BringWindowToTop( hwndTarget );
				SetForegroundWindow( lpParams->hWnd );
			}
		
			return FALSE;
		}
	}

	return TRUE;
}


static inline BOOL GetTargetTitleIni( LPTSTR lpStr, int iMax )
{
	if( ! lpStr ) return FALSE;

	const TCHAR * strIniPath = GetIniPath();

	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "GShow" ),
		NULL,
		lpStr,
		(DWORD) iMax,
		strIniPath
	);

	return ( *lpStr != TEXT( '\0' ) );
}

VOID GShow( HWND hWnd )
{
	TCHAR lpTargetTitle[ 256 ] = TEXT( "" );
	
	if( GetTargetTitleIni( lpTargetTitle, 255 ) )
	{
		GSHOW_PARAMS gs = {0};
			gs.hWnd = hWnd;
			gs.lpTarget = (LPCTSTR) lpTargetTitle;

		EnumWindows( EnumWindowsProc, (LPARAM) &gs );
	}
}		

BOOL CheckLanguageMenuRadio( const HWND hWnd )
{
	return CheckMenuRadioItem( GetMenu( hWnd ), IDM_CHINESE_S, IDM_JAPANESEo,
				(UINT)( ( GetLanguage() == LANG_CHINESE_T )? IDM_CHINESE_T :
				( GetLanguage() == LANG_CHINESE_S )? IDM_CHINESE_S :
				( GetLanguage() == LANG_JAPANESE )? IDM_JAPANESE :
				( GetLanguage() == LANG_JAPANESEo )? IDM_JAPANESEo :
				( GetLanguage() == LANG_SPANISH )? IDM_SPANISH :
				( GetLanguage() == LANG_FRENCH )? IDM_FRENCH :
				( GetLanguage() == LANG_FINNISH )? IDM_FINNISH : IDM_ENGLISH ),
				MF_BYCOMMAND
	);
}

WORD SetLanguage( WORD wLID )
{
	static WORD wLanguageId;
	
	if( wLID == (WORD) 0 )
	{
		if( wLanguageId == (WORD) 0 ) wLanguageId = LANG_ENGLISH;
	}
	else
	{
		wLanguageId = wLID;
		if( wLanguageId != LANG_ENGLISH && wLanguageId != LANG_FINNISH
			&& wLanguageId != LANG_FRENCH
			&& wLanguageId != LANG_JAPANESE && wLanguageId != LANG_JAPANESEo
			&& wLanguageId != LANG_CHINESE_T && wLanguageId != LANG_CHINESE_S 
			&& wLanguageId != LANG_SPANISH )
		{
			wLanguageId = LANG_ENGLISH;
		}
	}
	return wLanguageId;		
}
WORD GetLanguage( void )
{
	return SetLanguage( (WORD) 0 );
}


#if !defined(_MSC_VER) || _MSC_VER < 1400 // VC2005+ quick-emu

int sprintf_s( char * szBuffer, size_t cch, const char * szFormat, ... )
{
	if( ! szBuffer || ! szFormat || (int)(UINT) cch < 0 )
	{
		errno = EINVAL;
		return -1;
	}
	if( 0 == cch ) return 0;
	
	// buffer-full detector
	szBuffer[ cch - 1 ] = '\0';

	va_list args;
	va_start( args, szFormat );
	const int iWritten = _vsnprintf( szBuffer, cch - 1, szFormat, args );
	va_end( args );
	
	if( -1 == iWritten )
	{
		if( szBuffer[ cch - 1 ] != '\0' )
		{
			// ERANGE, not EINVAL (VC2005)
			errno = ERANGE;
		}
		else
		{
			errno = 0;
		}
		
		*szBuffer = '\0';
		return -1;
	}
	else if( (int) cch - 1 == iWritten )
	{
		szBuffer[ cch - 1 ] = '\0';
	}
	return iWritten;
}
int swprintf_s( wchar_t * szBuffer, size_t cch, const wchar_t * szFormat, ... )
{
	if( ! szBuffer || ! szFormat || (int)(UINT) cch < 0 )
	{
		errno = EINVAL;
		return -1;
	}
	if( 0 == cch ) return 0;
	
	// buffer-full detector
	szBuffer[ cch - 1 ] = L'\0';
	
	va_list args;
	va_start( args, szFormat );
	const int iWritten = _vsnwprintf( szBuffer, cch - 1, szFormat, args );
	va_end( args );
	
	if( -1 == iWritten )
	{
		if( szBuffer[ cch - 1 ] != L'\0' ) // buffer-full
		{
			// ERANGE, not EINVAL (VC2005)
			errno = ERANGE;
		}
		else // not buffer-full (when you try to write U+FFFF into szBuffer)
		{
			errno = 0; // like VC2005/2008
		}
		*szBuffer = L'\0';
		return -1;
	}
	else if( (int) cch - 1 == iWritten )
	{
		szBuffer[ cch - 1 ] = L'\0';
	}
	return iWritten;
}

errno_t strcpy_s(
   char *strDestination,
   size_t numberOfElements,
   const char *strSource 
)
{
	if( ! strDestination || ! strSource )
	{
		return EINVAL;
	}

	const size_t cch = strlen( strSource ) + 1; // '\0'
	if( numberOfElements < cch )
	{
		return ERANGE;
	}
	
	memcpy( strDestination, strSource, cch * sizeof( char ) );
	return 0;
}

errno_t wcscpy_s(
   wchar_t *strDestination,
   size_t numberOfElements,
   const wchar_t *strSource 
)
{
	if( ! strDestination || ! strSource ) return EINVAL;

	const size_t cch = wcslen( strSource ) + 1; // '\0'
	if( numberOfElements < cch ) return ERANGE;
	
	memcpy( strDestination, strSource, cch * sizeof( wchar_t ) );
	return 0;
}

errno_t strcat_s(
   char *strDestination,
   size_t numberOfElements,
   const char *strSource 
)
{
	size_t i;
	if( ! strDestination || ! strSource )
	{
		return EINVAL;
	}
	
	if( numberOfElements == 0 )
	{
		return ERANGE;
	}

	for( i = 0; i < numberOfElements; ++i ) if( strDestination[ i ] == '\0' ) break;

	// Unterminated
	if( i == numberOfElements )
	{
		return EINVAL;
	}
	
	const size_t cchUsed = i;
	const size_t cchFree = numberOfElements - cchUsed;

	for( i = 0; i < cchFree; ++i ) if( strSource[ i ] == '\0' ) break;

	if( i == cchFree )
	{
		return ERANGE;
	}
	
	const size_t cchRequired = i + 1;

	memcpy( &strDestination[ cchUsed ], 
		strSource,
		sizeof( strDestination[ 0 ] ) * cchRequired );
	
	return 0;
}


errno_t wcscat_s(
   wchar_t *strDestination,
   size_t numberOfElements,
   const wchar_t *strSource 
)
{
	size_t i;
	if( strDestination == NULL || strSource == NULL )
	{
		return EINVAL;
	}
	
	if( numberOfElements == 0 )
	{
		return ERANGE;
	}

	for( i = 0; i < numberOfElements; ++i ) if( strDestination[ i ] == L'\0' ) break;

	// Unterminated
	if( i == numberOfElements )
	{
		return EINVAL;
	}
	
	const size_t cchUsed = i;
	const size_t cchFree = numberOfElements - cchUsed;

	for( i = 0; i < cchFree; ++i ) if( strSource[ i ] == L'\0' ) break;

	if( i == cchFree )
	{
		return ERANGE;
	}
	
	const size_t cchRequired = i + 1;

	memcpy( &strDestination[ cchUsed ], 
		strSource, sizeof( strDestination[ 0 ] ) * cchRequired );
	
	return 0;
}



errno_t fopen_s( 
   FILE** pFile,
   const char *filename,
   const char *mode 
)
{
	if( ! pFile || ! filename || ! mode )
	{
		errno = EINVAL;
		return EINVAL;
	}

	*pFile = fopen( filename, mode );
	errno_t ret = 0;
	if( ! pFile )
	{
		ret = errno;
		if( ! ret ) ret = EPERM; // (1) Operation not permitted 
	}
	return ret;
}

errno_t _wfopen_s(
   FILE** pFile,
   const wchar_t *filename,
   const wchar_t *mode 
)
{
	if( ! pFile || ! filename || ! mode )
	{
		errno = EINVAL;
		return EINVAL;
	}

	*pFile = _wfopen( filename, mode );
	errno_t ret = 0;
	if( ! pFile )
	{
		ret = errno;
		if( ! ret ) ret = EPERM; // (1) Operation not permitted 
	}
	return ret;
}
//
// wcsncpy_s
//
//
errno_t wcsncpy_s(
   wchar_t *strDest,
   size_t numberOfElements,
   const wchar_t *strSource,
   size_t count 
)
{
	if( ! strDest || ! strSource || numberOfElements == 0 )
	{
		errno = EINVAL;
		return EINVAL;
	}
	
	errno_t ret = (errno_t) 0;
	const size_t cchSource = wcslen( strSource );
	const size_t cchDst = ( count == _TRUNCATE )? numberOfElements - 1 : count ;
	
	const size_t D = __min( cchSource, cchDst );
	
	if( numberOfElements < D + 1 )
	{
		strDest[ 0 ] = L'\0';

		// VS2005 returns ERANGE in this case, altough
		// msdn says EINVAL:
		// http://msdn.microsoft.com/en-us/library/5dae5d43(VS.80).aspx
		errno = ERANGE;
		ret = ERANGE;
	}
	else
	{
		memcpy( strDest, strSource, sizeof( strDest[ 0 ] ) * D );
		strDest[ D ] = L'\0';

		if( cchDst < cchSource && count == _TRUNCATE )
		{
#ifndef STRUNCATE
#define STRUNCATE 80
#endif
			ret = STRUNCATE;
		}
	}
	return ret;
}

errno_t strncpy_s(
   char *strDest,
   size_t numberOfElements,
   const char *strSource,
   size_t count
)
{
	if( ! strDest || ! strSource || numberOfElements == 0 )
	{
		errno = EINVAL;
		return EINVAL;
	}
	
	errno_t ret = (errno_t) 0;
	
	const size_t cchSource = strlen( strSource );

	const size_t cchDst = ( count == _TRUNCATE )? numberOfElements - 1 : count;
	const size_t D = __min( cchSource, cchDst );
	
	if( numberOfElements < D + 1 )
	{
		strDest[ 0 ] = '\0';

		// VS2005 returns ERANGE in this case, altough
		// msdn says EINVAL:
		// http://msdn.microsoft.com/en-us/library/5dae5d43(VS.80).aspx
		errno = ERANGE;
		ret = ERANGE;
	}
	else
	{
		memcpy( strDest, strSource, sizeof( strDest[ 0 ] ) * D );
		strDest[ D ] = '\0';

		if( cchDst < cchSource && count == _TRUNCATE )
		{
			ret = STRUNCATE;
		}
	}
		
	return ret;
}

//
// strncat_s, wcsncat_s
//
errno_t strncat_s(
   char *strDest,
   size_t numberOfElements,
   const char *strSource,
   size_t count
)
{
	if( ! strDest || ! strSource || numberOfElements == 0 )
	{
		if( strDest ) *strDest = '\0';
		errno = EINVAL;
		return EINVAL;
	}
	
	errno_t ret = (errno_t) 0;
	
	size_t cchUsed = 0;
	
	while( cchUsed < numberOfElements && strDest[ cchUsed ] )
	{
		++cchUsed;
	}

	if( cchUsed == numberOfElements )
	{
		*strDest = '\0';
		errno = EINVAL;
		return EINVAL;
	}

	const size_t cchFree = numberOfElements - cchUsed;

	// cchMaxCat doesn't include the NUL.
	const size_t cchMaxCat = ( count == _TRUNCATE )? cchFree - 1 : count;

	size_t cchSource = 0;

	// cchSource doesn't include the NUL.
	while( cchSource <= cchMaxCat && strSource[ cchSource ] )
	{
		++cchSource;
	}

	// D doesn't include the NUL.
	const size_t D = __min( cchSource, cchMaxCat );
	
	if( cchFree < D + 1 )
	{
		*strDest = '\0';

		errno = ERANGE;
		ret = ERANGE;
	}
	else
	{
		memcpy( strDest + cchUsed, strSource, sizeof( strDest[ 0 ] ) * D );
		strDest[ cchUsed + D ] = '\0';

		if( cchFree < cchSource + 1 && count == _TRUNCATE )
		{
			ret = STRUNCATE;
		}
	}
		
	return ret;
}

errno_t wcsncat_s(
   wchar_t *strDest,
   size_t numberOfElements,
   const wchar_t *strSource,
   size_t count
)
{
	if( ! strDest || ! strSource || numberOfElements == 0 )
	{
		if( strDest ) *strDest = L'\0';
		
		errno = EINVAL;
		return EINVAL;
	}
	
	errno_t ret = (errno_t) 0;
	
	size_t cchUsed = 0;
	
	while( cchUsed < numberOfElements && strDest[ cchUsed ] )
	{
		++cchUsed;
	}

	if( cchUsed == numberOfElements )
	{
		*strDest = L'\0';
		errno = EINVAL;
		return EINVAL;
	}

	const size_t cchFree = numberOfElements - cchUsed;

	// cchMaxCat doesn't include the NUL.
	const size_t cchMaxCat = ( count == _TRUNCATE )? cchFree - 1 : count;

	size_t cchSource = 0;

	// cchSource doesn't include the NUL.
	while( cchSource <= cchMaxCat && strSource[ cchSource ] )
	{
		++cchSource;
	}

	// D doesn't include the NUL.
	const size_t D = __min( cchSource, cchMaxCat );
	
	if( cchFree < D + 1 )
	{
		*strDest = L'\0';

		errno = ERANGE;
		ret = ERANGE;
	}
	else
	{
		memcpy( strDest + cchUsed, strSource, sizeof( strDest[ 0 ] ) * D );
		strDest[ cchUsed + D ] = L'\0';

		if( cchFree < cchSource + 1 && count == _TRUNCATE )
		{
			ret = STRUNCATE;
		}
	}
		
	return ret;
}


int _snprintf_s(
   char *buffer,
   size_t sizeOfBuffer,
   size_t count,
   const char *format,
      ... 
)
{
	if( ! buffer
		|| ! format
		|| count != _TRUNCATE && (SSIZE_T) count < 0
	)
	{
		if( buffer ) *buffer = '\0';

		errno = EINVAL;
		return -1;
	}

	// buffer-full detector
	if( 1 <= sizeOfBuffer ) buffer[ sizeOfBuffer - 1 ] = '\0';

	// The following works even if count == 0, when iRetValue is 0 if
	// format is something like "", or -1 otherwise.
	
	const size_t cch = ( count == _TRUNCATE )? sizeOfBuffer
		: __min( sizeOfBuffer, count )
		;

	va_list args;
	va_start( args, format );
	
#if ( defined( _MSC_VER ) )
	int iRetValue = _vsnprintf( buffer, cch, format, args );
#else
	int iRetValue = vsnprintf( buffer, cch, format, args );
#endif

	// Buffer full, Truncation, or Error
	if( cch <= (size_t) iRetValue
		||
		iRetValue < 0
	)
	{
		if( iRetValue == -1 && 1 <= sizeOfBuffer && buffer[ sizeOfBuffer - 1 ] == '\0' )
		{
			*buffer = '\0';
			errno = 0;
		}
		// Expected truncation
		else if( count == _TRUNCATE )
		{
			buffer[ sizeOfBuffer - 1 ] =  '\0';
			iRetValue = -1;
		}
		// Buffer size is ok: if iRetValue==cch, even no truncation is needed
		else if( count < sizeOfBuffer )
		{
			buffer[ count ] =  '\0';
			iRetValue = ( cch == (size_t) iRetValue )? (int) count : -1 ;
		}
		else
		{
			*buffer =  '\0';
			iRetValue = -1;

			errno = ERANGE;
		}
	}

	va_end( args );

	return iRetValue;
}

int _snwprintf_s(
   wchar_t *buffer,
   size_t sizeOfBuffer,
   size_t count,
   const wchar_t *format,
      ... 
)
{
	if( ! buffer
		|| ! format
		|| count != _TRUNCATE && (SSIZE_T) count < 0
	)
	{
		if( buffer ) *buffer = L'\0';

		errno = EINVAL;
		return -1;
	}

	if( 1 <= sizeOfBuffer ) buffer[ sizeOfBuffer - 1 ] = L'\0';
	
	// The following works even if count == 0, when iRetValue is 0 if
	// format is something like "", or -1 otherwise.
	
	const size_t cch = ( count == _TRUNCATE )? sizeOfBuffer
		: __min( sizeOfBuffer, count )
		;

	va_list args;
	va_start( args, format );
	
	int iRetValue = _vsnwprintf( buffer, cch, format, args );

	// Buffer full, Truncation, or Error
	if( cch <= (size_t) iRetValue
		||
		iRetValue < 0
	)
	{
		// error involving U+FFFF
		if( iRetValue == -1 && 1 <= sizeOfBuffer && buffer[ sizeOfBuffer - 1 ] == L'\0' )
		{
			*buffer =  L'\0';
			errno = 0; // like VC2005/2008
		}
		// Expected truncation
		else if( count == _TRUNCATE )
		{
			buffer[ sizeOfBuffer - 1 ] =  L'\0';
			iRetValue = -1;
		}
		// Buffer size is ok: if iRetValue==cch, even no truncation is needed
		else if( count < sizeOfBuffer )
		{
			buffer[ count ] =  L'\0';
			iRetValue = ( cch == (size_t) iRetValue )? (int) count : -1 ;
		}
		else
		{
			*buffer =  L'\0';
			iRetValue = -1;

			errno = ERANGE;
		}
	}
	
	va_end( args );

	return iRetValue;
}

#endif // < VC2005





HFONT MyCreateFont( HDC hDC, LPCTSTR lpszFace, int iPoint, BOOL bBold, BOOL bItalic )
{
	return CreateFont(
				-MulDiv( iPoint, GetDeviceCaps( hDC, LOGPIXELSY ), 72 ),
				0, 0, 0,
				bBold ? FW_BOLD : FW_NORMAL,
				(DWORD) (!! bItalic ),
				0UL, 0UL,
				DEFAULT_CHARSET,
				OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,
				DEFAULT_QUALITY,
				FF_SWISS | VARIABLE_PITCH,
				lpszFace
			);
}

HANDLE
CreateThread2(
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  void * lpVoid
)
{
	ULONG_PTR uThread;
	
	// On VC6 _beginthreadex returns unsigned long (bad for Win64)
	uThread = _beginthreadex( 
						NULL,
						0,
						lpStartAddress,
						lpVoid,
						0,
						NULL );
	
	if( ! uThread )
	{
		uThread = _beginthreadex( 
						NULL,
						127 * 1024,
						lpStartAddress,
						lpVoid,
						0,
						NULL );
	}

	return (HANDLE) uThread;
}

void OpenBrowser( LPCTSTR lpUrl )
{
	ShellExecute(
			(HWND) NULL,
			TEXT( "open" ),
			lpUrl,
			NULL,
			NULL, // lpszCurrentDir,
			SW_SHOWNORMAL
		);
}

bool IsMenuChecked( HMENU hMenu, int idm )
{
	return !!( MF_CHECKED & GetMenuState( hMenu, (UINT) idm, MF_BYCOMMAND ) );
}
bool IsMenuChecked( HWND hWnd, int idm )
{
	return !!( MF_CHECKED & GetMenuState( GetMenu( hWnd ), (UINT) idm, MF_BYCOMMAND ) );
}
CLEAN_POINTER LPVOID MemAlloc( rsize_t cb, size_t n )
{
	return HeapAlloc( GetProcessHeap(), 0, cb * n );
}
CLEAN_POINTER TCHAR * TcharAlloc( size_t cch )
{
	return static_cast<TCHAR *>( HeapAlloc( GetProcessHeap(), 0, cch * sizeof(TCHAR) ) );
}
void MemFree( LPVOID lp )
{
	HeapFree( GetProcessHeap(), 0, lp );
}
