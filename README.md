# DiscordMusicBee

Discord presence for MusicBee

## Plugin

See `lib/MusicBeeIPC_plugin/README.txt` to install `MusicBeeIPC` to `MusicBee/Plugins`
https://getmusicbee.com/addons/plugins/486/musicbee-ipc/

// TODO: install script for this

## Libs

`Rapid JSON` : `lib/rapidjson` : https://github.com/Tencent/rapidjson

`Discord Presence` : `lib/discord-rpc` : https://github.com/discord/discord-rpc

`MusicBeeIPC` : `lib/MusicBeeIPC_plugin` : http://www.zorexxlkl.com/musicbeeipc

## Building

Set `DISCORD_APP_ID` in system environment variables to your application ID from: https://discord.com/developers/applications/

Use `src/convert_icon.py` if you want some other image to be converted to `.ico` and `.res`
