# Abuse README

This is an enhanced version of Abuse based on the original source code that was released to the public domain. See the [Changelog](CHANGELOG.md) for details.

If you have any problems with the game or just want to chat, join my [Discord server](https://discord.gg/T8Kw6kwN). I'll see what I can do to help.

To report a bug, please create a new issue here on GitHub. Pull requests are welcome!

## Configuration

The Abuse configuration file has been updated in this version. It is stored locally in the "user" folder as `config.txt`, along with save game files and other original configuration files (such as gamma settings).

Lines starting with a ';' are comments. Setting an option to '1' turns it on, and '0' turns it off. The following settings can be changed via the config file:

- fullscreen - fullscreen or windowed mode (2 means "real" fullscreen)
- screen_width - game screen width
- screen_height - game screen height
- scale - window scale
- linear_filter - use linear texture filter (default is nearest)
- hires - enable high-resolution menu buttons and game screens (2 enables the Bungie logo)
- big_font - enable big font
- volume_sound - sound volume
- volume_music - music volume
- mono - use mono audio only
- no_music - disable music
- no_sound - disable sound effects
- local_save - save config and other files locally
- grab_mouse - lock the mouse to the window
- editor - enable editor mode
- physics_update - physics update interval in ms
- mouse_scale - scale factor for mouse input based on desktop or game screen size

To change the keys used in the game, simply type the key after the option:

- left - move left
- right - move right
- up - jump or climb a ladder
- down - use the lift, press a switch, or use a save console
- special - use special ability
- fire - fire weapon
- weapon_prev - select previous available weapon
- weapon_next - select next available weapon

The following special keys can also be used:

| Code                          | Represents                   |
| ----------------------------- | ---------------------------- |
| `LEFT`, `RIGHT`, `UP`, `DOWN` | Cursor keys and keypad.      |
| `CTRL_L`, `CTRL_R`            | Left and right Ctrl keys.    |
| `ALT_L`, `ALT_R`              | Left and right Alt keys.     |
| `SHIFT_L`, `SHIFT_R`          | Left and right Shift keys.   |
| `F1` - `F10`                  | Function keys 1 through 10.  |
| `TAB`                         | Tab key.                     |
| `BACKSPACE`                   | Backspace key.               |
| `ENTER`                       | Enter key                    |
| `INSERT`, `DEL`               | Insert and Delete keys.      |
| `PAGEUP`, `PAGEDOWN`          | Page Up and Page Down keys.  |
| `CAPS`, `NUM_LOCK`            | Caps-Lock and Num-Lock keys. |
| `SPACE`                       | Spacebar.                    |

The default key settings are as follows:

| Action      | Bound to           |
| ----------- | ------------------ |
| Left        | Left arrow, A      |
| Right       | Right arrow, D     |
| Up/Jump     | Up arrow, W        |
| Down/Use    | Down arrow, S      |
| Prev Weapon | Left or Right Ctrl |
| Next Weapon | Insert             |

The mouse controls your aim, with Left button for fire and Right button for special.
The mouse wheel can be used for changing weapons.

The game has almost full controller support now, there are several settings you can change in the config file:

- ctr_cd - crosshair distance from player
- ctr_rst_s - right stick/aiming sensitivity
- ctr_rst_dz - right stick/aiming dead zone
- ctr_lst_dzx - left stick left/right movement dead zones
- ctr_lst_dzy - left stick up/down movement dead zones

To bind controller buttons to in game action use the following names for the buttons;

- ctr_a, ctr_b, ctr_x, ctr_y
- ctr_left_shoulder, ctr_right_shoulder
- ctr_left_trigger, ctr_right_trigger
- ctr_left_stick, ctr_right_stick

See "3. Hardcoded keys" for the hardcoded controller bindings.

## 3. Hardcoded keys

There are several keys in the game that are hardcoded to some function originally or were added during porting:

- 1-7 - weapon selection
- right control - previous weapon
- insert - next weapon
- numpad 2, 4, 5, 6, 8 - player movement
- escape, space, enter - reset level on death
- c - cheat/chat console
- p - pause game
- F1 - show help/controls screen
- F5 - quick save on save consoles (1 or "save0001.spe" is the dedicated quick save slot)
- F6 - toggle window input grab
- F7 - toggle mouse scale type
- F8 - toggle controller use
- F9 - quick load
- F10 - toggle window/fullscreen mode
- F11 - scale window/screen up
- F12 - scale window/screen down
- print screen - take a screenshot

Controller defaults:

- D-pad, left stick - move left, right, up, down in game and in menus
- home - show help/controls screen
- back - behaves like the escape key
- start - behaves like enter key

## BuildingTheProject

Abuse has the following requirements:

- SDL2 2.0.3 or above
- SDL_mixer 2.0.0 or above
- GLee for OpenGL rendering
- OpenCV 2.1 for abuse-tool

Read the BUILDING.md file provided by Xenoveritas to see how to build the projects.
Do note this version also uses OpenGL(GLee) and OpenCV, so you will need to link to those libraries too.

## Notes

### Increased difficulty:

The game is designed for a 320x200 resolution. Playing in higher resolutions may reveal secret areas. Enemies are triggered sooner, so you might end up facing more aliens than originally intended. Since a higher resolution shows a larger area and makes the game easier, consider it a "difficulty adjustment" rather than a bug.

### Recommended game screen resolutions:

To scale the game properly on 1920x1080 screens, you can use these settings:

- 320x180 with scale set to 6
- 384x216 with scale set to 5
- 480x270 with scale set to 4
- 640x360 with scale set to 3
- 960x540 with scale set to 2

### Using cheat/chat console:

The mouse cursor must be inside the console window to be able to get input, because windows in the engine are only active then.
Press enter/return when the console is clear, or type "quit" or "exit" to close the console window.

### Quick save/load:

Save slot 1 or "save0001.spe" is the dedicated quick save slot. Quick load always loads the last save game, so if you start a new level or
manually save the game after you used the quick save, it will load that and not the quick save.
Some in game save consoles act as a switch to open doors, so if you just quick save the door will not open, you need to press the down/use key.

## Abuse Tool

Abuse-tool is a low-level tool to edit Abuse SPEC (.spe) files and to extract PSX image files contained in them to modern image formats
as individual images, tilemaps or a texture atlas.

Usage 1: abuse-tool \<spec_file\> \<command\> [args...]

Commands and arguments:

- list - list the contents of a SPEC file
- get \<id\> - dump entry \<id\> to stdout
- getpcx \<id\> - dump PCX image \<id\> to "abuse-tool" folder
- put \<id\> \<type\> \<name\> - insert file \<name\> of type \<type\> at position \<id\>
- putpcx \<id\> \<type\> \<name\> - insert PCX image \<name\> of type \<type\> at position \<id\>
- rename \<id\> \<name\> - rename entry \<id\> to \<name\>
- type \<id\> \<type\> - set entry \<id\> type to \<type\>
- move \<id1\> \<id2\> - move entry \<id1\> to position \<id2\>
- del \<id\> - delete entry \<id\>

Usage 2: abuse-tool

To extract the images to modern image formats the program reads the settings and list of files from "abuse-tool/extract.txt"
when being launched. More info can be found there. Extracted images will be stored at the location of the original files.

## Special Thanks

To everybody who worked on Abuse and its ports; from the people at Crack dot Com who made the original game,
to Xenoveritas and others who kept it alive for 20 years.

## About the game

### The story

"You are Nick Vrenna. It is the year 2009. You have been falsely incarcerated
inside a high security underground prison where illegal genetic experiments
are taking place.

Alan Blake, the head research scientist, has isolated the specific gene which
causes violence and aggression in humans. This genetic sequence, called
"Abuse", is highly infectious, causing horrific transformations and grotesque
side-effects. You are the only person to show immunity to it.

A prison riot erupts, and in the confusion all the cell doors are opened. Soon
everyone, guards and convicts alike, become infected and transform into a
horde of mutants which take over the building.

Your only chance for escape is to don battle armor and reach the Control Room
situated in the structure's deepest level. You must first stop the prison's
Abuse-infected water supply from contaminating the outside world. Freedom and
the fate of the world now depend on you."

### Abuse name

"We chose Abuse for our first game name because the game involved knowing full well that pressing the buttons in all those rooms was going to bring down hoards of howlers, but you would do it anyway, abusing yourself."

## Links

### Info about the game

[Moby Games page](http://www.mobygames.com/game/abuse)  
[Abuse homepage](http://web.archive.org/web/20010517011228/http://abuse2.com)  
[Free Abuse(Frabs) homepage](http://web.archive.org/web/20010124070000/http://www.cs.uidaho.edu/~cass0664/fRaBs)  
[Abuse fan page](http://web.archive.org/web/19970701080256/http://games.3dreview.com/abuse/index.html)

### Downloads

[Abuse Desura download](http://www.desura.com/games/abuse/download)  
[Free Abuse(Frabs) download](http://www.dosgames.com/g_act.php)

### Source code releases

[Original source code](https://archive.org/details/abuse_sourcecode)  
[Anthony Kruize Abuse SDL port (2001)](http://web.archive.org/web/20070205093016/http://www.labyrinth.net.au/~trandor/abuse)  
[Jeremy Scott Windows port (2001)](http://web.archive.org/web/20051023123223/http://www.webpages.uidaho.edu/~scot4875)  
[Sam Hocevar Abuse SDl port (2011)](http://abuse.zoy.org)  
[Xenoveritas SDL2 port (2014)](http://github.com/Xenoveritas/abuse)

### Bonus

[Gameplay video](http://www.youtube.com/watch?v=0Q0SbdDfnFI)  
[HMI to MIDI converter](http://www.ttdpatch.net/midi/games.html)

---

Thank you for playing Abuse!

---

Abuse, Copyright 1995 Crack dot Com
