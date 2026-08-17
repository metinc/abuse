# Abuse

![hero](https://github.com/user-attachments/assets/143352b6-dfe5-474f-926d-dd7f74548a85)

This is an enhanced version of Abuse based on the original source code that was released to the public domain. See the [Changelog](CHANGELOG.md) for details.

If you have any problems with the game or just want to chat, join my [Discord server](https://discord.gg/Zhjqf7EHWN). I'll see what I can do to help.

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

Configuration is stored in `settings.toml` in the user folder. See [`data/user/settings.toml`](data/user/settings.toml) for a complete example.

### Settings File Options

#### Display Settings

- `fullscreen` - Enable borderless desktop fullscreen
- `borderless` - Remove window decorations in windowed mode
- `widescreen_support` - Expand the framebuffer to match the desktop aspect ratio; when disabled, use the configured framebuffer size exactly
- `framebuffer_width` - Internal game framebuffer width when widescreen support is disabled (minimum `320`)
- `framebuffer_height` - Internal game framebuffer height when widescreen support is disabled (minimum `200`)
- `editor_framebuffer_width` - Editor baseline framebuffer width (default `640`, minimum `320`)
- `editor_framebuffer_height` - Editor baseline framebuffer height (default `400`, minimum `200`)
- `window_scale` - Integer window scale used in windowed mode
- `linear_filter` - Use linear texture filter (nearest is default)
- `hires` - Enable high resolution menu and screens (`2` for Bungie logo)
- `big_font` - Enable big font
- `gamma` - Display gamma (`0.5`-`2.0`; `1.0` is neutral)

The game is designed for a 320×200 framebuffer displayed with the original VGA pixel aspect ratio. With widescreen support enabled, one framebuffer dimension expands to match the desktop without changing the baseline gameplay zoom. For example, a 16:9 desktop uses a 427×200 framebuffer. With widescreen support disabled, `framebuffer_width` and `framebuffer_height` are used exactly.

The editor uses `editor_framebuffer_width` and `editor_framebuffer_height` as its independent zoom baseline. With widescreen support enabled, one dimension expands to match the desktop; for example, a `640×400` baseline becomes `853×400` on a 16:9 desktop. With widescreen support disabled, the editor dimensions are used exactly. Entering or leaving the editor resizes the existing display in place.

#### Audio Settings

- `sound_volume` - Sound effects gain (`0.0`-`1.0`)
- `music_volume` - Music gain (`0.0`-`1.0`)
- `mono` - Use mono audio only
- `music_enabled` - Enable music
- `sound_enabled` - Enable sound effects
- `soundfont` - SoundFont filename from `data/soundfonts`, or an absolute path to a custom `.sf2`/`.sf3` file

The in-game Audio Settings window lists available SoundFonts from `data/soundfonts` and applies a selection without
restarting the game. Changes apply as soon as a SoundFont is selected while preserving the current music position.

The build converts every HMI song in `data/music` to Standard MIDI. Only the generated `.mid` files are installed and
packaged; HMI conversion is not part of the game's runtime audio path. The C++ converter is also available manually as
`abuse-tool hmi2mid <input.hmi> <output.mid>`.

#### Game Settings

- `[gameplay].difficulty` - Difficulty (`"easy"`, `"medium"`, `"hard"`, or `"extreme"`)
- `[gameplay].physics_tick_ms` - Physics update time in ms (65ms/15FPS original)
- `[gameplay].max_fps` - Frame-rate limit
- `[general].grab_input` - Confine the mouse to the rendered game area in windowed mode
- `[general].language` - Game language (`"english"`, `"german"`, or `"french"`)

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

Keyboard bindings are arrays, allowing two keys for movement, for example `left = ["a", "LEFT"]`. Special key names include:

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
- <kbd>F8</kbd> - Toggle gamepad use
- <kbd>F9</kbd> - Quick load
- <kbd>F10</kbd> - Toggle window/fullscreen mode
- <kbd>Print Screen</kbd> - Take a screenshot

Default Controller Bindings:

- D-pad, left stick - Move in all directions
- Right stick - Aim
- South face button - Jump; confirm in menus
- East face button - Down/use; cancel in menus
- West face button - Use the active special ability
- Left/right shoulder - Previous/next weapon
- Left/right trigger - Special ability/fire
- Guide/Home - Show help/controls screen
- Back - Acts as <kbd>Escape</kbd> key
- Start - Acts as <kbd>Enter</kbd> key

### Gamepad Support

Options in `[input.gamepad]` include:

- `enabled` - Accept gamepad input; <kbd>F8</kbd> toggles and saves this setting
- `aim_invert_y` - Invert the right stick's vertical aiming axis
- `aim_correction_x` - Horizontal crosshair correction
- `crosshair_distance` - Crosshair distance from player
- `aim_sensitivity` - Right stick/aiming sensitivity (1-100)
- `aim_dead_zone` - Radial right stick/aiming dead zone (1-32766)
- `move_dead_zone_x` - Left stick horizontal dead zone
- `move_dead_zone_y` - Left stick vertical dead zone
- `trigger_threshold` - Trigger activation threshold (1-32767)
- `trigger_hysteresis` - Required trigger release movement below its activation threshold
- `menu_confirm`, `menu_cancel` - Buttons used to confirm and cancel in the main menu
- `quick_save`, `quick_load` - Optional buttons for quick save and quick load

Button binding names:

- `south`, `east`, `west`, `north` - Face buttons
- `left_shoulder`, `right_shoulder` - Shoulder buttons
- `left_trigger`, `right_trigger` - Triggers
- `left_stick`, `right_stick` - Stick clicks
- `dpad_up`, `dpad_down`, `dpad_left`, `dpad_right` - D-pad directions
- `start`, `back`, `guide` - System/menu buttons

Each configurable button or trigger maps to `"up"`, `"down"`, `"left"`, `"right"`, `"special"`, `"fire"`,
`"weapon_prev"`, `"weapon_next"`, `"confirm"`, `"cancel"`, `"help"`, or `"none"`.
The first gamepad used becomes active; other connected gamepads are ignored until the active one disconnects.

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
| `-fwin`         | Open foreground editor      |
| `-bwin`         | Open background editor      |
| `-owin`         | Open objects window         |
| `-no_autolight` | Disable auto lighting       |
| `-nolight`      | Disable all lighting        |
| `-bastard`      | Bypass filename security    |
| `-size`         | Custom framebuffer size     |
| `-lisp`         | Start LISP interpreter      |
| `-ec`           | Empty cache                 |
| `-t <filename>` | Insert tiles from file      |
| `-cprint`       | Enable console printing     |

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
