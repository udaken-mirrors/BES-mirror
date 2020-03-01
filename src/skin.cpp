/* 
 *	Copyright (C) 2005-2014 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"

static HBITMAP HandleToMemDC( HWND hWnd, HANDLE hFile, HDC hMemDC, SIZE& SkinSize );

#if 0
static void GetMaxPicSize( SIZE * pMaxPic )
{
	pMaxPic->cx = GetSystemMetrics( SM_CXSCREEN );

	if( pMaxPic->cx < 640L )
		pMaxPic->cx = 640L;

	pMaxPic->cy = pMaxPic->cx * 3L / 4L; // 480:640
}

static void FillBackground( HDC hMemDC, const SIZE& maxpic )
{
	HBRUSH hBrush = CreateSolidBrush( RGB( 0x00, 0x22, 0x44 ) );
	HBRUSH hOldBrush = SelectBrush( hMemDC, hBrush );

	// PatBlt should be faster since it doesn't handle Pen.
	//Rectangle( hMemDC, 0, 0, iMaxX, iMaxY );
	PatBlt( hMemDC, 0, 0, maxpic.cx, maxpic.cy, PATCOPY );
		
	SelectBrush( hMemDC, hOldBrush );
	DeleteBrush( hBrush );
}
#endif

HBITMAP LoadSkin( HWND hWnd, HDC hMemDC, SIZE& SkinSize )
{
	if( ! hMemDC )
		return NULL;

	TCHAR szFile[ CCHPATH ] = TEXT("");

	const TCHAR * strIniPath = GetIniPath();
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "Skin" ),
		NULL,
		szFile,
		CCHPATH,
		strIniPath
	);

	HANDLE hFile = INVALID_HANDLE_VALUE;
#define CreateFile_(szFile)	CreateFile(szFile,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL)
	// Skin is set in .ini
	if( szFile[ 0 ] )
	{
		// Load the custom skin
		hFile = CreateFile_( szFile );
	}

	TCHAR szDir[ CCHPATH ] = TEXT("");

	// If a custom skin was not loaded...
	if( hFile == INVALID_HANDLE_VALUE )
	{
		GetModuleFileName( NULL, szDir, _countof(szDir) );
		TCHAR * pBkSlash = _tcsrchr( szDir, _T('\\') );
		if( pBkSlash && szDir < pBkSlash )
			*pBkSlash = _T('\0');
		else
			GetCurrentDirectory( _countof(szDir), szDir );

		_stprintf_s( szFile, _countof(szFile), _T("%s\\skin.jpg"), szDir );
		hFile = CreateFile_( szFile );
	}

	if( hFile == INVALID_HANDLE_VALUE )
	{
		_stprintf_s( szFile, _countof(szFile), _T("%s\\skin.bmp"), szDir );
		hFile = CreateFile_( szFile );
	}

	if( hFile == INVALID_HANDLE_VALUE )
	{
		_stprintf_s( szFile, _countof(szFile), _T("%s\\defskin.jpg"), szDir );
		hFile = CreateFile_( szFile );
	}

	if( hFile == INVALID_HANDLE_VALUE )
	{
		_stprintf_s( szFile, _countof(szFile), _T("%s\\skin.gif"), szDir );
		hFile = CreateFile_( szFile );
	}

	if( hFile == INVALID_HANDLE_VALUE )
	{
		_tcscpy_s( szFile, _countof(szFile), szDir );
		TCHAR * pBkSlash = _tcsrchr( szFile, _T('\\') ); // upper directory
		if( pBkSlash && szFile < pBkSlash )
		{
			*pBkSlash = _T('\0');
			_tcscat_s( szFile, _countof(szFile), _T( "\\defskin.jpg" ) ); // file in the upper dir

			hFile = CreateFile_( szFile );
		}
	}

	HBITMAP hOrigBmp = NULL;
	if( hFile != INVALID_HANDLE_VALUE )
	{
		hOrigBmp = HandleToMemDC( hWnd, hFile, hMemDC, SkinSize );
		CloseHandle( hFile );
	}

	return hOrigBmp;
}


BOOL ChangeSkin( HWND hWnd, HDC hMemDC, SIZE& SkinSize, HBITMAP& hOrigBmp )
{
	if( ! hMemDC )
		return FALSE;

	const TCHAR * strIniPath = GetIniPath();

	TCHAR szFile[ CCHPATH ] = _T( "" );
	TCHAR szDir[ CCHPATH ] = _T( "" );
	
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "SkinDir" ),
		NULL,
		szDir,
		CCHPATH,
		strIniPath
	);

	if( *szDir == _T('\0') )
	{
		if( ! SUCCEEDED( SHGetFolderPath( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, szDir ) ) )
			GetCurrentDirectory( CCHPATH, szDir );
	}
	
	OPENFILENAME ofn = {0};
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.lpstrTitle = _T( "Skin" );
	ofn.lpstrInitialDir = szDir;
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = 
		TEXT( "Image (*.jpg, *.bmp)\0" )
			TEXT( "*.jpg;*.jpeg;*.jpe;*.jfif;*.bmp;*.wmf;*.gif;*.ico\0" )
		TEXT( "All Files\0" )
			TEXT( "*.*\0" )
		TEXT( "\0" );
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = CCHPATH;
	ofn.lpstrDefExt = TEXT("jpg");
#ifdef _UNICODE
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING | OFN_DONTADDTORECENT;
#else
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_ENABLESIZING;
#endif

	if( ! GetOpenFileName( &ofn ) )
	{
		return FALSE;
	}

	if( 1 <= ofn.nFileOffset && ofn.nFileOffset < _countof(szDir) )
	{
		_tcscpy_s( szDir, _countof(szDir), szFile );
		szDir[ ofn.nFileOffset - 1 ] = _T('\0');
		WritePrivateProfileString(
			TEXT( "Options" ), 
			TEXT( "SkinDir" ),
			szDir,
			strIniPath
		);
	}

	HANDLE hFile = CreateFile_( szFile );

	if( hFile == INVALID_HANDLE_VALUE )
	{
		MessageBox( hWnd, szFile, NULL, MB_ICONEXCLAMATION | MB_OK );
		return FALSE;
	}
	
#ifdef _DEBUG
	TCHAR s[100];
#endif

	if( hOrigBmp )
	{
		HBITMAP hBmp = SelectBitmap( hMemDC, hOrigBmp );

#ifdef _DEBUG
swprintf_s(s,100,L"\t&\tDeleting %p, Orig %p\n",hBmp,hOrigBmp);
OutputDebugString(s);
#endif

		DeleteBitmap( hBmp );
		hOrigBmp = NULL;
	}

	hOrigBmp = HandleToMemDC( hWnd, hFile, hMemDC, SkinSize );
	CloseHandle( hFile );

#ifdef _DEBUG
swprintf_s(s,100,L"\t&\tOrig %p\n",hOrigBmp);
OutputDebugString(s);
#endif

	if( hOrigBmp )
	{
		WritePrivateProfileString(
			TEXT( "Options" ), 
			TEXT( "Skin" ),
			szFile,
			strIniPath
		);
	}
	else
	{
		MessageBox( hWnd, szFile,
			TEXT( "Skin Changer Failed" ),
			MB_OK | MB_ICONEXCLAMATION );
	}

	InvalidateRect( hWnd, NULL, TRUE );

	return ( hOrigBmp != NULL );
}





static HBITMAP HandleToMemDC( HWND hWnd, HANDLE hFile, HDC hMemDC, SIZE& SkinSizeResult )
{
	SkinSizeResult.cx = 0L;
	SkinSizeResult.cy = 0L;
	SIZE mySkinSize = {0};

	if( ! hMemDC )
		return NULL;

	if( hFile == INVALID_HANDLE_VALUE ) return FALSE;

	HBITMAP hOrigBmp = NULL;

	HGLOBAL hGlobal = NULL;
	IStream  * pIst = NULL;
	IPicture * pIpic = NULL;

	DWORD dwFileSize = GetFileSize( hFile, NULL );
	BOOL bReadOK = FALSE;
	hGlobal = GlobalAlloc( GMEM_MOVEABLE, dwFileSize );
	if( hGlobal )
	{
		LPVOID lpv = GlobalLock( hGlobal );
		if( lpv )
		{
			DWORD dwBytes = 0L;
			if( ReadFile( hFile, lpv, dwFileSize, &dwBytes, NULL ) && dwBytes == dwFileSize )
				bReadOK = TRUE;
			GlobalUnlock( hGlobal );
			lpv = NULL;
		}

		CreateStreamOnHGlobal( hGlobal, FALSE, &pIst );
			
		if( ! bReadOK
			||
			S_OK != OleLoadPicture( pIst, 0L, FALSE, IID_IPicture, (LPVOID*) &pIpic )
		)
		{
			if( pIpic != NULL )
			{
				pIpic->Release();
				pIpic = NULL;
			}

			if( pIst != NULL )
			{
				pIst->Release();
				pIst = NULL;
			}

			GlobalFree( hGlobal );
			hGlobal = NULL;
			
			return NULL;
		}

		long hmWidth  = 0L;
		long hmHeight = 0L;
		pIpic->get_Width( &hmWidth );
		pIpic->get_Height( &hmHeight );

		mySkinSize.cx = MulDiv( hmWidth, GetDeviceCaps( hMemDC, LOGPIXELSX ), HIMETRIC_INCH );
		mySkinSize.cy = MulDiv( hmHeight, GetDeviceCaps( hMemDC, LOGPIXELSY ), HIMETRIC_INCH );

		double ar = (double) mySkinSize.cx / (double) mySkinSize.cy;
		SIZE CanvasSize = mySkinSize;
		if( CanvasSize.cx != 640L || CanvasSize.cy != 480L )
		{
			if( ar > 640.0 / 480.0 )
			{
				CanvasSize.cy = (LONG)( CanvasSize.cx * 480.0 / 640.0 + 0.5 );
			}
			else
			{
				CanvasSize.cx = (LONG)( CanvasSize.cy * 640.0 / 480.0 + 0.5 );
			}
		}

		HDC _hdc = GetDC( hWnd );

#ifdef _DEBUG
TCHAR s[100];
SetLastError(0);
#endif

		// This may fail with error code 8 (Not enough storage is available to process this command)
		HBITMAP hBmp = CreateCompatibleBitmap( _hdc, CanvasSize.cx, CanvasSize.cy );

#ifdef _DEBUG
swprintf_s(s,100,L"\t&\tCreated %p e=%d\n",hBmp,GetLastError());
OutputDebugString(s);
#endif

		ReleaseDC( hWnd, _hdc );
		_hdc = NULL;

		if( hBmp )
		{
			hOrigBmp = SelectBitmap( hMemDC, hBmp );

			if( hOrigBmp )
			{
				HBRUSH hbrOld = SelectBrush( hMemDC, GetStockBrush( BLACK_BRUSH ) );
				// Faster than Rectangle( hdc, 0, 0, 640, 480 );
				PatBlt( hMemDC, 0, 0, CanvasSize.cx, CanvasSize.cy, PATCOPY );
				SelectBrush( hMemDC, hbrOld );

				RECT dummy = {0}; // not necessary; only to silence the compiler warning
				pIpic->Render( hMemDC, 0L, 0L, mySkinSize.cx, mySkinSize.cy,
								0L, hmHeight, hmWidth, -hmHeight, &dummy );

				SkinSizeResult = mySkinSize;
			}
			else
			{
				DeleteBitmap( hBmp );
				hBmp = NULL;
			}
		}

#ifdef _DEBUG
swprintf_s(s,100,L"\t&\thOrigBmp=%p\n",hOrigBmp);
OutputDebugString(s);
#endif
	} // hGlobal

	if( pIpic != NULL )
	{
		pIpic->Release();
		pIpic = NULL;
	}

	if( pIst != NULL )
	{
		pIst->Release();
		pIst = NULL;
	}
	
	if( hGlobal != NULL )
	{
		GlobalFree( hGlobal );
		hGlobal = NULL;
	}

	return hOrigBmp;
}

BOOL DrawSkin( HDC hdc, HDC hMemDC, const SIZE& SkinSize )
{
	if( ! hMemDC || SkinSize.cx <= 0L || SkinSize.cy <= 0L )
	{
		HBRUSH hbrOld = SelectBrush( hdc, GetStockBrush( BLACK_BRUSH ) );
		
		// PatBlt should be faster since it doesn't handle Pen.
		//Rectangle( hdc, 0, 0, 640, 480 );
		PatBlt( hdc, 0, 0, 640, 480, PATCOPY );
		
		SelectBrush( hdc, hbrOld );
		return FALSE;
	}

	if( SkinSize.cx != 640 || SkinSize.cy != 480 )
	{
		SIZE CanvasSize = SkinSize;
		if( CanvasSize.cx != 640L || CanvasSize.cy != 480L )
		{
			double ar = (double) SkinSize.cx / (double) SkinSize.cy;
			if( ar > 640.0 / 480.0 )
			{
				CanvasSize.cy = (LONG)( CanvasSize.cx * 480.0 / 640.0 + 0.5 );
			}
			else
			{
				CanvasSize.cx = (LONG)( CanvasSize.cy * 640.0 / 480.0 + 0.5 );
			}
		}

		SetStretchBltMode( hdc, HALFTONE );
		SetBrushOrgEx( hdc, 0, 0, NULL );

		StretchBlt( hdc, 0, 0, 640, 480,
					hMemDC, 0, 0, (int) CanvasSize.cx, (int) CanvasSize.cy, SRCCOPY );

#if 0//def _DEBUG
TCHAR s[100];swprintf_s(s,100,L"\t&\t%d x %d skin\n",SkinSize.cx,SkinSize.cy);
OutputDebugString(s);
#endif

	}
	else BitBlt( hdc, 0, 0, 640, 480, hMemDC, 0, 0, SRCCOPY );

	return TRUE;
}
