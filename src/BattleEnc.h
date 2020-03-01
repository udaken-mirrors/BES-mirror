#if !defined(BATTLEENC_H)

#define BATTLEENC_H

#if defined(_MSC_VER) && _MSC_VER > 1000
	#pragma once
#endif

//#undef  _WIN32_WINNT
//#undef _WIN32_IE
#define _WIN32_IE 0x0500 // required for NOTIFYICONDATA ...0x0501 = _WIN32_IE_WIN2KSP4(?)
#define WINVER 0x0500
#define _WIN32_WINNT 0x0500 // Windows 2000
#define NTDDI_VERSION NTDDI_WIN2K

#define _WIN32_WINDOWS 0x0500

#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 0

#define APP_CLASS TEXT( "BATTLEENC" )

#define MB_CUTE 0x0UL
// This may not work (Win2K SP4 required):
//#define MB_CUTE MB_ERR_INVALID_CHARS //0x00000008

#ifdef _UNICODE
	#define APP_NAME L"BES \x2014 Battle Encoder Shiras\x00E9"
	#define APP_LONGNAME L"BES \x2014 Battle Encoder Shiras\x00E9 1.7.0-alpha"
#else
	#define APP_NAME "BES - Battle Encoder Shirase"
	#define APP_LONGNAME "BES - Battle Encoder Shirase 1.7.0-alpha"
#endif

#define APP_HOME_URL TEXT( "http://mion.faireal.net/BES/" )

#ifdef _MBCS
	#undef _MBCS
#endif

#ifdef _UNICODE
	#ifndef UNICODE
		#define UNICODE
	#endif
	#define APP_COPYRIGHT L"Copyright \251 2004" L"\x2013" L"2014 mion"
#else
	#ifdef UNICODE
		#undef UNICODE
	#endif
	#define APP_COPYRIGHT "Copyright (c) 2004-2014 mion"
#endif

#ifdef _UNICODE
#define WorA_ "W"
#else
#define WorA_ "A"
#endif

#if defined (_MSC_VER) && _MSC_VER < 1400
	#pragma warning (disable:4514) // unreferenced inline function has been removed
#endif

#pragma warning (push)
#pragma warning (disable:4820 4710 4668 4917) // padding & inline
#include <windows.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <Windowsx.h> // DeleteFont
#include <commctrl.h> // TRACKBAR_CLASS
#include <stdio.h> // fputc
#include <stdlib.h> // abs
#include <shlwapi.h> // DLLVERSIONINFO
#include <math.h> // floor
#include <olectl.h> // Jpeg
#include <shlobj.h> // SHGetFolderPath
#include <process.h>
#include <time.h>
#include <locale.h>
#define PSAPI_VERSION 1
#include <Psapi.h> // GetModuleFileNameEx

#include <errno.h>
#ifndef STRUNCATE
#define STRUNCATE       80
#endif

#include "resource.h"
#include "strings.utf8.h"
#include "sstp.sjis.h"
#pragma warning (pop)

#define WATCHING_IDLE      ( (DWORD) -1 )
#define TARGET_PID_NOT_SET          ( 0UL )
#define TARGET_UNDEF ( TEXT( "<target not specified>" ) )

#define JUST_UPDATE_STATUS          ( 3U )

#define MAX_SLOTS (3)

//#define MAX_THREAD_CNT  128
#define MAX_PROCESS_CNT  256
//#define MAX_WINDOWTEXT 1024
#define MAX_WINDOWTEXT 256
#define CCHPATH   (MAX_PATH*2)
#define CCHPATHEX (MAX_PATH*2+20)

#define UNIT_MAX 400
#define UNIT_MIN 2
#define UNIT_DEF 100

#define ALLOW100 TRUE

#define SLIDER_MIN 1
#if ALLOW100
#define SLIDER_MAX 109
#else
#define SLIDER_MAX 108
#endif
#define SLIDER_DEF 33

#define CP_SJIS 932U

#ifndef CP_UTF8
	#define CP_UTF8 65001U
#endif

#ifndef CREARTYPE_QUALITY
	#define CLEARTYPE_QUALITY 5
#endif


#define SURROGATE_LO( wc ) ( ( (WCHAR) wc ) >= 0xDC00 && ( (WCHAR) wc ) <= 0xDFFF )

#ifndef UNREFERENCED_PARAMETER
	#define UNREFERENCED_PARAMETER(P)((P))
#endif

#define LANG_JAPANESEo ((WORD) -2)
#define LANG_CHINESE_T ((WORD) 2807 )
#define LANG_CHINESE_S ((WORD) 2803 )

#define IS_ENGLISH  ( GetLangauge() == LANG_ENGLISH )
#define IS_FINNISH  ( GetLanguage() == LANG_FINNISH )
#define IS_JAPANESE ( GetLanguage() == LANG_JAPANESE || GetLanguage() == LANG_JAPANESEo )
#define IS_JAPANESEo ( GetLanguage() == LANG_JAPANESEo )
#define IS_CHINESE   ( GetLanguage() == LANG_CHINESE_T || GetLanguage() == LANG_CHINESE_S )
#define IS_CHINESE_T ( GetLanguage() == LANG_CHINESE_T )
#define IS_CHINESE_S ( GetLanguage() == LANG_CHINESE_S )
#define IS_SPANISH   ( GetLanguage() == LANG_SPANISH )
#define IS_FRENCH   ( GetLanguage() == LANG_FRENCH )
#define REPEATED_KEYDOWN( lParam ) ( ( lParam ) & 0x40000000 )

#define IFF_SYSTEM                 -2
#define IFF_FRIEND                 -1
#define IFF_UNKNOWN                 0
#define IFF_FOE                     1
#define IFF_ENEMY                   1
#define IFF_ABS_FOE                 2

#define XLIST_WATCH_THIS           -1
#define XLIST_RESTART_0             0
#define XLIST_RESTART_1             1
#define XLIST_RESTART_2             2
#define XLIST_CANCELED           1024
#define XLIST_NEW_TARGET         2048
#define XLIST_UNFREEZE           4096

// These values may also be used as LOWORD of lp of WM_USER_STOP (as notification)
#define STOP_FROM_TRAY     0x000A
#define NORMAL_TERMINATION 0x1000
#define NOT_WATCHING       0x1001
#define THREAD_NOT_OPENED  0xEEEE
#define TARGET_MISSING     0xF000
// These flags may be used as HIWORD of lp of WM_USER_STOP
#define STOPF_NO_INVALIDATE 0x00010000
#define STOPF_NO_LV_REFRESH 0x00020000

#define UNWATCH_NOW -2

#define WM_USER_HACK       ( WM_APP +   1U )
#define WM_USER_STOP       ( WM_APP +   2U ) // wp: iMyId/JUST_UPDATE_STATUS, lp: STOPFlags
#define WM_USER_RESTART    ( WM_APP +   3U )
#define WM_USER_NOTIFYICON ( WM_APP +  10U )
#define WM_USER_REFRESH    ( WM_APP +  20U )

#define WM_USER_BUTTON     ( WM_APP +  30U )
#define WM_USER_CAPTION    ( WM_APP +  40U )

#if !defined(_MSC_VER) || _MSC_VER < 1400

#include <errno.h>
#ifndef _ERRCODE_DEFINED
typedef int errcode;
typedef int errno_t;
#define _ERRCODE_DEFINED
#endif

#ifndef _RSIZE_T_DEFINED
typedef size_t rsize_t;
#define _RSIZE_T_DEFINED
#endif

#ifndef _PTRDIFF_T_DEFINED
#ifdef  _WIN64
typedef __int64             ptrdiff_t;
#else
typedef _W64 int            ptrdiff_t;
#endif
#define _PTRDIFF_T_DEFINED
#endif

int swprintf_s( wchar_t * szBuffer, size_t cch, const wchar_t * szFormat, ... );
int sprintf_s( char * szBuffer, size_t cch, const char * szFormat, ... );

#ifdef _UNICODE
#define  _stprintf_s swprintf_s
#else
#define  _stprintf_s sprintf_s
#endif

errno_t strcpy_s(
   char *strDestination,
   size_t numberOfElements,
   const char *strSource 
);
errno_t wcscpy_s(
   wchar_t *strDestination,
   size_t numberOfElements,
   const wchar_t *strSource 
);

#ifdef _UNICODE
#define  _tcscpy_s   wcscpy_s
#else
#define  _tcscpy_s   strcpy_s
#endif

errno_t strcat_s(
   char *strDestination,
   size_t numberOfElements,
   const char *strSource 
);
errno_t wcscat_s(
   wchar_t *strDestination,
   size_t numberOfElements,
   const wchar_t *strSource 
);
#if defined(_UNICODE)
#define _tcscat_s wcscat_s
#else
#define _tcscat_s strcat_s
#endif


errno_t fopen_s( 
   FILE** pFile,
   const char *filename,
   const char *mode 
);
errno_t _wfopen_s(
   FILE** pFile,
   const wchar_t *filename,
   const wchar_t *mode 
);
#ifdef _UNICODE
#define _tfopen_s _wfopen_s
#else
#define _tfopen_s fopen_s
#endif

errno_t strncpy_s(
   char *strDest,
   size_t numberOfElements,
   const char *strSource,
   size_t count
);

errno_t wcsncpy_s(
   wchar_t *strDest,
   size_t numberOfElements,
   const wchar_t *strSource,
   size_t count 
);
#if !defined(_TRUNCATE)
#define _TRUNCATE ((size_t)-1)
#endif

int _snprintf_s(
   char *buffer,
   size_t sizeOfBuffer,
   size_t count,
   const char *format,
      ... 
);
int _snwprintf_s(
   wchar_t *buffer,
   size_t sizeOfBuffer,
   size_t count,
   const wchar_t *format,
      ... 
);
#if defined(_UNICODE)
#define _tcsncpy_s   wcsncpy_s
#else
#define _tcsncpy_s   strncpy_s
#endif


#ifdef _UNICODE
#define _sntprintf_s  _snwprintf_s
#else
#define _sntprintf_s  _snprintf_s
#endif

#endif // < VC2005

typedef struct tagTargetInfo {
	TCHAR szExe[ CCHPATH ];
	TCHAR szPath[ CCHPATH ];
	TCHAR szText[ MAX_WINDOWTEXT ];
	ULONGLONG CPUTime;
	
	DWORD dwProcessId;
	int iIFF;
	float _cpu;

	DWORD numOfThreads;
	bool fWatch;
	bool fRealTime;
	bool fThisIsBES;
	bool dummy[5];
} TARGETINFO, *LPTARGETINFO;

typedef struct tagHackParams {
	LPTARGETINFO lpTarget;
	LPTSTR lpszStatus[ 16 ];
	//HWND myHwnd;
	int iMyId;
	int dummy;
} HACK_PARAMS, *LPHACK_PARAMS;
#define cchStatus ((size_t)(MAX_PATH*2))

// --- WM_COPYDATA related ---
#define BES_COPYDATA_ID (0xBA1EC0DEul)

typedef struct tagBesCopyData {
	UINT uCommand;
#define BES_COPYDATA_COMMAND_EXITNOW (0xbecd0000u)
#define BES_COPYDATA_COMMAND_PARSE   (0xbecd0001u)

	UINT uFlags;
#define BESCDF_ANSI 0x0001 // if set, wzCommand is non-unicode (ANSI)
#define BESCDF_SENT 0x0002 // COPYDATA was sent to another process at least once
	DWORD dwParam;  // reserved; could be also used for lo32 of a 64-bit param
	DWORD dwParam2; // reserved; could be also used for hi32 of a 64-bit param

#define CCH_MAX_CMDLINE 1024
	WCHAR wzCommand[ CCH_MAX_CMDLINE ];
} BES_COPYDATA, *LPBES_COPYDATA;
// --- WM_COPYDATA related END ---

unsigned __stdcall Hack( LPVOID dwTargetProcessId );

BOOL SelectWatch( HWND hWnd, HANDLE * phChildThread, LPHACK_PARAMS lphp );
BOOL SetTargetPlus( HWND hWnd, HANDLE * phChildThread, LPHACK_PARAMS lphp, LPCTSTR lpszTargetPath, LPCTSTR lpszTargetExe );

BOOL Unfreeze( HWND hWnd, DWORD dwProcessID );

INT_PTR CALLBACK xList(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

DWORD PathToProcessID( const TCHAR * pPath, const DWORD * pdwExcluded = NULL, ptrdiff_t nExcluded = 0 );

BOOL PathToExe( LPCTSTR strPath, LPTSTR lpszExe, int iBufferSize );
BOOL PathToExe( LPTSTR strPath );
BOOL PathToExeEx( LPTSTR strPath, int iBufferSize );

typedef struct tagProcessThreadPair {
	DWORD pid;
	DWORD tid;
} PROCESS_THREAD_PAIR;
BOOL IsProcessBES( DWORD dwProcessId, 
				  const PROCESS_THREAD_PAIR * pairs = NULL, ptrdiff_t numOfPairs = 0 );

BOOL OpenDebugLog();
BOOL PrintFileHeader( FILE * fp );
BOOL WriteDebugLog( LPCTSTR str );
BOOL CloseDebugLog();

const TCHAR * GetIniPath( void );
BOOL ReadIni( BOOL * pbAlloMulti );


BOOL CheckLanguageMenuRadio( const HWND hWnd );
VOID InitMenuEng( const HWND hWnd );
VOID InitToolTipsEng( TCHAR str[ 4 ][ 256 ] );

#ifdef _UNICODE
VOID InitMenuFin( const HWND hWnd );
VOID InitToolTipsFin( WCHAR str[ 4 ][ 256 ] );
VOID InitMenuJpn( const HWND hWnd );
VOID InitToolTipsJpn( WCHAR str[ 4 ][ 256 ] );
VOID InitMenuSpa( const HWND hWnd );
VOID InitToolTipsSpa( WCHAR str[ 4 ][ 256 ] );

VOID InitToolTipsChiT( WCHAR str[ 4 ][ 256 ] );
VOID InitToolTipsChiS( WCHAR str[ 4 ][ 256 ] );
VOID InitMenuChiT( const HWND hWnd );
VOID InitMenuChiS( const HWND hWnd );

VOID InitToolTipsFre( WCHAR str[ 4 ][ 256 ] );
VOID InitMenuFre( const HWND hWnd );
#endif

BOOL WriteIni( bool fRealTime );

BOOL SetSliderIni( LPCTSTR lpszTarget, const int iSlider );
int GetSliderIni( LPCTSTR lpszTarget, HWND hWnd );
VOID SetWindowPosIni( HWND hWnd );
VOID GetWindowPosIni( POINT * ppt, RECT * prcWin );

VOID InitSWIni( VOID );
VOID SaveCmdShow( HWND hWnd, int nCmdShow );
int GetCmdShow( HWND hWnd );

static BOOL BES_ShowWindow( HWND hCurWnd, HWND hwnd, int iShow );
#define BES_ERROR                -600

#define BES_DELETE_KEY           -100
#define BES_HIDE                    0
#define BES_SHOW                    1
#define BES_SHOW_MANUALLY           2

VOID GShow( HWND hWnd );


HFONT UpdateFont( HFONT hFont );



LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK xList(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK Settings( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );

VOID AboutShortcuts( HWND hWnd );

ptrdiff_t ListProcessThreads( DWORD dwOwnerPID, DWORD * pThreadIDs, ptrdiff_t numOfMaxIDs );
DWORD * ListProcessThreads_Alloc( DWORD dwOwnerPID, ptrdiff_t& numOfThreads );

BOOL SaveSnap( HWND hWnd );
BOOL SaveSnap( LPCTSTR lpszSavePath );

VOID AdjustLength( LPTSTR strPath );

BOOL SliderMoved( WPARAM wParam );

int ParseArgs(
	const TCHAR * lpszCmdLine, 
	size_t cchBuf,
	TCHAR * lpszTargetLongPath,
	TCHAR * lpszTargetLongExe,
	TCHAR * lpszOptions );


VOID SetRealTimeMode( HWND hWnd, bool fRealTime, LPTSTR lpszWindowText, size_t cchWindowText );


VOID Exit_CommandFromTaskbar( HWND hWnd );

DWORD InitNotifyIconData( HWND hWnd, NOTIFYICONDATA * lpni, TARGETINFO * ti );


HWND CreateTooltip ( const HINSTANCE hInst, const HWND hwndBase, LPCTSTR str );
VOID UpdateTooltip( const HINSTANCE hInst, const HWND hwndBase, LPCTSTR str, const HWND hwndToolTip );


BOOL Unwatch( HANDLE& hSema, HANDLE& hThread, volatile BOOL& bWatch );



#ifndef _countof
#define _countof(ar) (sizeof(ar)/sizeof((ar)[0]))
#endif






BOOL SSTP_Aviutl( HWND hWnd, LPCTSTR lpTitle );

//BOOL DirectSSTP( HWND hWnd, const char * hcScript );
//BOOL DirectSSTP( HWND hWnd, const char * hcSakura, const char * hcKero );
BOOL DirectSSTP( HWND hWnd, const char * hcSakura, const char * hcKero, const char * hcCharset );
VOID DirectSSTP( LPCTSTR str );


#define BTN_X1     480
#define BTN_WIDTH  130
#define BTN_X2 ( BTN_X1 + BTN_WIDTH )

#define BTN_HEIGHT_LARGE  75
#define BTN_HEIGHT_SMALL  50

#define BTN0_Y1  30
#define BTN0_Y2 ( BTN0_Y1 + BTN_HEIGHT_LARGE )
#define BTN1_Y1 125
#define BTN1_Y2 ( BTN1_Y1 + BTN_HEIGHT_SMALL )
#define BTN2_Y1 195
#define BTN2_Y2 ( BTN2_Y1 + BTN_HEIGHT_LARGE )
#define BTN3_Y1 330
#define BTN3_Y2 ( BTN3_Y1 + BTN_HEIGHT_SMALL )



int GetButtonId( POINT pt );


#ifndef HIMETRIC_INCH
	#define HIMETRIC_INCH 2540
#endif

//HFONT MyCreateFont( const HDC hDC, LPCTSTR lpszFace, int iPoint, BOOL bBold, BOOL bItalic );
//HFONT MyCreateFont( LPCTSTR lpszFace, int iSize, BOOL bBold, BOOL bItalic );

#define IGNORE_ARGV 200

static inline void EnableLoggingIni( BOOL bEnabled )
{
	WritePrivateProfileString(
		TEXT( "Options" ), 
		TEXT( "Logging" ),
		bEnabled ? TEXT( "1" ) : TEXT( "0" ),
		GetIniPath() );
}


WORD SetLanguage( WORD wLID );
WORD GetLanguage( void );


	// Ignore cases by the file system rules:
	// CompareString doesn't work properly for U+0186, U+0189, U+0191, U+01A9, U+0462, U+04B6,
	// and U+216F.
	// _tcsicmp internally calls LCMapString with LCMAP_LOWERCASE; they work better, as long as
	// setlocale is called first.
#define Lstrcmpi _tcsicmp
static inline bool StrEqualNoCase( const TCHAR * s1, const TCHAR * s2 )
{
	return ! _tcsicmp( s1, s2 );
}
static inline bool StrEqual( const TCHAR * s1, const TCHAR * s2 )
{
	return ! _tcscmp( s1, s2 );
}

bool IsAbsFoe( LPCTSTR strPath );

/*
inline BOOL IsWindowsXPOrLater()
{
	static BOOL bResult = -1;
	if( bResult == -1 )
	{
		OSVERSIONINFO osvi;
		osvi.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
		if( ! GetVersionEx( &osvi ) ) bResult = FALSE;
		else
		{
			bResult = (BOOL)
				( 
					osvi.dwMajorVersion == 5UL && osvi.dwMinorVersion >= 1UL
					||
					osvi.dwMajorVersion > 5UL
				);
		}
	}

	return bResult;
}
*/
HFONT MyCreateFont( HDC hDC, LPCTSTR lpszFace, int iPoint, BOOL bBold, BOOL bItalic );

void OpenBrowser( LPCTSTR lpUrl );




DWORD GetShell32Version( VOID );

// update the Status messages and enable/disable buttons/menus
#define UpdateStatus(hWnd) SendMessage( (hWnd), WM_USER_STOP, JUST_UPDATE_STATUS, 0 )


/*HANDLE
CreateThread1(
    __in_opt  LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in      SIZE_T dwStackSize,
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  LPVOID lpParameter,
    __in      DWORD dwCreationFlags,
    __out_opt LPDWORD lpThreadId
    );*/
HANDLE
CreateThread2(
    __in      unsigned (__stdcall * lpStartAddress)(void *),
    __in_opt  void * lpVoid
);

#ifndef WM_NCUAHDRAWCAPTION 
#define WM_NCUAHDRAWCAPTION  0x00AE
#endif


#ifdef _WIN64
#define FLORAL_LONG(x)     (x)
#define FLORAL_LONG_PTR(x) (x)
#define FLORAL_ULONG_PTR(x) (x)
#else
#define FLORAL_LONG(x)     ((LONG)(x))
#define FLORAL_LONG_PTR(x) ((LONG_PTR)(x))
#define FLORAL_ULONG_PTR(x) ((ULONG_PTR)(x))
#endif

#define PTRDIFF_INT(x)     ((int)(ptrdiff_t)(x))
#define PTRDIFF_UINT(x)    ((unsigned int)(ptrdiff_t)(x))

inline LONG_PTR SetWindowLongPtr_Floral( HWND hWnd, int nIndex, LONG_PTR LongPtr )
{
	return (LONG_PTR) SetWindowLongPtr( hWnd, nIndex, FLORAL_LONG( LongPtr ) );
}

inline LONG_PTR GetWindowLongPtr_Floral( HWND hWnd, int nIndex )
{
	return FLORAL_LONG_PTR( GetWindowLongPtr( hWnd, nIndex ) );
}

inline ULONG_PTR SetClassLongPtr_Floral( HWND hWnd, int nIndex, LONG_PTR LongPtr )
{
	return (ULONG_PTR) SetClassLongPtr( hWnd, nIndex, FLORAL_LONG( LongPtr ) );
}

inline ULONG_PTR GetClassLongPtr_Floral( HWND hWnd, int nIndex )
{
	return FLORAL_ULONG_PTR( GetClassLongPtr( hWnd, nIndex ) );
}

BOOL IsOptionSet( const TCHAR * pCmdLine, const TCHAR * pOption, const TCHAR * pOption2 );

#ifdef _UNICODE
#define UTchar_ wint_t
#else
#define UTchar_ unsigned char
#endif

//*-------------------------------
#endif // !defined(BATTLEENC_H)

inline bool IsMenuChecked( HMENU hMenu, int idm )
{
	return !!( MF_CHECKED & GetMenuState( hMenu, (UINT) idm, MF_BYCOMMAND ) );
}
inline bool IsMenuChecked( HWND hWnd, int idm )
{
	return !!( MF_CHECKED & GetMenuState( GetMenu( hWnd ), (UINT) idm, MF_BYCOMMAND ) );
}
