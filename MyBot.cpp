#define _CRT_SECURE_NO_WARNINGS     //fuck msvc

#pragma warning(push, 0)
#include "MyBot.h"
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
const std::string    BOT_TOKEN    = "Nzk1MjgzNjM2MDg1MzI1ODI1.GJQ65H.orWw99qm-DPXVNYJxb767PoppSohLsphOJKcJw";

int main()
{
    /* Create bot cluster */
    dpp::cluster bot(BOT_TOKEN);

    /* Output simple log messages to stdout */
    bot.on_log(dpp::utility::cout_logger());

	dpp::message newmsg(&bot);
    newmsg.set_guild_id(664744934003310592);
    newmsg.set_channel_id(1056052934921699388);

	std::ifstream infile("dump-msvc.txt");
	infile.seekg(0, infile.end);
	int len = infile.tellg();
	infile.seekg(0, infile.beg);
	char *buf = new char[len+1];
	infile.read(buf, len);
	std::string str = std::string(buf);
	infile.close();
	delete[] buf; 

	newmsg.add_file("file1.txt", str);
	newmsg.add_file("file2.txt", str);

    /* Handle slash command */
    bot.on_slashcommand([&bot, &newmsg](const dpp::slashcommand_t& event) {
         if (event.command.get_command_name() == "ping") {
            // event.reply("ready!");
			event.reply(newmsg);
			//somehow message_create work with everything over 8 MiB
			bot.message_create(newmsg);
        }
    });

    /* Register slash command here in on_ready */
    bot.on_ready([&bot](const dpp::ready_t& event) {
        /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
        if (dpp::run_once<struct register_bot_commands>()) {
            bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
        }
    });

    /* Start the bot */
    bot.start(false);

    return 0;
}
