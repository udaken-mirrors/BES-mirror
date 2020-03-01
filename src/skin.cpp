#include "BattleEnc.h"

static BOOL HandleToMemDC( HANDLE hFile, HDC hMemDC, SIZE& PicSize );

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

HBITMAP LoadSkin( HWND hWnd, HDC& hMemDC, SIZE& PicSize )
{
	// Default values
	PicSize.cx = 640L;
	PicSize.cy = 480L;

	HDC hdc = GetDC( hWnd );
	hMemDC = CreateCompatibleDC( hdc );
	if( ! hMemDC )
	{
		ReleaseDC( hWnd, hdc );
		return NULL;
	}

	SIZE maxpic;
	GetMaxPicSize( &maxpic );

	HBITMAP hBmp = CreateCompatibleBitmap( hdc, maxpic.cx, maxpic.cy );
	if( ! hBmp )
	{
		DeleteDC( hMemDC );
		hMemDC = NULL;
		ReleaseDC( hWnd, hdc );
		return NULL;
	}
	
	HBITMAP hOrigBmp = SelectBitmap( hMemDC, hBmp );
	if( ! hOrigBmp )
	{
		DeleteBitmap( hBmp );
		hBmp = NULL;
	}

	FillBackground( hMemDC, maxpic );

	ReleaseDC( hWnd, hdc );

	TCHAR szFile[ MAX_PATH + 1 ] = TEXT("");

	const TCHAR * strIniPath = GetIniPath();
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "Skin" ),
		NULL,
		szFile,
		MAX_PATH,
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

	TCHAR szDir[ MAX_PATH + 1 ] = TEXT("");

	// If a custom skin not loaded...
	if( hFile == INVALID_HANDLE_VALUE )
	{
		GetModuleFileName( NULL, szDir, MAX_PATH + 1UL );
		TCHAR * pBackSlash = _tcsrchr( szDir, _T('\\') );
		if( pBackSlash )
			*pBackSlash = _T('\0');

		if( szDir[ 0 ] == TEXT( '\0' ) )
			GetCurrentDirectory( MAX_PATH, szDir );

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
		TCHAR * pBackSlash = _tcsrchr( szFile, _T('\\') );
		if( pBackSlash )
		{
			*pBackSlash = _T('\0');
			_tcscat_s( szFile, _countof(szFile), _T( "\\defskin.jpg" ) );

			hFile = CreateFile_( szFile );
		}
	}

	if( hFile != INVALID_HANDLE_VALUE )
	{
		HandleToMemDC( hFile, hMemDC, PicSize );
		CloseHandle( hFile );
	}

	return hOrigBmp;
}


BOOL ChangeSkin( HWND hWnd, HDC hMemDC, SIZE& PicSize, HBITMAP& hOrigBmp )
{
	if( ! hMemDC )
		return FALSE;

	const TCHAR * strIniPath = GetIniPath();

	TCHAR szFile[ MAX_PATH + 1 ] = _T( "" );
	TCHAR szDir[ MAX_PATH + 1 ] = _T( "" );
	
	GetPrivateProfileString(
		TEXT( "Options" ),
		TEXT( "SkinDir" ),
		TEXT( "" ),
		szDir,
		MAX_PATH,
		strIniPath
	);

	if( *szDir == _T('\0') )
	{
		if( GetShell32Version() < 5 || ! SUCCEEDED(
				SHGetFolderPath( NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, szDir )
			)
		) 
		{
			GetCurrentDirectory( MAX_PATH + 1, szDir );
		}
	}
	
	OPENFILENAME ofn;
	ZeroMemory( &ofn, sizeof( OPENFILENAME ) );
	ofn.lStructSize = sizeof( OPENFILENAME );
	ofn.lpstrTitle = _T( "Skin" );
	ofn.lpstrInitialDir = szDir;
	ofn.hwndOwner = hWnd;
	ofn.lpstrFilter = 
		TEXT( "Skin Image (*.jpg, *.bmp)\0" )
			TEXT( "*.jpg;*.jpeg;*.jpe;*.jfif;*.bmp;*.wmf;*.gif;*.ico\0" )
		TEXT( "All files (*.*)\0" )
			TEXT( "*.*\0" )
		TEXT( "\0" );
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH + 1;
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
	

/*
	const int iMaxX = GetSystemMetrics( SM_CXSCREEN );
	const int iMaxY = iMaxX * 480 / 640;
	HBITMAP hBmp = CreateCompatibleBitmap( hMemDC, iMaxX, iMaxY );

	HBITMAP hPrevBmp = SelectBitmap( hMemDC, hBmp );
	if( hOrigBmp )
	{
		DeleteBitmap( hPrevBmp );
	}
*/

	SIZE maxpic;
	GetMaxPicSize( &maxpic );

	if( ! hOrigBmp )
	{
		HBITMAP hBmp = CreateCompatibleBitmap( hMemDC, maxpic.cx, maxpic.cy );
		hOrigBmp = SelectBitmap( hMemDC, hBmp );
	}

	FillBackground( hMemDC, maxpic );
	/*HBRUSH hBrush = CreateSolidBrush( RGB( 0x00, 0x22, 0x44 ) );
	HBRUSH hOldBrush = SelectBrush( hMemDC, hBrush );
	Rectangle( hMemDC, 0, 0, iMaxX, iMaxY );
	SelectBrush( hMemDC, hOldBrush );
	DeleteBrush( hBrush );
	//DeleteBitmap( hBmp );*/
	
	BOOL bSuccess = HandleToMemDC( hFile, hMemDC, PicSize );
	CloseHandle( hFile );

	InvalidateRect( hWnd, NULL, TRUE );

	if( bSuccess )
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
	return bSuccess;

}





static BOOL HandleToMemDC( HANDLE hFile, HDC hMemDC, SIZE& PicSize )
{
	//PicSize.cx = PicSize.cy = 0L;
	if( hFile == INVALID_HANDLE_VALUE ) return FALSE;

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
			if( hGlobal != NULL )
			{
				GlobalFree( hGlobal );
				hGlobal = NULL;
			}
			
			// Default
			PicSize.cx = 640L;
			PicSize.cy = 480L;
			return FALSE;
		}

		long hmWidth  = 0L;
		long hmHeight = 0L;
		pIpic->get_Width( &hmWidth );
		pIpic->get_Height( &hmHeight );

		PicSize.cx = MulDiv( hmWidth,
					GetDeviceCaps( hMemDC, LOGPIXELSX ), HIMETRIC_INCH );
		PicSize.cy = MulDiv( hmHeight,
					GetDeviceCaps( hMemDC, LOGPIXELSY ), HIMETRIC_INCH );

		SIZE maxpic;
		GetMaxPicSize( &maxpic );
		if( PicSize.cx > maxpic.cx ) PicSize.cx = maxpic.cx;
		if( PicSize.cy > maxpic.cy ) PicSize.cy = maxpic.cy;

//#if defined(_MSC_VER) && 1400 <= _MSC_VER
//# pragma warning (push)
//# pragma warning (disable:6309) // PREFast: C6309: Argument '10' is null: this does not adhere to function specification of 'IPicture::Render'
		const RECT dummy = { 0L, 0L, 640L, 480L };
		pIpic->Render( hMemDC,
					0L, 0L,
					PicSize.cx, PicSize.cy,
					0L, hmHeight, hmWidth, -hmHeight,
					&dummy
		);
//# pragma warning (pop)
//#endif
		double ar = (double) PicSize.cx / (double) PicSize.cy;
		if( PicSize.cx != 640L || PicSize.cy != 480L )
		{
			if( ar > 640.0 / 480.0 )
			{
				PicSize.cy = ( long ) floor( PicSize.cx * 480.0 / 640.0 );
			}
			else
			{
				PicSize.cx = ( long ) floor( PicSize.cy * 640.0 / 480.0 );
			}
		}
	}

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
	return TRUE;
}





BOOL DrawSkin( HDC hdc, HDC hMemDC, SIZE& SkinSize )
{
	if( ! hMemDC || SkinSize.cx <= 0L || SkinSize.cy <= 0L )
	{
		return FALSE;
	}

	SetStretchBltMode( hdc, HALFTONE );
	SetBrushOrgEx( hdc, 0, 0, NULL );
	StretchBlt( hdc, 0, 0, 640, 480,
				hMemDC, 0, 0, (int) SkinSize.cx, (int) SkinSize.cy, SRCCOPY );
	return TRUE;
}



