/* 
 *	Copyright (C) 2005-2019 mion
 *	http://mion.faireal.net
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License.
 */

#include "BattleEnc.h"
#ifndef INT_MAX
# include <limits.h>
#endif

//#define PTRSORT

typedef struct tagProcessInfo {
	TCHAR * szPath;
	const TCHAR * pExe;

	TCHAR * szText;
	DWORD dwProcessId;

	DWORD numOfThreads;
	WORD slotid;
	bool fWatched;
	bool fUnlimited;

	int iIFF;
	short _cpu;
	bool fWatch;
	bool fThisIsBES;
} PROCESSINFO;

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hListDlg;
extern HFONT g_hFont;

extern volatile BOOL g_bHack[ MAX_SLOTS ];
extern volatile int g_Slider[ MAX_SLOTS ];
extern HANDLE hSemaphore[ MAX_SLOTS ];
extern HANDLE hChildThread[ MAX_SLOTS ];
extern TCHAR * ppszStatus[ 18 ];
BOOL g_bSelNextOnHide = FALSE;

TCHAR * g_lpszEnemy[ MAX_ENEMY_CNT ];
ptrdiff_t g_numOfEnemies = 0;
TCHAR * g_lpszFriend[ MAX_FRIEND_CNT ];
ptrdiff_t g_numOfFriends = 0;


static ptrdiff_t s_nSortedPairs = 0;
static PROCESS_THREAD_PAIR * s_lpSortedPairs = NULL;
HANDLE hReconSema = NULL;

#ifndef KEY_UP				
#define KEY_UP(vk)   (0 <= (int) GetKeyState(vk))
#endif




typedef struct tagCPUTime {
	ULONGLONG t;
	DWORD pid;
	DWORD dummy;
} CPUTIME;


void CachedList_Refresh( DWORD msMaxWait );
BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );

void WriteIni_Time( const TCHAR * pExe );
BOOL UpdateIFF_Ini( BOOL bCleanSliderSection );
BOOL VerifyOSVer( DWORD dwMajor, DWORD dwMinor, int iSP );
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );

BOOL LimitProcess_NoWatch( HWND hDlg, TARGETINFO * pTarget, PROCESSINFO& ProcessInfo, const int * aiParams, int mode = 0 ); // v1.7.5
TCHAR * PIDToPath_Alloc( DWORD dwProcessID, size_t * pcch ); // v1.7.5

static INT_PTR CALLBACK Question( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
static void _nuke( HWND hDlg, const PROCESSINFO& TgtInfo );
static void SetProcessInfoMsg( HWND hDlg, const PROCESSINFO& ti, int slotid );

static CLEAN_POINTER TCHAR * ProcessToPath_Alloc( HANDLE hProcess, size_t * pcch );
static bool IsAbsFoe2( const TCHAR * strExe, const TCHAR * strPath );
static BOOL CALLBACK GetImportantText_EnumProc( HWND hwnd, LPARAM lParam );

//#define MAX_WINDOWS_CNT (1024)
#define MAX_WINDOWS_CNT (280)

//#define CCH_ITEM_MAX    (128)
typedef struct tagWinInfo {
	TCHAR szTitle[ MAX_WINDOWS_CNT ][ MAX_WINDOWTEXT ];
	HWND hwnd[ MAX_WINDOWS_CNT ];
	ptrdiff_t numOfItems;
	DWORD_PTR pid; // unused (as of 2019-08-15)
} WININFO;

#define TIMER_ID (0x101u)

// WM_USER_REFRESH
//    wParam    PID to select; 0 to select the first item; -1 to reuse the currently selected PID
//    lParam    following flag(s) if necessary
#define URF_ENSURE_VISIBLE 0x0002 // ListView_EnsureVisible
#define URF_SORT_ALGO      0x0004 // change sort algo: HIWORD is the new sort_algo
#define URF_RESET_FOCUS    0x0010 // SetDlgFocus(hLV)

static BOOL CALLBACK WndEnumProc( HWND hwnd, LPARAM lParam )
{
	if( lParam )
	{
		WININFO& wi = *(WININFO *) lParam;
		if( wi.numOfItems < MAX_WINDOWS_CNT )
		{
			wi.hwnd[ wi.numOfItems ] = hwnd;
			if( ! GetWindowText( hwnd, wi.szTitle[ wi.numOfItems ], MAX_WINDOWTEXT ) )
				wi.szTitle[ wi.numOfItems ][ 0 ] = _T('\0');

			++wi.numOfItems;
			return TRUE;
		}
	}
	return FALSE;
}

#define FOE_COLOR    RGB( 0xee, 0x90, 0xee )
#define FRIEND_COLOR RGB( 0x00, 0xff, 0x66 )

#define TEXT_COLOR RGB( 0x00, 0x00, 0x99 )
#define BG_COLOR RGB( 0xff, 0xff, 0xcc )


/*
// Commctrl.h
#ifndef LVM_SETINFOTIP
typedef struct tagLVSETINFOTIP
{
    UINT cbSize;
    DWORD dwFlags;
    LPWSTR pszText;
    int iItem;
    int iSubItem;
} LVSETINFOTIP, *PLVSETINFOTIP;
# define  LVM_SETINFOTIP         (LVM_FIRST + 173)
# define ListView_SetInfoTip(hwndLV, plvInfoTip)\
        (BOOL)SNDMSG((hwndLV), LVM_SETINFOTIP, (WPARAM)0, (LPARAM)(plvInfoTip))
#endif
*/

enum eLVcols {
	SLOT,
	PID, IFF, EXE, CPU, TITLE,
	PATH,
	THREADS, // # of threads
	N_COLS
};

static void _init_ListView( HWND hDlg )
{
	LPTSTR colHeader[ N_COLS ]
	= { _T("Slot"), _T("PID "), _T("IFF"), _T("Name"), _T("CPU"), _T("Window Title"),
		_T("Path"), _T("Threads")
	};

	HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
	DWORD dwStyles = ListView_GetExtendedListViewStyle( hLV );
	dwStyles |= /*LVS_EX_FULLROWSELECT |*/ LVS_EX_GRIDLINES | LVS_EX_HEADERDRAGDROP;

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER     0x00010000
#endif
	if( VerifyOSVer( 5, 1, 0 ) )
	{
		dwStyles |= LVS_EX_DOUBLEBUFFER;
	}
	ListView_SetExtendedListViewStyle( hLV, (LPARAM)(DWORD_PTR) dwStyles );
	ListView_SetBkColor( hLV, BG_COLOR );

	LVCOLUMN col = {0};
	col.mask = LVCF_FMT | LVCF_ORDER | LVCF_SUBITEM | LVCF_TEXT;
	for( int col_idx = 0; col_idx < N_COLS; ++col_idx )
	{
		// The alignment of the leftmost column is always LVCFMT_LEFT; it cannot be changed.
		col.fmt =
			( col_idx == IFF ) ? LVCFMT_CENTER :
			( col_idx == PID || col_idx == CPU || col_idx == THREADS ) ? LVCFMT_RIGHT : 
			LVCFMT_LEFT ;
		col.pszText = colHeader[ col_idx ];
		col.iSubItem = col.iOrder = col_idx;
		ListView_InsertColumn( hLV, col.iOrder, &col );
	}

	TCHAR szColumnOrder[ 256 ] = _T("");
	if( GetPrivateProfileString( TEXT("Options"), TEXT("ColumnOrderArray"), NULL, 
									szColumnOrder, _countof(szColumnOrder), GetIniPath() ) )
	{
		ptrdiff_t i, j;
		INT rg[ N_COLS ];
		TCHAR *p = szColumnOrder, *endptr;
		for( i = 0; i < N_COLS; ++i )
		{
			rg[ i ] = (INT) _tcstol( p, &endptr, 10 );

			if( *endptr != _T(',') ) break;

			p = endptr + 1;
		}

		// Make sure that we have every one of 0, 1, ..., N_COLS, in this array.
		// This checking will be useful in the future if we change N_COLS.
		if( i == N_COLS )
		{
			for( i = 0; i < N_COLS; ++i )
			{
				for( j = 0; j < N_COLS && rg[ j ] != i; ++j ){ ; }
				if( j == N_COLS ) break; // Bad news; the value i is not found in the array
			}

			if( i == N_COLS ) // A good array
				ListView_SetColumnOrderArray( hLV, N_COLS, rg );
		}
	}
}


static void _init_DialogPos( HWND hDlg )
{
	TCHAR szDialogPos[ 256 ] = _T("");
	if( GetPrivateProfileString( TEXT("Window"), TEXT("DialogPos"), NULL, 
									szDialogPos, _countof(szDialogPos), GetIniPath() ) )
	{
		int x = 0, y = 0;
#ifdef _stscanf_s
		if( _stscanf_s( szDialogPos, _T("%d,%d"), &x, &y ) == 2 )
#else
		if( _stscanf( szDialogPos, _T("%d,%d"), &x, &y ) == 2 )
#endif
		{
			RECT rcWorkArea = {0};
			SystemParametersInfo( SPI_GETWORKAREA, 0, &rcWorkArea, 0 );
			const int xMax = __max( 0, rcWorkArea.right - 400 );
			const int yMax = __max( 0, rcWorkArea.bottom - 200 );
			x = __min( __max( 0, x ), xMax );
			y = __min( __max( 0, y ), yMax );

			SetWindowPos( hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
		}
	}
}


static void _init_Slider( HWND hDlg )
{
	const bool fAllowMoreThan99 = IsMenuChecked( g_hWnd, IDM_ALLOWMORETHAN99 );
	const int iMax = fAllowMoreThan99 ? SLIDER_MAX : 99 ;
	SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG( SLIDER_MIN, iMax ) );
	SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETPAGESIZE, 0, 5 );
	for( long lPos = 10L; lPos <= 90L; lPos += 10L )
		SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETTIC, 0, lPos );
}


static void ListView_SetCurrentSel( HWND hListview, ptrdiff_t sel, BOOL bEnsureVisible )
{
	const ptrdiff_t nItems = SendMessage( hListview, LVM_GETITEMCOUNT, 0, 0 );

	// min-max limits
	if( nItems <= sel ) sel = nItems - 1;
	if( sel < 0 ) sel = 0;

#define SEL_ ((UINT)(LVIS_FOCUSED|LVIS_SELECTED))
	for( ptrdiff_t i = 0; i < nItems; ++i )
		ListView_SetItemState( hListview, i, ( i != sel ? 0 : SEL_ ), SEL_ );

	if( bEnsureVisible )
		ListView_EnsureVisible( hListview, sel, FALSE );




}

static ptrdiff_t ListView_GetCurrentSel( HWND hLV )
{
// ListView_GetSelectedColumn only on XP
	return SendMessage( hLV, LVM_GETNEXTITEM, (WPARAM) -1, LVNI_ALL | LVNI_FOCUSED );
}

static void EnableDlgItem( HWND hDlg, int idCtrl, BOOL bEnable )
{
	HWND hwndCtrl = GetDlgItem( hDlg, idCtrl );
	
	// We're disabling a control that currently has keyboard focus.
	if( ! bEnable && GetFocus() == hwndCtrl )
	{
		// Focus the next item, so that the [tab] key handling may not get stuck.
		// In this case, PostMessage is not good; change focus now, before disabling it.
		SendMessage( hDlg, WM_NEXTDLGCTL, 0, MAKELPARAM( FALSE, 0 ) );
	}

	// Now it's safe to disable (or enable) it.
	EnableWindow( hwndCtrl, bEnable );
}

BOOL HasFreeSlot( void )
{
	for( ptrdiff_t g = 0; g < MAX_SLOTS; ++g )
	{ 
		if( g != 3 && ! g_bHack[ g ] ) return TRUE;
	}
	return FALSE;
}







static void LV_OnSelChange(
	HWND hDlg,
#ifdef PTRSORT
	const PROCESSINFO * const * rgProcessInfo,
#else
	const PROCESSINFO * rgProcessInfo,
#endif
	ptrdiff_t cItems,
	ptrdiff_t selectedItem,
	ptrdiff_t& curIdx
)
{
	if( selectedItem < 0 || cItems <= selectedItem || ! rgProcessInfo ) return;
	curIdx = selectedItem;

#ifdef PTRSORT
	const PROCESSINFO& Item = *(rgProcessInfo[ selectedItem ]);
#else
	const PROCESSINFO& Item = rgProcessInfo[ selectedItem ];
#endif
	const int current_IFF = Item.iIFF;

	BOOL bLimitable = TRUE;
	BOOL bUnlimitable = FALSE;
	extern ptrdiff_t g_nPathInfo;

	/// ***Tentative*** FIX @ 20190317 ## Item.slotid==MAX_SLOTS is possible; g_bHack[MAX_SLOTS-1] is the last element
	/// ***TODO*** probably the "! g_bHack[ Item.slotid ]" case should be removed here
	///	BOOL bWatchable = ( ! Item.fWatched || ! g_bHack[ Item.slotid ] ) && g_nPathInfo < MAX_WATCH;
	BOOL bWatchable = ( ! Item.fWatched
						|| Item.slotid == MAX_SLOTS
						|| Item.slotid < MAX_SLOTS && ! g_bHack[ Item.slotid ] ) && g_nPathInfo < MAX_WATCH;



	//BOOL bUnwatchable = FALSE;
	BOOL bSlotAllocated = FALSE;

	BOOL bFriend = TRUE;
	BOOL bResetIFF = FALSE;
	BOOL bFoe = TRUE;

	BOOL bUnfreeze = TRUE;
	BOOL bShow = TRUE;
	BOOL bShowM = TRUE;
	BOOL bHide = TRUE;

	const UINT g = Item.slotid;


	TCHAR szSliderBtnText[ 16 ] = _T("Unl&imit");
	if( g < MAX_SLOTS && g != 3 )
	{
		bSlotAllocated = TRUE;
		bUnlimitable = g_bHack[ g ];
		bLimitable = ! bUnlimitable;
		if( bLimitable )
			_tcscpy_s( szSliderBtnText, _countof(szSliderBtnText), _T("Rel&imit") );
	}

	/*TCHAR szWatchBtnText[ 16 ] = _T("Limit/&Watch");
	if( ! bWatchable && g == 2 )
	{
		bUnwatchable = TRUE;
		_tcscpy_s( szWatchBtnText, _countof(szWatchBtnText), _T("Un&watch") );
	}*/

	if( Item.fThisIsBES )
	{
		bLimitable = FALSE;

		bWatchable = FALSE;
		bFriend = FALSE;
		bFoe = FALSE;
		
		if( Item.dwProcessId == GetCurrentProcessId() )
		{
			bUnfreeze = FALSE;
			bShowM = FALSE;
		}
		
		bShow = FALSE;
		bHide = FALSE;
	}
	else if( current_IFF == IFF_FRIEND )
	{
		bFriend = FALSE;
		bResetIFF = TRUE;
	}
	else if( current_IFF == IFF_FOE )
	{
		bFoe = FALSE;
		bResetIFF = TRUE;
	}
	else if( current_IFF == IFF_ABS_FOE	|| current_IFF == IFF_SYSTEM )
	{
		bFriend = FALSE;
		bFoe = FALSE;
	}

	// To be really limitable, there must be at least one free slot
	if( bLimitable )
	{
		bLimitable = HasFreeSlot();
	}

	//BOOL bUnlocked = ( current_IFF != IFF_SYSTEM &&
	//					Button_GetCheck( GetDlgItem( hDlg, IDC_SAFETY ) ) == BST_UNCHECKED );

	// do this in reverse tab order

	EnableDlgItem( hDlg, IDC_UNFREEZE, bUnfreeze );
	EnableDlgItem( hDlg, IDC_SHOW_MANUALLY, bShowM );
	EnableDlgItem( hDlg, IDC_SHOW, bShow );
	EnableDlgItem( hDlg, IDC_HIDE, bHide );

	EnableDlgItem( hDlg, IDC_SLIDER_Q, bSlotAllocated );
	EnableDlgItem( hDlg, IDC_TEXT_PERCENT, bSlotAllocated );
	if( ! bSlotAllocated ) SetDlgItemText( hDlg, IDC_TEXT_PERCENT, TEXT("") );
	EnableDlgItem( hDlg, IDC_SLIDER, bSlotAllocated );
	EnableDlgItem( hDlg, IDC_SLIDER_BUTTON, bSlotAllocated );
	SetDlgItemText( hDlg, IDC_SLIDER_BUTTON, szSliderBtnText );

	EnableDlgItem( hDlg, IDC_FOE, bFoe );
	EnableDlgItem( hDlg, IDC_RESET_IFF, bResetIFF );
	EnableDlgItem( hDlg, IDC_FRIEND, bFriend );

	ptrdiff_t e = 0;
	for( ; e < MAX_SLOTS; ++e ){ if( e != 3 && g_bHack[ e ] ) break; }
	EnableDlgItem( hDlg, IDC_UNLIMIT_ALL, e < MAX_SLOTS );
	
/* 20151204 @1.7.0.27
	EnableDlgItem( hDlg, IDC_WATCH, 
					bWatchable && (bLimitable || bUnlimitable)
					|| bUnwatchable );
	SetDlgItemText( hDlg, IDC_WATCH, szWatchBtnText );
*/
	
	EnableDlgItem( hDlg, IDOK, bLimitable );
//	SetDlgItemText( hDlg, IDOK, szOkBtnText );


	EnableDlgItem( hDlg, IDC_WATCH, bWatchable );

	/*
	(@) Unknown [ Friend ] [ Unknown ] [ Foe ]
	^^^         ^        ^
	ThisArea  rect.left  rect.right
	*/
	RECT rect = {0};
	GetWindowRect( GetDlgItem( hDlg, IDC_FRIEND ), &rect );
	MapWindowRect( NULL, hDlg, &rect );
	rect.right = rect.left - 1;
	rect.left = 10;
	InvalidateRect( hDlg, &rect, TRUE );

	SetProcessInfoMsg( hDlg, Item, (int) g );

//	if( g < MAX_SLOTS && g != 3 )
	if( g != 3 ) // 20170906
	{
		if( GetFocus() != GetDlgItem( hDlg, IDC_SLIDER ) )
		{
			// const int iSlider = g_Slider[ g ];
			const int iSlider = GetSliderIni( Item.szPath, g_hWnd ); // 20170906
			TCHAR strPercent[ 16 ] = _T("");
			GetPercentageString( iSlider, strPercent, _countof(strPercent) );
			SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) iSlider );
			SetDlgItemText( hDlg, IDC_TEXT_PERCENT, strPercent );
		}
	}
}

static void SetProcessInfoMsg( HWND hDlg, const PROCESSINFO& ti, int slotid )
{
#if 1
	TCHAR hdr[ 128 ];
	if( slotid == MAX_SLOTS )
	{
		_sntprintf_s( hdr, _countof(hdr), _TRUNCATE,
								_T("PID %lu: %s"),
								ti.dwProcessId, ti.pExe );
	}
	else
	{
		_sntprintf_s( hdr, _countof(hdr), _TRUNCATE,
								_T("PID %lu: %s (Target #%d)"),
								ti.dwProcessId, ti.pExe,
								slotid < 3 ? slotid + 1 : slotid );
	}
	
	TCHAR current_text[ CCHPATH ] = _T("");
	GetDlgItemText( hDlg, IDC_GROUPBOX, current_text, (int) _countof(current_text) );
	if( _tcscmp( current_text, hdr ) != 0 )
		SetDlgItemText( hDlg, IDC_GROUPBOX, hdr );
	
	GetDlgItemText( hDlg, IDC_EDIT_INFO, current_text, (int) _countof(current_text) );
	if( _tcscmp( current_text, ti.szPath ) != 0 )
		SetDlgItemText( hDlg, IDC_EDIT_INFO, ti.szPath );

#else
	ptrdiff_t numOfThreads = 0;
	DWORD * pThreadIDs = ListProcessThreads_Alloc( lpTarget->dwProcessId, numOfThreads );
	if( ! pThreadIDs )
	{
		SetDlgItemText( hDlg, IDC_EDIT_INFO, _T("") );
		return FALSE;
	}

	const size_t cchmsg = CCHPATH * 2 + MAX_WINDOWTEXT + 512 + 32 * (size_t) numOfThreads;
	TCHAR * msg = (TCHAR*) HeapAlloc( GetProcessHeap(), 0L, sizeof(TCHAR) * cchmsg );
	if( ! msg )
	{
		HeapFree( GetProcessHeap(), 0L, pThreadIDs );
		SetDlgItemText( hDlg, IDC_EDIT_INFO, _T("") );
		return FALSE;
	}
	*msg = _T('\0');

	TCHAR strFormat[ 1024 ] = TEXT( S_FORMAT1_ASC );
#ifdef _UNICODE
	if( IS_JAPANESE )
	{
		MultiByteToWideChar( CP_UTF8, MB_CUTE, S_FORMAT1_JPN, -1, strFormat, 1023 );
	}
#endif

	_stprintf_s( msg, cchmsg, strFormat, // ~2400 (plus 32*numOfThreads)
					lpTarget->szExe, // 520
					lpTarget->szPath, // 520
					lpTarget->szText ? lpTarget->szText : _T("<no text>"), // 1024
					lpTarget->dwProcessId, // 8
					lpTarget->dwProcessId, // 10
					(unsigned) numOfThreads // 10
				);

#ifdef _UNICODE
	if( ! IS_JAPANESE )
	{
		// "including %u thread(s)" <-- this (s)
		if( numOfThreads != 1 ) _tcscat_s( msg, cchmsg, _T( "s" ) );
	}
#endif
/*
	_tcscat_s( msg, cchmsg, _T( ":\r\n" ) );

	for( ptrdiff_t i = 0; i < numOfThreads; ++i )
	{
		TCHAR line[ 100 ];
		_stprintf_s( line, _countof(line),
						_T( "Thread #%03u: ID 0x%04X %04X\r\n" ),
						(int) i + 1,
						HIWORD( pThreadIDs[ i ] ),
						LOWORD( pThreadIDs[ i ] )
		);

		_tcscat_s( msg, cchmsg, line );
	}

	_tcscat_s( msg, cchmsg, _T( "\r\n" ) );
*/
	SetDlgItemText( hDlg, IDC_EDIT_INFO, msg );

	HeapFree( GetProcessHeap(), 0L, msg );
	HeapFree( GetProcessHeap(), 0L, pThreadIDs );
	return TRUE;
#endif
}

#ifdef STRICT
#define WNDPROC_FARPROC WNDPROC
#else
#define WNDPROC_FARPROC FARPROC
#endif
static WNDPROC_FARPROC  DefTBProc = NULL;
LRESULT TB_OnMouseWheel( HWND hTrack, WPARAM wParam );
static WNDPROC_FARPROC  DefLVProc = NULL;

static LRESULT CALLBACK SubLVProc( HWND hLV, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	// I don't know why this is happening, but WM_CONTEXTMENU is issued both on
	// WM_RBUTTONDOWN and WM_RBUTTONUP.  Context menu is supposed to be shown on R-UP,
	// not R-DOWN.  So we'll eat WM_CONTEXTMENU on R-UP.
	if( uMessage == WM_CONTEXTMENU )
	{
		if( IsRButtonDown() )
			return TRUE;
	}

	return CallWindowProc( DefLVProc, hLV, uMessage, wParam, lParam );
}


bool TB_OnLButtonDown( HWND hTB )
{
	POINT ptClicked = {0};
	RECT rcThumb = {0};
	GetCursorPos( &ptClicked );
	ScreenToClient( hTB, &ptClicked );
	SendMessage( hTB, TBM_GETTHUMBRECT, 0, (LPARAM) &rcThumb );
	
	if( PtInRect( &rcThumb, ptClicked ) ) return false;

	LONG lNewPos = -1L;
	RECT rcChannel = {0};
	SendMessage( hTB, TBM_GETCHANNELRECT, 0, (LPARAM) &rcChannel );

	const LONG nTicks = (LONG) SendMessage( hTB, TBM_GETNUMTICS, 0, 0 );
	for( LONG n = 0L; n < nTicks - 2; ++n )
	{
		const LONG x = (LONG) SendMessage( hTB, TBM_GETTICPOS, (WPARAM) n, 0 );
		if( x != -1L && labs( x - ptClicked.x ) < 3L ) // Clicked on/near a tick
		{
			lNewPos = (LONG) SendMessage( hTB, TBM_GETTIC, (WPARAM) n, 0 );
			break;
		}
	}

	if( lNewPos == -1L )
	{
		LONG lTotalWidth = rcChannel.right - rcChannel.left;
		LONG lMaxPos = (LONG) SendMessage( hTB, TBM_GETRANGEMAX, 0, 0 );
		lNewPos = (LONG)( lMaxPos * ( ptClicked.x - rcChannel.left ) / (double) lTotalWidth + 0.5 );
	}

	SendMessage( hTB, TBM_SETPOS, TRUE, lNewPos );
	SendMessage( GetParent( hTB ), WM_HSCROLL, MAKEWPARAM( TB_THUMBPOSITION, lNewPos ), (LPARAM) hTB );
	SendMessage( GetParent( hTB ), WM_HSCROLL, TB_ENDTRACK, (LPARAM) hTB );
	
	return true;
}

static LRESULT CALLBACK SubTBProc( HWND hTB, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	if( uMessage == WM_KILLFOCUS )
	{
		TCHAR szTarget[ CCHPATH ];
		if( GetDlgItemText( GetParent( hTB ), IDC_EDIT_INFO, szTarget, CCHPATH ) )
		{
			LRESULT slider = SendMessage( hTB, TBM_GETPOS, 0, 0 );
			SetSliderIni( szTarget, slider );
		}
	}
	else if( uMessage == WM_MOUSEWHEEL )
	{
		return TB_OnMouseWheel( hTB, wParam );
	}
	else if( uMessage == WM_LBUTTONDOWN )
	{
		if( TB_OnLButtonDown( hTB ) )
			return TRUE;
	}

	return CallWindowProc( DefTBProc, hTB, uMessage, wParam, lParam );
}



static void _add_item( HWND hwndList, int iff, PROCESSINFO& ti )
{
	LVITEM item = {0};
	item.mask = LVIF_PARAM;
	item.iItem = INT_MAX;
	item.iSubItem = 0;     // must be zero
	item.lParam = (LONG) ti.dwProcessId;
	const int itemIdx = ListView_InsertItem( hwndList, &item );

	if( ti.slotid < MAX_SLOTS ) // ti.slotid==MAX_SLOTS if no slot is used for this item
	{
		TCHAR szSlotText[ 8 ];
		_stprintf_s( szSlotText, _countof(szSlotText), _T("#%d%c"), 
										(int)( ti.slotid < 3 ? ti.slotid + 1 : ti.slotid ),
										( ti.slotid == 2 && g_bHack[ 3 ] ) ? _T('W') : _T('\0') );
		ListView_SetItemText( hwndList, itemIdx, SLOT, szSlotText );
	}

#define M (48)
	TCHAR szShortText[ M + 1 ] = _T(""); // up to M cch, not incl. the term NUL

	_stprintf_s( szShortText, _countof(szShortText), _T("%lu"), ti.dwProcessId );
/*
	_stprintf_s( szShortText, _countof(szShortText), _T("%5lu"), ti.dwProcessId );
#ifdef _UNICODE
	TCHAR * p = &szShortText[ 0 ];
	while( *p == _T(' ') ) // make leading spaces (if any) prettier
	{
		*p++ = _T('\x2007'); // normal SPACE to Unicode Figure-space
	}
#endif
*/
	ListView_SetItemText( hwndList, itemIdx, PID, szShortText );

	if( 0 < iff )
	{
		ListView_SetItemText( hwndList, itemIdx, IFF, iff == IFF_ABS_FOE ? _T("++") : _T("+") );
	}
	else if( iff < 0 )
	{
#ifdef _UNICODE
		ListView_SetItemText( hwndList, itemIdx, IFF, iff == IFF_SYSTEM ? _T("\x2212\x2212") : _T("\x2212") );
#else
		ListView_SetItemText( hwndList, itemIdx, IFF, iff == IFF_SYSTEM ? _T("--") : _T("-") );
#endif
	}

	_stprintf_s( szShortText, _countof(szShortText), _T("%02d"), (int) ti._cpu );
	ListView_SetItemText( hwndList, itemIdx, CPU, szShortText );

	if( STRUNCATE == _tcsncpy_s( szShortText, _countof(szShortText), 
						ti.szText ? ti.szText : _T(""), _TRUNCATE ) )
	{
#ifdef _UNICODE
		szShortText[ M - 1 ] = _T('\x2026');
#else
		szShortText[ M - 3 ] = _T('.');
		szShortText[ M - 2 ] = _T('.');
		szShortText[ M - 1 ] = _T('.');
#endif
		szShortText[ M ] = _T('\0'); // already NUL-terminated but just in case
	}
#undef M

	ListView_SetItemText( hwndList, itemIdx, EXE, (LPTSTR)( ti.pExe ? ti.pExe : _T("") ) );
	ListView_SetItemText( hwndList, itemIdx, TITLE, szShortText );
	ListView_SetItemText( hwndList, itemIdx, PATH, ti.szPath ? ti.szPath : _T("") );
	_stprintf_s( szShortText, _countof(szShortText), _T("%lu"), ti.numOfThreads );
	ListView_SetItemText( hwndList, itemIdx, THREADS, szShortText );
}
#ifdef PTRSORT
# define PICAST(ptr) (**static_cast<const PROCESSINFO* const *>(ptr))
#else
# define PICAST(ptr) (*static_cast<const PROCESSINFO*>(ptr))
#endif
static int picmp_SLOT( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	if( target1.slotid == target2.slotid )
		return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;

	return (int) target1.slotid - (int) target2.slotid;
}
static int picmp_SLOT2( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );
	int id1 = (target1.slotid >= MAX_SLOTS)? -1 : (int) target1.slotid;
	int id2 = (target2.slotid >= MAX_SLOTS)? -1 : (int) target2.slotid;

	if( id1 == id2 )
		return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;

	return id2 - id1;
}
static int picmp_IFF( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = target2.iIFF - target1.iIFF;
	if( ! result )
	{
		result = _tcsicmp( target1.pExe ? target1.pExe : _T(""),
							target2.pExe ? target2.pExe : _T("") );
		
		if( ! result )
			result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	}
	return result;
}
static int picmp_IFF2( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = target1.iIFF - target2.iIFF;
	if( ! result )
	{
		result = _tcsicmp( target1.pExe ? target1.pExe : _T(""),
							target2.pExe ? target2.pExe : _T("") );
		
		if( ! result )
			result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	}
	return result;
}

static int picmp_CPU( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	if( target1._cpu == target2._cpu )
		return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;

	return target2._cpu - target1._cpu;
}
static int picmp_CPU2( const void * pv1, const void * pv2 )
{
	return picmp_CPU( pv2, pv1 );
}

static int picmp_PID( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );
	return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
}
static int picmp_PID2( const void * pv1, const void * pv2 )
{
	return picmp_PID( pv2, pv1 );
}

static int picmp_EXE( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = _tcsicmp( target1.pExe ? target1.pExe : _T(""),
							target2.pExe ? target2.pExe : _T("") );
	if( ! result )
		result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	
	return result;
}
static int picmp_EXE2( const void * pv1, const void * pv2 )
{
	return picmp_EXE( pv2, pv1 );
}

static int picmp_PATH( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = _tcsicmp( target1.szPath, target2.szPath );
	if( ! result )
		result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	
	return result;
}
static int picmp_PATH2( const void * pv1, const void * pv2 )
{
	return picmp_PATH( pv2, pv1 );
}
static int picmp_THREADS( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );
	
	if( target1.numOfThreads > target2.numOfThreads ) return +1;
	if( target1.numOfThreads < target2.numOfThreads ) return -1;
	return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
}
static int picmp_THREADS2( const void * pv1, const void * pv2 )
{
	return picmp_THREADS( pv2, pv1 );
}

static int picmp_TITLE( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = 0;
	if( ! target1.szText )
	{
		if( target2.szText ) result = +1;
		else result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	}
	else if( ! target2.szText )
	{
		result = -1;
	}
	else
	{
		result = _tcsicmp( target1.szText, target2.szText );

		if( ! result )
			result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	}
	
	return result;
}
static int picmp_TITLE2( const void * pv1, const void * pv2 )
{
	const PROCESSINFO& target1 = PICAST( pv1 );
	const PROCESSINFO& target2 = PICAST( pv2 );

	int result = 0;
	if( ! target1.szText )
	{
		if( target2.szText ) result = +1;
		else result = ( target1.dwProcessId > target2.dwProcessId ) ? -1 : +1 ;
	}
	else if( ! target2.szText )
	{
		result = -1;
	}
	else
	{
		result = _tcsicmp( target2.szText, target1.szText );
		if( ! result )
			result = ( target1.dwProcessId > target2.dwProcessId ) ? -1 : +1 ;
	}
	
	return result;
}

static int pair_comp( const void * pv1, const void * pv2 )
{
	const PROCESS_THREAD_PAIR& pair1 = *static_cast<const PROCESS_THREAD_PAIR *>( pv1 );
	const PROCESS_THREAD_PAIR& pair2 = *static_cast<const PROCESS_THREAD_PAIR *>( pv2 );

	if( pair1.pid < pair2.pid ) return -1;
	if( pair1.pid > pair2.pid ) return +1;
	// NOTE: if you do bsearch, return 0 when equal
	return ( pair1.tid < pair2.tid ) ? (-1) : (+1) ;
}

typedef struct tagFindImportantText {
	TCHAR * pszText; // must be able to store MAX_WINDOWTEXT cch
	DWORD_PTR dwImportance;
} IMPORTANT_TEXT;

static CLEAN_POINTER TCHAR * GetProcessWindowText(
	const PROCESS_THREAD_PAIR * pairs,
	ptrdiff_t numOfThreads )
{
	TCHAR * lpWindowText = NULL;

	TCHAR szBuffer[ MAX_WINDOWTEXT ];

	szBuffer[ 0 ] = _T('\0');
	IMPORTANT_TEXT it;
	it.pszText = szBuffer;
	it.dwImportance = 0;

	for( ptrdiff_t threadIndex = 0; threadIndex < numOfThreads; ++threadIndex )
		EnumThreadWindows( pairs[ threadIndex ].tid, &GetImportantText_EnumProc, (LPARAM) &it );

	if( szBuffer[ 0 ] )
	{
		size_t cchMem = _tcslen( szBuffer ) + 1;
		lpWindowText = (TCHAR *) HeapAlloc( GetProcessHeap(), 0, cchMem * sizeof(TCHAR) );
		if( lpWindowText )
			_tcscpy_s( lpWindowText, cchMem, szBuffer );
	}

	return lpWindowText;
}

#define LIST_SYSTEM_PROCESS (2)
#define ListMore(listLevel) (LIST_SYSTEM_PROCESS<=(listLevel))
#define ListLess(listLevel) ((listLevel)<LIST_SYSTEM_PROCESS)
typedef BOOL (WINAPI *GetProcessTimes_t)(HANDLE,LPFILETIME,LPFILETIME,LPFILETIME,LPFILETIME);
extern GetProcessTimes_t g_pfnGetProcessTimes;
// Just using sorted_pairs is faster than using CreateToolhelp32Snapshot or EnumProcesses,
// mainly because that way we have a sorted list of every processID-threadID pair.
// Note that, if we use CreateToolhelp32Snapshot again, the # of threads may be different
// than in sorted_pairs, because it's a different snap taken few ms later.

static ptrdiff_t UpdateProcessSnap(
	PROCESSINFO *& rgProcessInfo,
	bool fListAll,
	ptrdiff_t * pNumOfPairs,
	ptrdiff_t cPrevItems
)
{
	static CPUTIME * s_lpCPUTime = NULL;
	static ptrdiff_t s_cntCPUTime = 0;
	static ULONGLONG s_tPrev = 0;

	if( ! rgProcessInfo || ! pNumOfPairs )
	{
		if( s_lpCPUTime )
		{
			MemFree( s_lpCPUTime );
			s_lpCPUTime = NULL;
			s_cntCPUTime = 0;
			s_tPrev = 0;
		}
		return 0;
	}

	SIZE_T K = HeapSize( GetProcessHeap(), 0, rgProcessInfo ) / sizeof(PROCESSINFO);
	if( K < (SIZE_T) cPrevItems )
	{
		TCHAR s[100];_stprintf_s(s,100,_T("DEBUG ME\n\n\tK=%d prev=%d"),(int)K,(int)cPrevItems);
		MessageBox(NULL,s,APP_NAME,MB_ICONSTOP|MB_TOPMOST);

		cPrevItems = (ptrdiff_t) K;
	}
	for( ptrdiff_t i = 0; i < cPrevItems; ++i )
	{
		if( rgProcessInfo[ i ].szPath )
		{
			HeapFree( GetProcessHeap(), 0, rgProcessInfo[ i ].szPath );
			rgProcessInfo[ i ].szPath = NULL;
		}
		
		if( rgProcessInfo[ i ].szText )
		{
			HeapFree( GetProcessHeap(), 0, rgProcessInfo[ i ].szText );
			rgProcessInfo[ i ].szText = NULL;
		}
	}

	*pNumOfPairs = 0;

	ptrdiff_t numOfPairs = 0;

	PROCESS_THREAD_PAIR * sorted_pairs = NULL;
	bool fCachedOperation = false;
	if( WaitForSingleObject( hReconSema, 100 ) == WAIT_OBJECT_0 )
	{
		numOfPairs = s_nSortedPairs;
		sorted_pairs = (PROCESS_THREAD_PAIR*) HeapAlloc( GetProcessHeap(), 0, 
														numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
		
		if( sorted_pairs && s_lpSortedPairs && numOfPairs )
		{
			memcpy( sorted_pairs, s_lpSortedPairs, numOfPairs * sizeof(PROCESS_THREAD_PAIR) );
			fCachedOperation = true;
		}
		ReleaseSemaphore( hReconSema, 1L, NULL );
	}

	if( ! fCachedOperation )
	{
#ifdef _DEBUG
		OutputDebugString(_T("\t###NOT###\n"));
#endif
		if( sorted_pairs ) MemFree( sorted_pairs );
		sorted_pairs = AllocSortedPairs( numOfPairs, 0 );
	}
	
	if( ! sorted_pairs )
	{
		return 0;
	}

	// Count the number of Processes
	SIZE_T numOfPIDs = 0;
	for( ptrdiff_t px = 0; px < numOfPairs; ++px )
		if( px == 0 || sorted_pairs[ px ].pid != sorted_pairs[ px - 1 ].pid ) ++numOfPIDs;

	MemFree( rgProcessInfo );
	rgProcessInfo = (PROCESSINFO *) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
													sizeof(PROCESSINFO) * numOfPIDs );

	if( rgProcessInfo == NULL )
	{
		MemFree( sorted_pairs );
		sorted_pairs = NULL;
		return 0;
	}
	
	CPUTIME * lpCPUTime = (CPUTIME *) MemAlloc( sizeof(CPUTIME), numOfPIDs );
	if( ! lpCPUTime )
	{
		MemFree( sorted_pairs );
		sorted_pairs = NULL;
		return 0;
	}

	const DWORD dwCurrentPID = GetCurrentProcessId();

	FILETIME ft = {0};
	GetSystemTimeAsFileTime( &ft );
	const ULONGLONG tNow = ((ULONGLONG) ft.dwHighDateTime) << 32 | ft.dwLowDateTime;

#if defined(_MSC_VER) && _MSC_VER < 1400
	const double CPUTimeDenom = (double)(LONGLONG)( tNow - s_tPrev ) / 100.0;
#else
	const double CPUTimeDenom = (double)(ULONGLONG)( tNow - s_tPrev ) / 100.0;
#endif
	
	const bool fCheckCPUTime = ( s_tPrev != 0 ) && ( s_tPrev < tNow ) && ( s_lpCPUTime != NULL );

	ptrdiff_t pix = 0; // rgProcessInfo array index
	ptrdiff_t cx = 0;    // CPUTime array (s_lpCPUTime) index

#ifdef _DEBUG
	SIZE_T skipped = 0;
#endif

	for( ptrdiff_t s = 0, cntThreads; s < numOfPairs; s += cntThreads )
	{
		const DWORD dwProcessID = sorted_pairs[ s ].pid;
		cntThreads = 1;
		while( s + cntThreads < numOfPairs && sorted_pairs[ s + cntThreads ].pid == dwProcessID )
			++cntThreads;

		if( dwProcessID == 0 ||
			dwProcessID == dwCurrentPID && ! fListAll ) {
#ifdef _DEBUG
			++skipped;
#endif
				continue;
		}

		HANDLE hProcess = OpenProcess(
			//PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_VM_READ,
			PROCESS_QUERY_INFORMATION,
			FALSE, dwProcessID );

		if( hProcess == NULL ) {
#ifdef _DEBUG
			++skipped;
#endif
			continue;
		}



		FILETIME ftCreated, ftExited, ftKernel, ftUser;
		if( g_pfnGetProcessTimes
			&& (*g_pfnGetProcessTimes)( hProcess, &ftCreated, &ftExited, &ftKernel, &ftUser ) )
		{
			ULONGLONG t	= ((ULONGLONG) ftKernel.dwHighDateTime << 32 | ftKernel.dwLowDateTime )
							+ ((ULONGLONG) ftUser.dwHighDateTime << 32 | ftUser.dwLowDateTime );

			if( fCheckCPUTime )
			{
				while( cx < s_cntCPUTime && s_lpCPUTime[ cx ].pid < dwProcessID )
					++cx;

				if( cx < s_cntCPUTime && s_lpCPUTime[ cx ].pid == dwProcessID )
				{
					ULONGLONG CPUTimeDiff =	t - s_lpCPUTime[ cx ].t;
#if defined(_MSC_VER) && _MSC_VER < 1400
					double d = (double) (LONGLONG) CPUTimeDiff / CPUTimeDenom;
#else
					double d = (double) CPUTimeDiff / CPUTimeDenom;
#endif

					if( 0 <= d && d <= 100.0 )
						rgProcessInfo[ pix ]._cpu = (short)( d + 0.5 );
					else
						rgProcessInfo[ pix ]._cpu = (short) 100;
				}
			}

			lpCPUTime[ pix ].pid = dwProcessID;
			lpCPUTime[ pix ].t = t;
		}

		rgProcessInfo[ pix ].iIFF = IFF_UNKNOWN;

#ifdef _DEBUG
		if(rgProcessInfo[ pix ].szPath)
		{
			OutputDebugString(rgProcessInfo[ pix ].szPath);
			OutputDebugString(_T("###WHY###\n"));
		}
#endif

		rgProcessInfo[ pix ].szPath = ProcessToPath_Alloc( hProcess, NULL );


#if 0
		DWORD_PTR dw1 = 0, dw2 = 0;
		SetLastError(0);
		BOOL b = GetProcessAffinityMask( hProcess, &dw1, &dw2 );
		TCHAR ss[1000];
		swprintf_s(ss,1000,L"dw1=%u dw2=%u b=%d e=%u %s\n",dw1,dw2,b,GetLastError(),rgProcessInfo[pix].szPath);
		OutputDebugString(ss);
#endif

		const TCHAR * pExe;
		if( rgProcessInfo[ pix ].szPath )
		{
			pExe = _tcsrchr( rgProcessInfo[ pix ].szPath, _T('\\') );
			if( pExe && pExe[ 1 ] ) ++pExe;
			else pExe = rgProcessInfo[ pix ].szPath;
			rgProcessInfo[ pix ].pExe = pExe;

			if( dwProcessID == dwCurrentPID
				|| ! _tcsicmp( pExe, _T("BES.exe") )
					&& IsProcessBES( dwProcessID, sorted_pairs + s, cntThreads ) )
			{
				if( ! fListAll )
				{
					// Free it; otherwise it leaks next time in this loop 20140405
					TcharFree( rgProcessInfo[ pix ].szPath );
					rgProcessInfo[ pix ].szPath = NULL;
					continue;
				}
					
				rgProcessInfo[ pix ].fThisIsBES = true;
				rgProcessInfo[ pix ].iIFF = IFF_SYSTEM;
			}
			// These are absolutely foes (IFF_ABS_FOE)
			// We should make sure they are in the Enemy list.
			else if( rgProcessInfo[ pix ].iIFF == IFF_UNKNOWN )
			{
				if( IsAbsFoe2( pExe, rgProcessInfo[ pix ].szPath ) ) //20111230
				{
					rgProcessInfo[ pix ].iIFF = IFF_ABS_FOE;
				}
			}
		}
		else //if( dwErr == ERROR_ACCESS_DENIED )
		{
			// rgProcessInfo[ pix ].szPath is NULL here

			if( ! fListAll )
				continue;

			// Since the old value is NUL, this doesn't leak
			rgProcessInfo[ pix ].szPath	= TcharAlloc( 7 );
			_tcscpy_s( rgProcessInfo[ pix ].szPath, 7, _T("System") );
			rgProcessInfo[ pix ].pExe = rgProcessInfo[ pix ].szPath;
			pExe = rgProcessInfo[ pix ].szPath;
				
			rgProcessInfo[ pix ].iIFF = IFF_SYSTEM;
		}

#ifdef _DEBUG
		if(rgProcessInfo[ pix ].szText)OutputDebugString(_T("###WHY###\n"));
#endif
		rgProcessInfo[ pix ].szText = GetProcessWindowText( sorted_pairs + s, cntThreads );
		rgProcessInfo[ pix ].dwProcessId = dwProcessID;
		rgProcessInfo[ pix ].numOfThreads = (DWORD) cntThreads;

		if( pExe )
		{
			// Detect user-defined foes
			if( rgProcessInfo[ pix ].iIFF == IFF_UNKNOWN )
			{
				for( ptrdiff_t i = 0; i < g_numOfEnemies; ++i )
				{
					if( _tcsicmp( pExe, g_lpszEnemy[ i ] ) == 0 )
					{
						rgProcessInfo[ pix ].iIFF = IFF_FOE;
						break;
					}
				}
			}

			// detect friends
			if( rgProcessInfo[ pix ].iIFF == IFF_UNKNOWN )
			{
				for( ptrdiff_t i = 0; i < g_numOfFriends; ++i )
				{
					if( _tcsicmp( pExe, g_lpszFriend[ i ] ) == 0 )
					{
						rgProcessInfo[ pix ].iIFF = IFF_FRIEND;
						break;
					}
				}
			}
		}

		++pix;

#ifdef _DEBUG
		if( numOfPIDs < (SIZE_T) pix )
			MessageBox( NULL, _T("BAD"), NULL, MB_OK|MB_ICONSTOP|MB_TOPMOST);
#endif

	}
#ifdef _DEBUG
	if( (SIZE_T) pix != numOfPIDs - skipped )
	{
		MessageBox( NULL, _T("BAD pix"), NULL, MB_OK|MB_ICONSTOP|MB_TOPMOST);
		/*SIZE_T n = 0;
		DWORD prev = (DWORD)-1;
		for( ptrdiff_t j = 0; j < numOfPairs; ++j ) {
			if( sorted_pairs[j].pid != prev ) {
				++n;
				prev = sorted_pairs[j].pid;
			}
			WCHAR aa[ 1000 ];
			swprintf_s( aa, 1000, L"[%u/%u] %d/%d:%08x:%08x {pix=%d; skipped=%u}\n",
				n, numOfPIDs, j, numOfPairs, sorted_pairs[j].pid, sorted_pairs[j].tid, pix, skipped );
			OutputDebugString(aa);
		}*/
	}
#endif

	MemFree( sorted_pairs );
	sorted_pairs = NULL;

	if( s_lpCPUTime ) MemFree( s_lpCPUTime );

	s_lpCPUTime = lpCPUTime;
	s_cntCPUTime = pix;
	s_tPrev = tNow;
	
	*pNumOfPairs = numOfPairs; // debug info
	return pix;
}

static INT_PTR LV_OnCustomDraw(
	HWND hDlg,
	HWND hLV,
	LPARAM lParam,
	const PROCESSINFO * arPi,
	DWORD cItems,
	DWORD dwCurrentSel )
{
	NMLVCUSTOMDRAW& cd = *(NMLVCUSTOMDRAW *) lParam;
	if( cd.nmcd.dwDrawStage == CDDS_PREPAINT
		|| cd.nmcd.dwDrawStage == CDDS_ITEMPREPAINT )
	{
		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW );
	}
	else if( cd.nmcd.dwDrawStage == ( CDDS_ITEMPREPAINT | CDDS_SUBITEM ) )
	{
		DWORD itemIndex = cd.nmcd.dwItemSpec;

		// cd.nmcd.uItemState (CDIS_FOCUS bit) is usable only when the LV has focus;
		// otherwise it's unclear from this flag which row is currently selected
		//const bool fHighlight = !!( cd.nmcd.uItemState & CDIS_FOCUS );
		bool fHighlight = ( itemIndex == dwCurrentSel );

#if 0
//		cd.clrText = fHighlight ? RGB( 0xFF, 0xFF, 0xFF ) : TEXT_COLOR ;
//		cd.clrTextBk = fHighlight ? RGB( 0x31, 0x6A, 0xC5 ) : BG_COLOR ; 
		cd.clrText = fHighlight ? RGB( 0xFF, 0xFF, 0xFF ) : TEXT_COLOR ;
		cd.clrTextBk = fHighlight ? RGB( 0x00, 0x00, 0x00 ) : BG_COLOR ; 
#else	
		// Todo: Shold we cache these system colors?  Tests show this is not significantly
		// slower than using constant values, like RGB( 0xFF, 0xFF, 0xFF ).
		cd.clrText = fHighlight ? GetSysColor( COLOR_HIGHLIGHTTEXT ) : RGB( 0x00, 0x00, 0x00 ) ;
		cd.clrTextBk = fHighlight ? GetSysColor( COLOR_HIGHLIGHT ) : BG_COLOR ;
#endif

		if( cd.iSubItem == EXE )
		{
			// The item is currently being limited
			UINT slotid = (UINT)( arPi && itemIndex < cItems ? arPi[ itemIndex ].slotid : MAX_SLOTS );
			if( slotid < MAX_SLOTS && g_bHack[ slotid ] )
			{
				cd.clrText = TEXT_COLOR;
				cd.clrTextBk = RGB( 0xff, 0x00, 0x00 );
			}
		}
		
		if( ! fHighlight )
		{
			if( cd.iSubItem == IFF )
			{
				cd.clrText = RGB( 0x00, 0x00, 0xFF );
				TCHAR szIFF[ 4 ];
				LVITEM li;
				li.iSubItem = IFF;
				li.pszText = szIFF;
				li.cchTextMax = (int) _countof(szIFF);
				if( SendMessage( hLV, LVM_GETITEMTEXT, itemIndex, (LPARAM) &li ) )
				{
					if( *szIFF == _T('+') ) cd.clrTextBk = FOE_COLOR;
#ifdef _UNICODE
					else if( *szIFF == _T('\x2212') ) cd.clrTextBk = FRIEND_COLOR;
#else
					else if( *szIFF == _T('-') ) cd.clrTextBk = FRIEND_COLOR;
#endif
				}
			}
			else if( cd.iSubItem == CPU )
			{
				cd.clrText = RGB( 0x80, 0x00, 0x00 );
			}
			else if( cd.iSubItem == SLOT || cd.iSubItem == EXE || cd.iSubItem == TITLE )
			{
				cd.clrText = TEXT_COLOR;
			}
		}

		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, CDRF_NEWFONT );
	}
	else
	{
		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, CDRF_DODEFAULT );
	}

	return TRUE;
}

static void SetDlgItemFocus( HWND hDlg, HWND hwndCtrl )
{
	PostMessage( hDlg, WM_NEXTDLGCTL, (WPARAM) hwndCtrl, MAKELPARAM( TRUE, 0 ) );
}

static inline void Tcscpy_s( TCHAR * pDst, size_t cchDst, const TCHAR * pSrc )
{
	if( pDst != pSrc ) _tcscpy_s( pDst, cchDst, pSrc );
}

static void ActivateSlider( HWND hDlg, int iSlider )
{
	TCHAR szPercent[ 16 ] = _T("");
	GetPercentageString( iSlider, szPercent, _countof(szPercent) );
	
	HWND hTB = GetDlgItem( hDlg, IDC_SLIDER );
	EnableWindow( hTB, TRUE );

	// SetDlgItemFocusNow: This will protext the text in IDC_TEXT_PERCENT in onSelChange,
	// where that text is set empty if GetFocus() != GetDlgItem( hDlg, IDC_SLIDER )
	SendMessage( hDlg, WM_NEXTDLGCTL, (WPARAM) hTB, MAKELPARAM( TRUE, 0 ) );
	SendMessage( hTB, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) iSlider );

	EnableWindow( GetDlgItem( hDlg, IDC_TEXT_PERCENT ), TRUE );
	SetDlgItemText( hDlg, IDC_TEXT_PERCENT, szPercent );
}
/*
typedef struct tagTargetInfo {
	TCHAR szPath[ CCHPATH ];
	//TCHAR szExe[ CCHPATH ];

	const TCHAR * pExe;
	DWORD dwProcessId;

	int slotid;
	bool fWatch;
	bool fThisIsBES;
	bool fRealTime;
	bool fUnlimited;
} TARGETINFO;
*/

void ResetTi( TARGETINFO& ti )
{
	const WORD slotid_saved = ti.slotid;

	if( ti.lpPath ) TcharFree( ti.lpPath );
	if( ti.disp_path ) TcharFree( ti.disp_path );
	//if( ti.lpExe ) TcharFree( ti.lpExe );

	ZeroMemory( &ti, sizeof(TARGETINFO) );

	ti.slotid = slotid_saved;
}

static BOOL TiFromPi( TARGETINFO& ti, const PROCESSINFO& pi )
{
	if( ! pi.szPath ) return FALSE;

	ResetTi( ti );
	
	size_t cchMem_Path = _tcslen( pi.szPath ) + 1;
	ti.lpPath = TcharAlloc( cchMem_Path );
	if( ! ti.lpPath ) return FALSE;
	_tcscpy_s( ti.lpPath, cchMem_Path, pi.szPath );

	ti.dwProcessId = pi.dwProcessId;

	return TRUE;
}

BOOL TiCopyFrom( TARGETINFO& dst, const TARGETINFO& src )
{
	if( &dst == &src ) return TRUE;

	ResetTi( dst );

	size_t cchMem_Path = _tcslen( src.lpPath ) + 1;
	dst.lpPath = TcharAlloc( cchMem_Path );
	if( ! dst.lpPath ) return FALSE;
	_tcscpy_s( dst.lpPath, cchMem_Path, src.lpPath );

	if( src.disp_path )
	{
		size_t cchMem_Disp = _tcslen( src.disp_path ) + 1;
		dst.disp_path = TcharAlloc( cchMem_Disp ); // 2014-04-01Z
		if( ! dst.disp_path ) return FALSE;
		_tcscpy_s( dst.disp_path, cchMem_Disp, src.disp_path );
	}

	//size_t cchMem_Exe = _tcslen( src.lpExe ) + 1;
	//dst.lpExe = TcharAlloc( cchMem_Exe );
	//if( ! dst.lpExe ) return FALSE;
	//_tcscpy_s( dst.lpExe, cchMem_Exe, src.lpExe );

	dst.dwProcessId = src.dwProcessId;
	dst.fWatch = src.fWatch;
	dst.fRealTime = src.fRealTime;
	dst.fUnlimited = src.fUnlimited;

	return TRUE;
}

static bool has_prefix( const TCHAR * szText ) // 2019-08-15
{
	return( szText != NULL
		&& szText[ 0 ] == _T('[')
		&& _T('0') <= szText[ 1 ] && szText[ 1 ] <= _T('9')
		&& _T('0') <= szText[ 2 ] && szText[ 2 ] <= _T('9')
		&& szText[ 3 ] == _T(']')
		&& szText[ 4 ] == _T(' ')
		&& szText[ 5 ] != _T('\0') );
}