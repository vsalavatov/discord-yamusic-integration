# Discord - Ya.Music rich presence integration (unofficial)

### Installation & Usage
* Place discord_game_sdk.so to `/usr/lib/libdiscord_game_sdk.so` (or modify cmake script to use local version). 
* Install boost 1.65
```bash
$ sudo apt install libboost-all-dev
```
* Compile sources and run `discord_yamusic_integration`
* Install web-extension (see Releases) that sends information about current song to the server
    * also you can install web-extension from sources (see `browser-extension/`)