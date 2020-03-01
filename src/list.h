#include "BattleEnc.h"
#ifndef INT_MAX
# include <limits.h>
#endif

extern HINSTANCE g_hInst;
extern HWND g_hWnd;
extern HWND g_hListDlg;
extern HFONT g_hFont;
extern volatile DWORD g_dwTargetProcessId[ 4 ];
extern volatile BOOL g_bHack[ 4 ];
extern volatile int g_Slider[ 4 ];
extern HANDLE hSemaphore[ 4 ];
extern HANDLE hChildThread[ 4 ];

BOOL g_bSelNextOnHide = FALSE;

TCHAR g_lpszEnemy[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
int g_numOfEnemies = 0;

TCHAR g_lpszFriend[ MAX_PROCESS_CNT ][ MAX_PATH * 2 ];
int g_numOfFriends = 0;

#ifndef KEY_UP				
#define KEY_UP(vk)   (0 <= (int) GetKeyState(vk))
#endif

typedef struct tagCPUTime {
	ULONGLONG t;
	DWORD pid;
	DWORD dummy;
} CPUTIME;


void WriteIni_Time( const TCHAR * pExe );
BOOL VerifyOSVer( DWORD dwMajor, DWORD dwMinor, int iSP );
TCHAR * GetPercentageString( LRESULT lTrackbarPos, TCHAR * lpString, size_t cchString );
static PROCESS_THREAD_PAIR * AllocSortedPairs( ptrdiff_t& numOfPairs );

static INT_PTR CALLBACK Question( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
static void _nuke( HWND hDlg, const TARGETINFO& TgtInfo );

static void SetProcessInfoMsg( HWND hDlg, const TARGETINFO& ti );

static BOOL ProcessToPath( HANDLE hProcess, TCHAR * szPath, DWORD cchPath );
static bool IsAbsFoe2( LPCTSTR strExe, LPCTSTR strPath );

//#define MAX_WINDOWS_CNT (1024)
#define MAX_WINDOWS_CNT (280)

//#define CCH_ITEM_MAX    (128)
typedef struct tagWinInfo {
	TCHAR szTitle[ MAX_WINDOWS_CNT ][ MAX_WINDOWTEXT ];
	HWND hwnd[ MAX_WINDOWS_CNT ];
	ptrdiff_t numOfItems;
	INT_PTR dummy;
} WININFO;

#define TIMER_ID (0x101u)

// WM_USER_REFRESH
//    wParam    PID to select; 0 to select the first item; -1 to reuse the currently selected PID
//    lParam    following flag(s) if necessary
#define URF_ENSURE_VISIBLE 0x0002 // ListView_EnsureVisible
#define URF_RESET_FOCUS    0x0010 // SetDlgFocus(hLV)

static BOOL CALLBACK GetImportantText_EnumProc( HWND hwnd, LPARAM lParam );

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

#if 0
enum eLVcols {
	PID, IFF, EXE, CPU, TITLE,
	PATH,
	THREADS, // # of threads
	N_COLS
};
#else
enum eLVcols {
	PID, IFF, EXE, CPU, TITLE,
	PATH,
	THREADS, // # of threads
	N_COLS
};
#endif
static void _init_ListView( HWND hDlg )
{
	LPTSTR colHeader[ N_COLS ]
	= { _T("PID "), _T("IFF"), _T("Name"), _T("CPU"), _T("Window Title"),
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
			( col_idx == CPU || col_idx == THREADS ) ? LVCFMT_RIGHT : 
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
	const bool fAllowMoreThan99 = 
				!!( MF_CHECKED & GetMenuState( GetMenu( g_hWnd ), 
					IDM_ALLOWMORETHAN99, MF_BYCOMMAND ) );
	const int iMax = fAllowMoreThan99 ? SLIDER_MAX : 99 ;
	SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETRANGE, TRUE, MAKELONG( SLIDER_MIN, iMax ) );
	SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETPAGESIZE, 0, 5 );
	for( long lPos = 10L; lPos <= 90L; lPos += 10L )
	{
		SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETTIC, 0, lPos );
	}
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
static ptrdiff_t ListView_GetCurrentSel( HWND hListview )
{
// ListView_GetSelectedColumn only on XP
	return SendMessage( hListview, LVM_GETNEXTITEM, (WPARAM) -1, LVNI_ALL | LVNI_FOCUSED );
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


static void LV_OnSelChange(
	HWND hDlg,
	const ptrdiff_t * MetaIndex,
	const TARGETINFO * ti,
	ptrdiff_t cItems,
	int& current_IFF
)
{
	HWND hListview = GetDlgItem( hDlg, IDC_TARGET_LIST );
	const ptrdiff_t selectedItem = ListView_GetCurrentSel( hListview );
	if( selectedItem < 0 || cItems <= selectedItem ) return;

	const ptrdiff_t index = MetaIndex[ selectedItem ];
	current_IFF = ti[ index ].iIFF;

	BOOL bLimitable = TRUE;
	BOOL bUnlimitable = FALSE;
	BOOL bWatchable = !g_bHack[ 3 ];
	BOOL bUnwatchable = FALSE;
	BOOL bSlotAllocated = FALSE;

	BOOL bFriend = TRUE;
	BOOL bResetIFF = FALSE;
	BOOL bFoe = TRUE;

	BOOL bUnfreeze = TRUE;
	BOOL bShow = TRUE;
	BOOL bShowM = TRUE;
	BOOL bHide = TRUE;

	ptrdiff_t g = 0;
	for( ; g < 3; ++g )
	{
		if(	ti[ index ].dwProcessId == g_dwTargetProcessId[ g ] )
			break;
	}

	TCHAR szSliderBtnText[ 32 ] = _T("Unl&imit");
	if( g < MAX_SLOTS )
	{
		bSlotAllocated = TRUE;
		bUnlimitable = g_bHack[ g ];
		bLimitable = ! bUnlimitable;
		if( bLimitable )
			_tcscpy_s( szSliderBtnText, _countof(szSliderBtnText), _T("Rel&imit") );
	}

	TCHAR szWatchBtnText[ 32 ] = _T("Limit/&Watch");
	if( ! bWatchable && g == 2 )
	{
		bUnwatchable = TRUE;
		_tcscpy_s( szWatchBtnText, _countof(szWatchBtnText), _T("Un&watch") );
	}

	if( ti[ index ].fThisIsBES )
	{
#ifdef _DEBUG
		TCHAR s[100];
		_stprintf_s(s,100,_T("index=%d: %s is BES\n"),(int)index,ti[index].szExe);
		OutputDebugString(s);
#endif
		bLimitable = FALSE;

		bWatchable = FALSE;
		bFriend = FALSE;
		bFoe = FALSE;
		
		if( ti[ index ].dwProcessId == GetCurrentProcessId() )
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
		bLimitable = ( !g_bHack[ 0 ] || !g_bHack[ 1 ] || !g_bHack[ 2 ] );
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

	EnableDlgItem( hDlg, IDC_UNLIMIT_ALL, g_bHack[ 0 ] || g_bHack[ 1 ] || g_bHack[ 2 ] );
	EnableDlgItem( hDlg, IDC_WATCH, 
					bWatchable && (bLimitable || bUnlimitable)
					|| bUnwatchable );
	SetDlgItemText( hDlg, IDC_WATCH, szWatchBtnText );
	
	EnableDlgItem( hDlg, IDOK, bLimitable );
//	SetDlgItemText( hDlg, IDOK, szOkBtnText );

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

	SetProcessInfoMsg( hDlg, ti[ index ] );


	if( g < 3 )
	{
		if( GetFocus() != GetDlgItem( hDlg, IDC_SLIDER ) )
		{
			const int iSlider = g_Slider[ g ];
			TCHAR strPercent[ 32 ] = _T("");
			GetPercentageString( iSlider, strPercent, _countof(strPercent) );
			SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETPOS, (WPARAM) TRUE, (LPARAM) iSlider );
			SetDlgItemText( hDlg, IDC_TEXT_PERCENT, strPercent );
		}
	}
}

static void SetProcessInfoMsg( HWND hDlg, const TARGETINFO& ti )
{
#if 1
	TCHAR szHeader[ CCHPATH + 64 ] = _T("");
	_stprintf_s( szHeader, _countof(szHeader), 
				_T("PID %lu: %s (%lu Thread%s)"),
				ti.dwProcessId,
				ti.szExe,
				ti.numOfThreads,
				( ti.numOfThreads == 1 )? _T("") : _T("s") );
	SetDlgItemText( hDlg, IDC_GROUPBOX, szHeader );
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
					*lpTarget->szText ? lpTarget->szText : _T("<no text>"), // 1024
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

static LRESULT CALLBACK SubTBProc( HWND hTB, UINT uMessage, WPARAM wParam, LPARAM lParam )
{
	/*if( uMessage == WM_SETFOCUS )
	{
		HWND hDlg = GetParent( hTB );
		HWND hLV = GetDlgItem( hDlg, IDC_TARGET_LIST );
		LVITEM li;
		li.mask = LVIF_PARAM;
		li.iItem = (int) ListView_GetCurrentSel( hLV );
		li.iSubItem = 0;
		ListView_GetItem( hLV, &li );

		int k = 0;
		while( k < MAX_SLOTS && g_dwTargetProcessId[ k ] != (DWORD) li.lParam ) ++k;

		if( k < MAX_SLOTS )
		{
			const int iSlider = g_Slider[ k ];
			SendDlgItemMessage( hDlg, IDC_SLIDER, TBM_SETPOS, TRUE, iSlider );
			TCHAR szPercent[ 32 ] = _T("");
			SetDlgItemText( hDlg, IDC_TEXT_PERCENT, 
							GetPercentageString( iSlider, szPercent, _countof(szPercent) ) );
			OutputDebugString(L"PERCENT=");
			OutputDebugString(szPercent);
			OutputDebugString(L"\n");
		}
	}
	else*/ if( uMessage == WM_KILLFOCUS )
	{
		TCHAR szTarget[ CCHPATH ] = _T("");
		GetDlgItemText( GetParent( hTB ), IDC_EDIT_INFO, szTarget, CCHPATH );
		SetSliderIni( szTarget, (int) SendMessage( hTB, TBM_GETPOS, 0, 0 ) );
	}
	else if( uMessage == WM_MOUSEWHEEL )
	{
		return TB_OnMouseWheel( hTB, wParam );
	}

	return CallWindowProc( DefTBProc, hTB, uMessage, wParam, lParam );
}


/*
static int cputime_comp( const void * pv1, const void * pv2 )
{
	const CPUTIME& t1 = *static_cast<const CPUTIME *>( pv1 );
	const CPUTIME& t2 = *static_cast<const CPUTIME *>( pv2 );
	if( t1.pid < t2.pid ) return -1;
	if( t1.pid > t2.pid ) return +1;
	return 0;
}
*/

static void _add_item( HWND hwndList, int iff, TARGETINFO& ti )
{
	LVITEM item = {0};
	item.mask = LVIF_PARAM;
	item.iItem = INT_MAX;
	item.iSubItem = 0;     // must be zero
	item.lParam = (LONG) ti.dwProcessId;
	const int itemIdx = ListView_InsertItem( hwndList, &item );

#define M (48)
	TCHAR szShortText[ M + 1 ] = _T(""); // up to M cch, not incl. the term NUL

	_stprintf_s( szShortText, _countof(szShortText), _T("%5lu"), ti.dwProcessId );

#ifdef _UNICODE
	TCHAR * p = &szShortText[ 0 ];
	while( *p == _T(' ') ) // make leading spaces (if any) prettier
	{
		*p++ = _T('\x2007'); // normal SPACE to Unicode Figure-space
	}
#endif

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

	_stprintf_s( szShortText, _countof(szShortText), _T("%02.0f"), ti._cpu );
	ListView_SetItemText( hwndList, itemIdx, CPU, szShortText );

	if( STRUNCATE == _tcsncpy_s( szShortText, _countof(szShortText), ti.szText, _TRUNCATE ) )
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

	ListView_SetItemText( hwndList, itemIdx, EXE, (LPTSTR) ti.szExe );
	ListView_SetItemText( hwndList, itemIdx, TITLE, szShortText );
	ListView_SetItemText( hwndList, itemIdx, PATH, (LPTSTR) ti.szPath );
	_stprintf_s( szShortText, _countof(szShortText), _T("%lu"), ti.numOfThreads );
	ListView_SetItemText( hwndList, itemIdx, THREADS, szShortText );
}

static int target_comp_CPU( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );
	if( target1._cpu > target2._cpu ) return -1;
	if( target1._cpu < target2._cpu ) return +1;
	
	return (int)( target1.dwProcessId - target2.dwProcessId );
}
static int target_comp_CPU2( const void * pv1, const void * pv2 )
{
	return target_comp_CPU( pv2, pv1 );
}

static int target_comp_PID( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );
	return ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
}
static int target_comp_PID2( const void * pv1, const void * pv2 )
{
	return target_comp_PID( pv2, pv1 );
}

static int target_comp_EXE( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );

	int result = _tcsicmp( target1.szExe, target2.szExe );
	if( ! result )
		result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	
	return result;
}
static int target_comp_EXE2( const void * pv1, const void * pv2 )
{
	return target_comp_EXE( pv2, pv1 );
}

static int target_comp_PATH( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );

	int result = _tcsicmp( target1.szPath, target2.szPath );
	if( ! result )
		result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	
	return result;
}
static int target_comp_PATH2( const void * pv1, const void * pv2 )
{
	return target_comp_PATH( pv2, pv1 );
}
static int target_comp_THREADS( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );
	
	if( target1.numOfThreads != target2.numOfThreads )
		return (INT) target1.numOfThreads - (INT) target2.numOfThreads;
	else
		return (INT) target1.dwProcessId - (INT) target2.dwProcessId;
}
static int target_comp_THREADS2( const void * pv1, const void * pv2 )
{
	return target_comp_THREADS( pv2, pv1 );
}


static int target_comp_TITLE( const void * pv1, const void * pv2 )
{
	const TARGETINFO& target1 = *static_cast<const TARGETINFO *>( pv1 );
	const TARGETINFO& target2 = *static_cast<const TARGETINFO *>( pv2 );

	int result = _tcsicmp( target1.szText, target2.szText );
	if( ! result )
		result = ( target1.dwProcessId > target2.dwProcessId ) ? +1 : -1 ;
	
	return result;
}
static int target_comp_TITLE2( const void * pv1, const void * pv2 )
{
	return target_comp_TITLE( pv2, pv1 );
}
static int pair_comp( const void * pv1, const void * pv2 )
{
	const PROCESS_THREAD_PAIR& pair1 = *static_cast<const PROCESS_THREAD_PAIR *>( pv1 );
	const PROCESS_THREAD_PAIR& pair2 = *static_cast<const PROCESS_THREAD_PAIR *>( pv2 );

	if( pair1.pid < pair2.pid ) return -1;
	if( pair1.pid > pair2.pid ) return +1;

	if( pair1.tid < pair2.tid ) return -1;
	if( pair1.tid > pair2.tid ) return +1;
		
	return 0;
}
/*static int pair_compS( const void * pv1, const void * pv2 )
{
	const PROCESS_THREAD_PAIR& pair1 = *static_cast<const PROCESS_THREAD_PAIR *>( pv1 );
	const PROCESS_THREAD_PAIR& pair2 = *static_cast<const PROCESS_THREAD_PAIR *>( pv2 );

	if( pair1.pid < pair2.pid ) return -1;
	if( pair1.pid > pair2.pid ) return +1;

	return 0;
}*/

typedef struct tagFindImportantText {
	TCHAR * pszText; // must be able to store MAX_WINDOWTEXT cch
	DWORD_PTR dwImportance;
} IMPORTANT_TEXT;

static void GetProcessWindowText(
	const PROCESS_THREAD_PAIR * pairs,
	ptrdiff_t numOfThreads,
	TCHAR * pWindowText // cch: MAX_WINDOWTEXT
)
{
	*pWindowText = _T('\0');

	IMPORTANT_TEXT it;
	it.pszText = pWindowText;
	it.dwImportance = 0;

	for( ptrdiff_t threadIndex = 0; threadIndex < numOfThreads; ++threadIndex )
		EnumThreadWindows( pairs[ threadIndex ].tid, &GetImportantText_EnumProc, (LPARAM) &it );
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
	TARGETINFO ** ppti,
	ptrdiff_t * pMax,
	int listLevel,
	int * pi
)
{
	static CPUTIME * s_lpCPUTime = NULL;
	static ptrdiff_t s_cntCPUTime = 0;
	static ULONGLONG s_tPrev = 0;

	if( ! ppti || ! pMax || ! pi )
	{
#ifdef _DEBUG
		OutputDebugString(_T("Freeing s_lpCPUTime...\n"));
#endif
		if( s_lpCPUTime )
		{
			HeapFree( GetProcessHeap(), 0, s_lpCPUTime );
			s_lpCPUTime = NULL;
			s_cntCPUTime = 0;
		}
		return 0;
	}
	LPTARGETINFO& target = *ppti;
	ptrdiff_t& maxItems = *pMax;

	//** HOTFIX @ 20140306
	memset( target, 0, maxItems * sizeof(TARGETINFO) );
	// HOTFIX END**

	//TCHAR szSysDir[ MAX_PATH + 1 ] = _T("");
	//GetSystemDirectory( szSysDir, MAX_PATH );

	ptrdiff_t numOfPairs = 0;
	PROCESS_THREAD_PAIR * sorted_pairs = AllocSortedPairs( numOfPairs );
	
	if( ! sorted_pairs )
	{
		*pi = 0;
		return 0;
	}

	*pi = (int) numOfPairs; // debug info

	/*WININFO * lpWinInfo = (WININFO*) HeapAlloc( GetProcessHeap(), 0, sizeof(WININFO) );
													//VirtualAlloc( NULL, sizeof(WININFO),
													//MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
		
	if( ! lpWinInfo )
	{
		HeapFree( GetProcessHeap(), 0, sorted_pairs );
		return 0;
	}*/

	const DWORD dwCurrentPID = GetCurrentProcessId();

	FILETIME ft = {0};
	GetSystemTimeAsFileTime( &ft );
	const ULONGLONG tNow = ((ULONGLONG) ft.dwHighDateTime) << 32 | ft.dwLowDateTime;
	const double CPUTimeDenom = ( tNow - s_tPrev ) / 100.0;
	const bool fCheckCPUTime = s_lpCPUTime && CPUTimeDenom != 0.0;
/*
	HANDLE hProcessSnap = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( hProcessSnap == INVALID_HANDLE_VALUE )
	{
		HeapFree( GetProcessHeap(), 0L, lpWinInfo );
		HeapFree( GetProcessHeap(), 0L, sorted_pairs );
		return 0;
	}
	
	PROCESSENTRY32 pe32;
	pe32.dwSize = sizeof( PROCESSENTRY32 );


	for( BOOL bSuccess = Process32First( hProcessSnap, &pe32 );
			bSuccess; bSuccess = Process32Next( hProcessSnap, &pe32 ) )
*/

	ptrdiff_t index = 0; // index of target
	ptrdiff_t cx = 0;    // CPUTime array (s_lpCPUTime) indeX
	for( ptrdiff_t s = 0, cntThreads; s < numOfPairs; s += cntThreads )
	{
		DWORD dwProcessID = sorted_pairs[ s ].pid;
		cntThreads = 1;
		while( s + cntThreads < numOfPairs && sorted_pairs[ s + cntThreads ].pid == dwProcessID )
			++cntThreads;

		if( dwProcessID == 0 ) continue;

		target[ index ].iIFF = IFF_UNKNOWN;

/*
		if( pe32.th32ProcessID == 0 ) // system idle process?
			continue; // TODO***
*/

		// itself or another instance of BES
		/*if( pe32.th32ProcessID == dwCurrentPID
			|| _tcsicmp( pe32.szExeFile, _T( "BES.exe" ) ) == 0	)*/
		if( dwProcessID == dwCurrentPID )
		{
			if( ListLess( listLevel ) ) continue;
				
			target[ index ].fThisIsBES = true;
			target[ index ].iIFF = IFF_SYSTEM;
		}

		HANDLE hProcess = OpenProcess(
			//PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION | PROCESS_VM_READ,
			PROCESS_QUERY_INFORMATION,
			FALSE,
			dwProcessID //pe32.th32ProcessID
		);

		if( hProcess == NULL ) continue;
/*
		if( hProcess == NULL )
		{
			if( ListMore( listLevel ) ) // list everything anyway
			{
				target[ index ].iIFF = IFF_SYSTEM;

				hProcess = OpenProcess(
					PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
					FALSE,
					pe32.th32ProcessID
				);
			}
			else
			{
				continue;
			}
		}
*/
		//if( hProcess != NULL )
		{
			FILETIME ftCreated, ftExited, ftKernel, ftUser;
			if( g_pfnGetProcessTimes
				&& (*g_pfnGetProcessTimes)( hProcess, &ftCreated, &ftExited, &ftKernel, &ftUser ) )
			{
				target[ index ].CPUTime
					= ((ULONGLONG) ftKernel.dwHighDateTime) << 32 | ftKernel.dwLowDateTime;
				target[ index ].CPUTime
					+= ((ULONGLONG) ftUser.dwHighDateTime) << 32 | ftUser.dwLowDateTime;

				if( fCheckCPUTime )
				{
					while( cx < s_cntCPUTime && s_lpCPUTime[ cx ].pid < dwProcessID )
						++cx;

					if( s_lpCPUTime[ cx ].pid == dwProcessID )
					{
						ULONGLONG CPUTimeDiff = target[ index ].CPUTime - s_lpCPUTime[ cx ].t;
						target[ index ]._cpu = (float)( CPUTimeDiff / CPUTimeDenom );
					}
				}
			}

			if( ProcessToPath( hProcess, target[ index ].szPath, MAX_PATH * 2 ) )
			{
				PathToExe( target[ index ].szPath, target[ index ].szExe, MAX_PATH * 2 );
			}
			else
			{
				//_tcscpy_s( target[ index ].szExe, MAX_PATH * 2, pe32.szExeFile );
				//_tcscpy_s( target[ index ].szPath, MAX_PATH * 2, pe32.szExeFile );

				if( ListLess( listLevel ) ) continue;
				_tcscpy_s( target[ index ].szExe, MAX_PATH * 2, _T("System") );
				_tcscpy_s( target[ index ].szPath, MAX_PATH * 2, _T("System") );
					
				target[ index ].iIFF = IFF_SYSTEM;
			}

			///CloseHandle( hProcess ); // close first

			/*
			// HOTFIX--- 20140302 Fix regression in v1.6.1
			if( _tcsicmp( target[ index ].szExe, _T("System") ) == 0 )
			{
				if( ListLess( listLevel ) ) continue;
					
				target[ index ].iIFF = IFF_SYSTEM;
			} //---END HOTFIX
			*/

			if( _tcsicmp( target[ index ].szExe, _T( "BES.exe" ) ) == 0
				&& IsProcessBES( dwProcessID, sorted_pairs + s, cntThreads ) )
			{
				if( ListLess( listLevel ) ) continue;
					
				target[ index ].fThisIsBES = true;
				target[ index ].iIFF = IFF_SYSTEM;
			}
		}
		//else // if( hProcess == NULL )
		//{
			//_tcscpy_s( target[ index ].szExe, MAX_PATH * 2, pe32.szExeFile );
			//_tcscpy_s( target[ index ].szPath, MAX_PATH * 2, pe32.szExeFile );
		//}
/*
		GetProcessDetails2(
			dwProcessID, // pe32.th32ProcessID,
			&target[ index ],
			lpWinInfo,
			sorted_pairs + s,
			cntThreads, //numOfPairs,
			cntThreads //(ptrdiff_t) pe32.cntThreads
		);
*/

		GetProcessWindowText( sorted_pairs + s, cntThreads, target[ index ].szText );
		target[ index ].dwProcessId = dwProcessID; //pe32.th32ProcessID;
		target[ index ].numOfThreads = (DWORD) cntThreads; //pe32.cntThreads;


		// These are absolutely foes (IFF_ABS_FOE)
		// We should make sure they are in the Enemy list.
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			if( IsAbsFoe2( target[ index ].szExe, target[ index ].szPath ) ) //20111230
			{
				target[ index ].iIFF = IFF_ABS_FOE;
			}
		}

		// Detect user-defined foes
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			for( int i = 0; i < g_numOfEnemies; ++i )
			{
				if( Lstrcmpi( target[ index ].szExe, g_lpszEnemy[ i ] ) == 0 )
				{
					target[ index ].iIFF = IFF_FOE;
					break;
				}
			}
		}

		// detect friends
		if( target[ index ].iIFF == IFF_UNKNOWN )
		{
			for( int i = 0; i < g_numOfFriends; ++i )
			{
				if( Lstrcmpi( target[ index ].szExe, g_lpszFriend[ i ] ) == 0 )
				{
					target[ index ].iIFF = IFF_FRIEND;
					break;
				}
			}
		}

		++index;

		if( index == maxItems )
		{
			if( 32768 <= index ) // hardcoded limit
				break;

			LPVOID lpv = HeapReAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, target,
										sizeof(TARGETINFO) * ( maxItems * 2 ) );
			if( lpv )
			{
				maxItems *= 2;
				target = (LPTARGETINFO) lpv;
			}
			else
			{
				index = 0;
				break;
			}
		}
	}

	//HeapFree( GetProcessHeap(), 0, lpWinInfo );


	HeapFree( GetProcessHeap(), 0, sorted_pairs );
 


	if( s_lpCPUTime ) HeapFree( GetProcessHeap(), 0, s_lpCPUTime );
	s_lpCPUTime = (CPUTIME*) HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY,
											sizeof(CPUTIME) * index );
	
	if( s_lpCPUTime )
	{
		for( cx = 0; cx < index; ++cx )
		{
			s_lpCPUTime[ cx ].pid = target[ cx ].dwProcessId;
			s_lpCPUTime[ cx ].t   = target[ cx ].CPUTime;
		}

		// target is based on sorted_pairs, so target is sorted by dwProcessId;
		// hence s_lpCPUTime is already sorted by pid.

		//qsort( s_lpCPUTime, (size_t) index, sizeof(CPUTIME), cputime_comp );

		s_cntCPUTime = index;
		s_tPrev = tNow;
	}
	else s_tPrev = (ULONGLONG) 0;

	return index;
}

static INT_PTR LV_OnCustomDraw( HWND hDlg, HWND hLV, LPARAM lParam, int * hot )
{
	NMLVCUSTOMDRAW& cd = *(NMLVCUSTOMDRAW *) lParam;
	if( cd.nmcd.dwDrawStage == CDDS_PREPAINT
		|| cd.nmcd.dwDrawStage == CDDS_ITEMPREPAINT )
	{
		SetWindowLongPtr( hDlg, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW );
	}
	else if( cd.nmcd.dwDrawStage == ( CDDS_ITEMPREPAINT | CDDS_SUBITEM ) )
	{
		const int itemIndex = (int) cd.nmcd.dwItemSpec;

		// cd.nmcd.uItemState (CDIS_FOCUS bit) is usable only when the LV has focus;
		// otherwise it's unclear from this flag which row is currently selected
		//const bool fHighlight = !!( cd.nmcd.uItemState & CDIS_FOCUS );
		const bool fHighlight = ( itemIndex == ListView_GetCurrentSel( hLV ) );

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
			if( itemIndex == hot[ 0 ] || itemIndex == hot[ 1 ] || itemIndex == hot[ 2 ] )
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
				if( SendMessage( hLV, LVM_GETITEMTEXT, (WPARAM) itemIndex, (LPARAM) &li ) )
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
			else if( cd.iSubItem == EXE || cd.iSubItem == TITLE )
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
	TCHAR szPercent[ 32 ] = _T("");
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