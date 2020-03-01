#include "BattleEnc.h"

bool g_fRealTime = false;
HANDLE g_hMutexDLL = NULL; // used in list.cpp
HINSTANCE g_hInst = NULL;
HWND g_hWnd = NULL;
HWND g_hListDlg = NULL;
HWND g_hSettingsDlg = NULL;

BOOL EnableDebugPrivilege( HANDLE * phToken, DWORD * pPrevAttributes );
BOOL SetDebugPrivilege( HANDLE hToken, BOOL bEnable, DWORD * pPrevAttributes );

void ShowCommandLineHelp( HWND hWnd );

static HHOOK hKeyboardHook = NULL;
static LRESULT CALLBACK KeyboardHookProc( int nCode, WPARAM wParam, LPARAM lParam );

static DWORD _bes_init( int& nCmdShow );
static void _bes_uninit( void );
static void _prevent_dll_preloading( void );
static HWND _create_window( HINSTANCE hInstance, DWORD dwStyle );
#if defined(_MSC_VER) && 1400 <= _MSC_VER
void invalid_parameter_handler( const wchar_t * expr, const wchar_t * func, const wchar_t * file, unsigned int line, uintptr_t reserved )
{
	(void) expr; (void) func; (void) file; (void) line; (void) reserved;
	MessageBox( NULL, _T("Invalid Parameter!"), APP_NAME, MB_OK | MB_ICONERROR );
}
#endif

int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( lpCmdLine );

	g_hInst = hInstance;

	setlocale( LC_ALL, "English_us.1252" ); // _tcsicmp doesn't work properly in the "C" locale

#if defined(_MSC_VER) && 1400 <= _MSC_VER
	_set_invalid_parameter_handler( invalid_parameter_handler );
#endif

	_prevent_dll_preloading();

	const DWORD dwStyle = _bes_init( nCmdShow );

	if( ! dwStyle )
	{
		_bes_uninit();
		return 0;
	}

	HWND hWnd = _create_window( hInstance, dwStyle );

	if( ! hWnd )
	{
		_bes_uninit();
		return 0;
	}

	g_hWnd = hWnd;

	HACCEL hAccelTable
		= LoadAccelerators( hInstance, MAKEINTRESOURCE( IDC_BATTLEENC ) ); // auto-freed
	
	ShowWindow( hWnd, nCmdShow );
	if( nCmdShow != SW_SHOWMINNOACTIVE )
	{
		// Workaround to make AutoHotkey happy
		BringWindowToTop( hWnd );
		SetForegroundWindow( hWnd );
		SetActiveWindow( hWnd );
		SetFocus( hWnd );
	}
	UpdateWindow( hWnd );

	HANDLE hToken = NULL;
	DWORD dwPrevAttributes = 0ul;
	BOOL bPriv = EnableDebugPrivilege( &hToken, &dwPrevAttributes );

	MSG msg;
	while ( GetMessage( &msg, NULL, 0U, 0U ) )
	{
		if( msg.hwnd != hWnd )
		{
			if( msg.message == WM_LBUTTONUP )
			{
				SendMessage( hWnd, WM_USER_BUTTON, (WPARAM) msg.hwnd, 0L );
			}
		}
	
		if ( ! TranslateAccelerator( msg.hwnd, hAccelTable, &msg ) ) 
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
	}

	if( hToken )
	{
		if( bPriv ) 
			SetDebugPrivilege( hToken, FALSE, &dwPrevAttributes );

		CloseHandle( hToken );
	}

	_bes_uninit();

	return (int) msg.wParam;
}

typedef BOOL (WINAPI * SetDllDirectory_t)(LPCTSTR);
static void _prevent_dll_preloading( void )
{
	HMODULE hModuleKernel32 = SafeLoadLibrary( _T("Kernel32.dll") );
	if( hModuleKernel32 )
	{
		SetDllDirectory_t pfnSetDllDirectory
			= (SetDllDirectory_t)(void *) GetProcAddress( hModuleKernel32, "SetDllDirectory" WorA_ );

		// This is a better fix, but only valid for XP SP1+
		if( pfnSetDllDirectory )
			(*pfnSetDllDirectory)( _T("") );

		FreeLibrary( hModuleKernel32 );
	}
}

static DWORD _bes_init( int& nCmdShow )
{
	OleInitialize( NULL ); //*

	BOOL bAllowMulti = FALSE;
	ReadIni( &bAllowMulti ); // Load conf. incl. logging-setting (so do this before OpenDebugLog)
	OpenDebugLog();

	TCHAR strOptions[ 256 ];
	const int iSlider = ParseArgs( GetCommandLine(), _countof(strOptions), NULL, NULL, strOptions );

	if( IsOptionSet( strOptions, _T("--help"), _T("-h") )
		|| IsOptionSet( strOptions, _T("/?"), NULL ) )
	{
		ShowCommandLineHelp( NULL );
		return 0;
	}

	const BOOL bEmptyCmd = ( iSlider == IGNORE_ARGV );
	const BOOL bExitNow = ! bEmptyCmd && IsOptionSet( strOptions, _T("--exitnow"), _T("-e") );
	const BOOL bMinimize = ! bEmptyCmd && ! bExitNow && IsOptionSet( strOptions, _T("--minimize"), _T("-m") );
	const BOOL bUnlimit = ! bEmptyCmd && ! bExitNow && IsOptionSet( strOptions, _T("--unlimit"), _T("-u") );
	if( IsOptionSet( strOptions, _T("--allow-multi"), NULL ) )
		bAllowMulti = TRUE;
	if( IsOptionSet( strOptions, _T("--disallow-multi"), NULL ) )
		bAllowMulti = FALSE;

	if( bMinimize )
	{
		//WriteDebugLog( _T( "SW_HIDE" ) );
		nCmdShow = SW_SHOWMINNOACTIVE;
	}

	DWORD_PTR Handled = FALSE;
	HWND hwndToShow = NULL;
	BES_COPYDATA mydata;

	// deny multiple instances?
	//if( ! bAllowMulti || bExitNow )
	{
		HWND hwndFound = NULL;
		memset( &mydata, 0, sizeof(mydata) );
#ifdef _UNICODE
		wcsncpy_s( mydata.wzCommand, CCH_MAX_CMDLINE, GetCommandLineW(), _TRUNCATE );
#else
		// mydata.wzCommand can store 2*cchMax CHARs; use one half only
		strncpy_s( (char*) mydata.wzCommand, CCH_MAX_CMDLINE, 
					GetCommandLineA(), _TRUNCATE );
		mydata.uFlags |= BESCDF_ANSI;
#endif

#ifdef _DEBUG
		mydata.dwParam = GetCurrentProcessId();
#endif
		COPYDATASTRUCT cd;
		cd.dwData = BES_COPYDATA_ID;
		cd.cbData = (DWORD) sizeof(mydata);
		cd.lpData = &mydata;

		HWND hwndList[ 32 ];
		memset( hwndList, 0, sizeof(hwndList) );

		// safety prevents an infinite loop, possibly caused by FindWindowEx if the window order
		// is changed before this for-loop is broken.
		for( ptrdiff_t idxFound = 0; 
			( hwndFound = FindWindowEx( NULL, hwndFound, APP_CLASS, NULL ) ) != NULL && idxFound < 32;
			++idxFound )
		{
			if( bEmptyCmd )
			{
				hwndToShow = hwndFound;
				break;
			}

			ptrdiff_t k = 0;
			for( ; k < idxFound && hwndList[ k ] != hwndFound; ++k )
				;
			
			// ignore an already-seen window, if any
			if( k < idxFound ) continue;

			hwndList[ idxFound ] = hwndFound;
			
			if( bMinimize && ! bExitNow )
			{
				if( IsWindowVisible( hwndFound ) )
				{
					//ShowWindow( hwndFound, SW_MINIMIZE );
					ShowWindow( hwndFound, SW_SHOWMINNOACTIVE ); //changed@20120425
				}
			}
			else hwndToShow = hwndFound;

			if( bExitNow )
				mydata.uCommand = BES_COPYDATA_COMMAND_EXITNOW;
			else
				mydata.uCommand = BES_COPYDATA_COMMAND_PARSE;
	
#ifdef _DEBUG
			TCHAR str[ 1024 ] = _T("");
			_stprintf_s( str, 1024, _T( "BES PID 0x%08lx: Sending message to %p" ), 
						GetCurrentProcessId(), hwndFound );
			WriteDebugLog( str );
#endif
			Handled = FALSE;
#ifdef _DEBUG
			LRESULT lres =
#endif
				SendMessageTimeout( hwndFound,
									WM_COPYDATA,
									(WPARAM)(HWND) NULL,
									(LPARAM) &cd,
									SMTO_NORMAL | SMTO_ABORTIFHUNG,
									15 * 1000,
									&Handled );
			
			mydata.uFlags |= BESCDF_SENT;
	
#ifdef _DEBUG
			_stprintf_s( str, 1024, _T( "BES PID 0x%08lx: Result=%ld Handled=%d (from %p)" ), 
						GetCurrentProcessId(), (long) lres, (BOOL) Handled, hwndFound );
			WriteDebugLog( str );
#endif
			if( Handled || ! bAllowMulti && ! bUnlimit && ! bExitNow )
				break;
		}
	}

	if( bExitNow || bUnlimit && ( mydata.uFlags & BESCDF_SENT )
		|| Handled || hwndToShow && ! bAllowMulti )
	{
		if( hwndToShow && ! bMinimize && ! bExitNow && ! bAllowMulti )
		{
			// Workaround to make AutoHotkey happy (another BES is now tray-ed)
			ShowWindow( hwndToShow, SW_HIDE );
		
			ShowWindow( hwndToShow, SW_RESTORE );
			
			BringWindowToTop( hwndToShow );
			SetForegroundWindow( hwndToShow );
			SetActiveWindow( hwndToShow );
			SetFocus( hwndToShow );
		}

		return 0UL;
	}

	g_hMutexDLL = CreateMutex( NULL, FALSE, NULL );

	INITCOMMONCONTROLSEX iccex; 
	iccex.dwICC = ICC_WIN95_CLASSES;
	iccex.dwSize = sizeof( INITCOMMONCONTROLSEX );
	InitCommonControlsEx( &iccex );

	g_fRealTime = !! GetPrivateProfileInt( TEXT("Options"), TEXT("RealTime"), FALSE, GetIniPath() );

	SetPriorityClass( GetCurrentProcess(), 
						(DWORD)( g_fRealTime ? REALTIME_PRIORITY_CLASS : HIGH_PRIORITY_CLASS ) );
	SetThreadPriority( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );

	InitSWIni();

	hKeyboardHook = SetWindowsHookEx( WH_KEYBOARD, &KeyboardHookProc, NULL, GetCurrentThreadId() );

	DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

	if( ! bMinimize )
		dwStyle |= WS_VISIBLE;
	
	return dwStyle;
}

static void _bes_uninit( void )
{
	if( hKeyboardHook )
	{
		UnhookWindowsHookEx( hKeyboardHook );
		hKeyboardHook = NULL;
	}

	if( g_hWnd )
	{
		WriteIni( g_fRealTime );
		g_hWnd = NULL;
	}

	if( g_hMutexDLL )
	{
		CloseHandle( g_hMutexDLL );
		g_hMutexDLL = NULL;
	}
		
	OleUninitialize();
	CloseDebugLog();
}

static HWND _create_window( HINSTANCE hInstance, DWORD dwStyle )
{
	HWND hWnd = NULL;

	WNDCLASSEX wcex = {0};
	wcex.cbSize = sizeof( WNDCLASSEX ); 
	wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	wcex.lpfnWndProc	= &WndProc;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon( hInstance, MAKEINTRESOURCE( IDI_IDLE ) );
	wcex.hIconSm		= wcex.hIcon;
	wcex.hCursor		= NULL;
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= MAKEINTRESOURCE( IDC_BATTLEENC );
	wcex.lpszClassName	= APP_CLASS;

	if( RegisterClassEx( &wcex ) )
	{
		POINT pt;
		RECT rcWin;
		GetWindowPosIni( &pt, &rcWin );

		hWnd = CreateWindow( APP_CLASS, TEXT(""), dwStyle,
							(int) pt.x,
							(int) pt.y,
							(int)( rcWin.right - rcWin.left ),
							(int)( rcWin.bottom - rcWin.top ),
							NULL, NULL, hInstance, NULL );
	}

	return hWnd;
}


#define FIRST_PRESSED_CONTEXT_0( lParam ) ( ! ( ((ULONG_PTR)lParam) & 0xE0000000UL ) )
#define BEING_PRESSED( lParam )  ( ! ( ((ULONG_PTR)(lParam)) & 0x80000000 ) )
#define BEING_RELEASED( lParam ) (     ((ULONG_PTR)(lParam)) & 0x80000000   )
#define FIRST_PRESSED( lParam )           ( ! ( ((ULONG_PTR)lParam) & 0xC0000000 ) )
#define KEY_DOWN(vk) ( (int) GetKeyState( (vk) ) < 0 )
static LRESULT CALLBACK KeyboardHookProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	if( nCode < 0 )
	{
		return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
	}
#define IsMenuMode(lp) ((lp) & 0x10000000L)
	else if( FIRST_PRESSED( lParam ) && ! IsMenuMode( lParam ) )
	{
		if( wParam == 'B' ) // boss-key
		{
			HWND hwndFocus = GetFocus();
			if( hwndFocus && hwndFocus != g_hWnd )
				hwndFocus = GetParent( hwndFocus );

			if( hwndFocus
				&& ( hwndFocus == g_hWnd || hwndFocus == g_hListDlg || hwndFocus == g_hSettingsDlg ) )
			{
				// SendMessage may time out if the dialogbox is there but is about to be destroyed.
				if( g_hListDlg )
					SendMessageTimeout( g_hListDlg, WM_COMMAND, IDCANCEL, 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, NULL );
			
				if( g_hSettingsDlg )
					SendMessageTimeout( g_hSettingsDlg, WM_COMMAND, IDCANCEL, 0,
										SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, NULL );
			
				ShowWindow( g_hWnd, SW_HIDE );
				return 1;
			}
		}

		if( ! g_hListDlg )
		{
			return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
		}

		HWND hFocus = GetFocus();

		if( hFocus && hFocus != g_hListDlg )
			hFocus = GetParent( hFocus );

		if( hFocus != g_hListDlg 
			||
			KEY_DOWN( VK_CONTROL ) && wParam != _T('R')
		)
		{
			return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
		}

		if( wParam == VK_F5 || wParam == _T('R') )
		{
			if( SendMessageTimeout( g_hListDlg, WM_COMMAND, MAKEWPARAM( IDC_RELOAD, BN_CLICKED ), 
								(LPARAM) GetDlgItem( g_hListDlg, IDC_RELOAD ),
								SMTO_NORMAL | SMTO_ABORTIFHUNG, 3000, NULL ) ) return 1;
		}

		int wId = -1;

		if( wParam == _T('L') ) wId = IDOK;
		else if( wParam == _T('W') ) wId = IDC_WATCH;
		else if( wParam == _T('C') ) wId = IDCANCEL;
		else if( wParam == _T('H') ) wId = IDC_HIDE;
		else if( wParam == _T('S') ) wId = IDC_SHOW;
		else if( wParam == _T('K') ) wId = IDC_SHOW_MANUALLY;
		else if( wParam == _T('A') ) wId = IDC_LISTALL_SYS;
		else if( wParam == _T('F') ) wId = IDC_FRIEND;
		else if( wParam == _T('U') ) wId = IDC_RESET_IFF;
		else if( wParam == _T('O') ) wId = IDC_FOE;
		else if( wParam == _T('Z') ) wId = IDC_UNFREEZE;
		else if( wParam >= _T('A') && wParam <= _T('Z') ) return 1;
		else return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );

		if( SendMessageTimeout( g_hListDlg, WM_COMMAND, MAKEWPARAM( wId, BN_CLICKED ), 
								(LPARAM) GetDlgItem( g_hListDlg, wId ),
								SMTO_NORMAL | SMTO_ABORTIFHUNG, 1000, NULL ) ) return 1;
	}

	return CallNextHookEx( hKeyboardHook, nCode, wParam, lParam );
}
