#define _CRT_SECURE_NO_WARNINGS     //fuck msvc

#pragma warning(push, 0)
// #include "MyBot.h"
#include <dpp/dpp.h>
#include <thread>
#pragma warning(pop)


/* Be sure to place your token in the line below.
 * Follow steps here to get a token:
 * https://dpp.dev/creating-a-bot-application.html
 * When you invite the bot, be sure to invite it with the 
 * scopes 'bot' and 'applications.commands', e.g.
 * https://discord.com/oauth2/authorize?client_id=940762342495518720&scope=bot+applications.commands&permissions=139586816064
 */
const std::string    BOT_TOKEN    = "Nzk1MjgzNjM2MDg1MzI1ODI1.Gi9kUj.ZBA78aLZeE_ZNnQuQM3LYIXTIfq11eYNMcUKbk";

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) 
{

    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if(argc < 2) {
        return -1;
    }

    int clientid = 0;

    swscanf(argv[1], L"%d", &clientid);

    /* Create bot cluster */
    dpp::cluster bot(BOT_TOKEN, dpp::i_default_intents,
            0,          //shards
            clientid, 5,       //cluster id/ max cluster
            true,       //compressed?
            { dpp::cp_aggressive, dpp::cp_aggressive, dpp::cp_aggressive },
            10, 0);

    /* Output simple log messages to stdout */
    bot.on_log(dpp::utility::cout_logger());

    unsigned int procid = GetProcessId(GetCurrentProcess());
    
    std::string str = "Process " + std::to_string(procid) + " answered!"; 

	dpp::message msg(&bot);
    msg.set_guild_id(664744934003310592);
    msg.set_channel_id(1056052934921699388);
    msg.set_content(str);

    /* Start the bot */
    bot.start(true);

    while(true) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        bot.message_create(msg);
    }

    return 0;
}
