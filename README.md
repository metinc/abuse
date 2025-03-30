# Abuse

![hero](https://github.com/user-attachments/assets/143352b6-dfe5-474f-926d-dd7f74548a85)

This is an enhanced version of Abuse based on the original source code that was released to the public domain. See the [Changelog](CHANGELOG.md) for details.

If you have any problems with the game or just want to chat, join my [Discord server](https://discord.gg/T8Kw6kwN). I'll see what I can do to help.

To report a bug, please create a new issue here on GitHub. Pull requests are welcome!

## Table of Contents

- [Getting Started](#getting-started)
- [Configuration](#configuration)
- [Resources](#resources)
- [Acknowledgments](#acknowledgments)

## Getting Started

To install the game, see the last release available for your platform in the release section.

### Data Files

While this repository contains all data files needed to play the base game, these assets come from different sources with varying licenses and historical records. My hope is that the educational and non-profit intentions of this repository will enable it to stay hosted and available. If you prefer to use only clearly-licensed content, please replace the included assets with the public domain subset of the original shareware content available from various archives online.

Save files and configuration are stored in the user folder, which can override default files in the game folder. The game only looks for built-in files if they weren't already found in the user folder. This allows adding [custom levels](https://dl.pancik.com/abuse/levels/) or [mods](https://dl.pancik.com/abuse/addons/) without affecting the original bundled game files.

Default paths for user data:

- Windows: `%APPDATA%\abuse`
- macOS: `~/Library/Application Support/abuse`
- Linux: `~/.local/share/abuse`

For custom content, there are three types to consider:

| Type                  | Installation                                  | Launch                                        |
| --------------------- | --------------------------------------------- | --------------------------------------------- |
| Custom Levels         | Place level files in the `data/levels` folder | `abuse -f levels/levelname.spe`               |
| Regular Mods          | Place mod files in the `data/addon` folder    | `abuse -a modname`                            |
| Total Conversion Mods | Place directly in the data folder             | Launches automatically when starting the game |

### Cheats

To use cheats, press <kbd>c</kbd> to open the console and type the desired cheat command. The mouse cursor must be inside the console window for input. Press enter when done, or type "quit"/"exit" to close the console.

Available cheats:

- `god` - Makes you invulnerable to all damage
- `giveall` - Gives all weapons and maximum ammunition
- `flypower` - Grants Anti-Gravity Boots effect
- `sneakypower` - Grants Cloak effect
- `fastpower` - Grants Flash Speed effect
- `healthpower` - Grants Ultra-Health effect
- `nopower` - Removes all active special abilities

## Configuration

Configuration is stored in `config.txt` in the user folder. It will be created if it doesn't exist at launch. Lines starting with `;` are comments. Use `1` to enable and `0` to disable options.

### Config File Options

#### Display Settings

- `fullscreen` - Fullscreen mode (`0` - window, `1` - fullscreen window, `2` - fullscreen)
- `borderless` - Enable borderless window mode
- `vsync` - Enable vertical sync
- `virtual_width` - Internal game resolution width
- `virtual_height` - Internal game resolution height (calculated from aspect ratio if not specified)
- `screen_width` - Game window width
- `screen_height` - Game window height
- `linear_filter` - Use linear texture filter (nearest is default)
- `hires` - Enable high resolution menu and screens (`2` for Bungie logo)
- `big_font` - Enable big font
- `mouse_scale` - Mouse to game scaling (`0` - match desktop, `1` - match game screen)

The game is designed to be played at an internal resolution of 320×200 (`virtual_width`×`virtual_height`). Using a higher resolution may reveal some hidden areas. However, when using the editor, a higher resolution is recommended for better visibility and usability.

#### Audio Settings

- `volume_sound` - Sound effects volume (0-127)
- `volume_music` - Music volume (0-127)
- `mono` - Use mono audio only
- `no_music` - Disable music
- `no_sound` - Disable sound effects
- `soundfont` - Path to custom soundfont file. Custom or bundled `AWE64 Gold Presets.sf2` or `Roland SC-55 Presets.sf2`

#### Game Settings

- `local_save` - Save config and files locally
- `grab_input` - Grab mouse to window
- `editor` - Enable editor mode
- `physics_update` - Physics update time in ms (65ms/15FPS original)
- `language` - Game language (`english`, `german`, `french`)

### Key Bindings

Default control scheme:

| Action      | Default Binding                          |
| ----------- | ---------------------------------------- |
| Left        | <kbd>←</kbd> <kbd>A</kbd>                |
| Right       | <kbd>→</kbd> <kbd>D</kbd>                |
| Up/Jump     | <kbd>↑</kbd> <kbd>W</kbd>                |
| Down/Use    | <kbd>↓</kbd> <kbd>S</kbd>                |
| Prev Weapon | <kbd>Q</kbd> <kbd>Mouse Wheel Up</kbd>   |
| Next Weapon | <kbd>E</kbd> <kbd>Mouse Wheel Down</kbd> |
| Fire        | <kbd>Mouse Left</kbd>                    |
| Special     | <kbd>Mouse Right</kbd>                   |

Special key codes for config file:

- `LEFT`, `RIGHT`, `UP`, `DOWN` - Cursor keys and keypad
- `CTRL_L`, `CTRL_R` - Left and right Ctrl
- `ALT_L`, `ALT_R` - Left and right Alt
- `SHIFT_L`, `SHIFT_R` - Left and right Shift
- `F1` - `F10` - Function keys
- `TAB`, `BACKSPACE`, `ENTER` - Standard keys
- `INSERT`, `DEL`, `PAGEUP`, `PAGEDOWN` - Navigation keys
- `CAPS`, `NUM_LOCK` - Lock keys
- `SPACE` - Spacebar

Hardcoded Keys:

- <kbd>1-7</kbd> - Direct weapon selection
- <kbd>Escape/Space/Enter</kbd> - Reset level on death
- <kbd>P</kbd> - Pause game
- <kbd>C</kbd> - Cheat/chat console
- <kbd>F1</kbd> - Show help/controls screen
- <kbd>F5</kbd> - Quick save on save consoles (slot 1/"save0001.spe")
- <kbd>F6</kbd> - Toggle window input grab
- <kbd>F7</kbd> - Toggle mouse scale type
- <kbd>F8</kbd> - Toggle gamepad use
- <kbd>F9</kbd> - Quick load
- <kbd>F10</kbd> - Toggle window/fullscreen mode
- <kbd>Print Screen</kbd> - Take a screenshot

Default Controller Bindings:

- D-pad, left stick - Move in all directions (game and menus)
- Home - Show help/controls screen
- Back - Acts as <kbd>Escape</kbd> key
- Start - Acts as <kbd>Enter</kbd> key

### Gamepad Support

Gamepad options:

- `ctr_aim` - Enable right stick aiming
- `ctr_cd` - Crosshair distance from player
- `ctr_rst_s` - Right stick/aiming sensitivity
- `ctr_rst_dz` - Right stick/aiming dead zone
- `ctr_lst_dzx` - Left stick horizontal dead zone
- `ctr_lst_dzy` - Left stick vertical dead zone

Button binding names:

- `ctr_a`, `ctr_b`, `ctr_x`, `ctr_y` - Face buttons
- `ctr_left_shoulder`, `ctr_right_shoulder` - Shoulder buttons
- `ctr_left_trigger`, `ctr_right_trigger` - Triggers
- `ctr_left_stick`, `ctr_right_stick` - Stick clicks

### Command Line Arguments

#### Core Settings

| Argument          | Description                                |
| ----------------- | ------------------------------------------ |
| `-lsf <filename>` | Custom startup file (default: `abuse.lsp`) |
| `-a <name>`       | Load addon from `addon/<name>/<name>.lsp`  |
| `-f <filename>`   | Load specific level file                   |
| `-nodelay`        | Disable frame delay/timing control         |

#### Network Settings

| Argument                | Description                |
| ----------------------- | -------------------------- |
| `-nonet`                | Disable networking         |
| `-port <number>`        | Set network port (1-32000) |
| `-net <hostname>`       | Connect to host name or IP |
| `-server <name>`        | Run as server              |
| `-min_players <number>` | Set minimum players (1-8)  |
| `-ndb <number>`         | Network debug level (1-3)  |
| `-fs <address>`         | File server address        |
| `-remote_save`          | Store saves on server      |

#### Development/Debug

| Argument        | Description                 |
| --------------- | --------------------------- |
| `-edit`         | Launch editor mode          |
| `-fwin`         | Open foreground editor      |
| `-bwin`         | Open background editor      |
| `-owin`         | Open objects window         |
| `-no_autolight` | Disable auto lighting       |
| `-nolight`      | Disable all lighting        |
| `-bastard`      | Bypass filename security    |
| `-size`         | Custom window size (editor) |
| `-lisp`         | Start LISP interpreter      |
| `-ec`           | Empty cache                 |
| `-t <filename>` | Insert tiles from file      |
| `-cprint`       | Enable console printing     |

#### Audio Settings

| Argument                 | Description                  |
| ------------------------ | ---------------------------- |
| `-sfx_volume <number>`   | Sound effects volume (0-127) |
| `-music_volume <number>` | Music volume (0-127)         |

## Resources

### Game Information

[Moby Games page](http://www.mobygames.com/game/abuse)  
[Abuse homepage](http://web.archive.org/web/20010517011228/http://abuse2.com)  
[Free Abuse (fRaBs) homepage](http://web.archive.org/web/20010124070000/http://www.cs.uidaho.edu/~cass0664/fRaBs)  
[Abuse fan page](http://web.archive.org/web/19970701080256/http://games.3dreview.com/abuse/index.html)
[Gameplay video](http://www.youtube.com/watch?v=0Q0SbdDfnFI)

### Downloads

[ETTiNGRiNDER's Fortress](https://ettingrinder.youfailit.net/abuse-main.html)  
[Assorted Abuse Files](https://dl.pancik.com/abuse/)
[HMI to MIDI converter](http://www.ttdpatch.net/midi/games.html)

### Source code releases

[Original source code](https://archive.org/details/abuse_sourcecode)  
[Anthony Kruize Abuse SDL port (2001)](http://web.archive.org/web/20070205093016/http://www.labyrinth.net.au/~trandor/abuse)  
[Jeremy Scott Windows port (2001)](http://web.archive.org/web/20051023123223/http://www.webpages.uidaho.edu/~scot4875)  
[Sam Hocevar Abuse SDl port (2011)](http://abuse.zoy.org)  
[Xenoveritas SDL2 port (2014)](http://github.com/Xenoveritas/abuse)  
[Antonio Radojkovic Abuse 1996](https://github.com/antrad/Abuse_1996)
[Andrej Pancik Abuse 2025](https://github.com/apancik/Abuse_2025)

## Acknowledgments

Special thanks go to Jonathan Clark, Dave Taylor and the rest of the Crack Dot Com team for making the best 2D platform shooter ever, and then releasing the code that makes Abuse possible.

Also, thanks go to Jonathan Clark for allowing Anthony to distribute the original datafiles with Abuse.

Thanks to everyone who has contributed ideas, bug reports, and patches over the years (see the AUTHORS file for details). This project stands on the shoulders of many developers who kept it alive for three decades - from the original Crack Dot Com team to the various port maintainers like Anthony Kruize, Jeremy Scott, Sam Hocevar, Xenoveritas, Antonio Radojkovic and Andrej Pancik.
