#define _CRT_SECURE_NO_WARNINGS     //fuck msvc
#define UNICODE

//#include <windows.h>
//#include <shlobj.h>

#include <dpp/cluster.h>
#include <dpp/once.h>

const std::string    BOT_TOKEN    = "Nzk1MjgzNjM2MDg1MzI1ODI1.GJQ65H.orWw99qm-DPXVNYJxb767PoppSohLsphOJKcJw";

int main()
{
    try {
        /* Create bot cluster */
        dpp::cluster bot(BOT_TOKEN,
            dpp::i_default_intents,
            0, 0, 2, true,
            { dpp::cp_aggressive, dpp::cp_aggressive, dpp::cp_aggressive },
            10, 0);
    

        /* Output simple log messages to stdout */
        bot.on_log(dpp::utility::cout_logger());

        /* Handle slash command */
        bot.on_slashcommand([](const dpp::slashcommand_t& event) {
             if (event.command.get_command_name() == "ping") {
                event.reply("Pong!");
            }
        });

        /* Register slash command here in on_ready */
        bot.on_ready([&bot](const dpp::ready_t& event) {
            /* Wrap command registration in run_once to make sure it doesnt run on every full reconnection */
            if (dpp::run_once<struct register_bot_commands>()) {
                bot.global_command_create(dpp::slashcommand("ping", "Ping pong!", bot.me.id));
            }
        });

        dpp::message newmsg(&bot);
        newmsg.set_guild_id(664744934003310592);
        newmsg.set_channel_id(1056052934921699388);
    
        /* Start the bot */
        bot.start(true);


        std::thread t1([&bot, &newmsg]() {
            char buffer[1001];
        while (true) {
            snprintf(buffer, 1000, "Hello world! Running on %s. Current time is %s",
                dpp::utility::version().c_str(),
                dpp::utility::current_date_time().c_str());
            newmsg.set_content(std::string(buffer));
            bot.message_create(newmsg, [](const dpp::confirmation_callback_t& tres) {
                assert(!tres.is_error());
                });
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
            });

        t1.join();

    }
    catch (dpp::exception& e) {
        std::cout << "\n" << e.what();
    }



    return 0;
}
