# Changelog

## Abuse 1.0.2 by Metin Çelik (2025-03-15)

- Improved performance of light calculation
- Fixed VRAM leak by rewriting the renderer
- Fixed weapon switching using the mouse wheel

## Abuse 1.0.1 by Metin Çelik (2025-03-09)

- Corrected the behavior of the icon buttons in the main menu.
- Increased the brightness of the icon buttons in the main menu.
- Made camera movement smoother when climbing ladders.
- Fixed music not working on Linux if FluidSynth was not installed.
- Fixed crashing when hitting ESC in the main menu before a level was loaded.

## Abuse 1.0.0 by Metin Çelik (2025-03-02)

- **FPS Unlocked**: The game is no longer locked to 15 FPS. Frame rate is now interpolated to match your screen refresh rate.
- **Adaptive VSync**: Added support for FreeSync and G-Sync to eliminate screen tearing.
- **Music**: Resolved the issue of music not playing by including a SoundFont.
- **Controller Support**: Automatically detects controllers without requiring additional setup.
- **No Automatic Weapon Switching**: You now retain your current weapon when picking up a new one.
- **No 'Are you sure?' Prompt on Quit**: Removed the confirmation prompt for quicker exiting.
- **Menu & ESC Key**: Pressing ESC from the main menu now returns you to the game instead of quitting.
- **AppImage Support on Linux**: Added AppImage support, plus `.deb` and `.rpm` packages for easy installation.
- Fixed typos and corrected character encoding in German and French text.
- Addressed minor bugs that caused occasional crashes or slowdowns.

## Abuse 2025 by Andrej Pancik (2025-01-07)

### Multiplayer

- player tints
- reenable networking GUI
- name saving/loading

### Rendering

- sharp scaling
- high DPI screens support
- widescreen support
- window resizing
- remove border from rendering

### Others

- file handling refactor
- MIDI soundfonts
- compiles on Mac and Ubuntu
- new macos icon
- enable translations through config

## Abuse 0.9a by Antonio Radojkovic (2016)

### Abuse

- enabled custom screen size and resolution
  - light enabled at high resolutions
- local config file
  - screen width, height, scale, borderless window, grab input
  - editor mode
  - controller aiming, right stick sensitivity, dead zone and crosshair distance from the player
  - physics update time in ms
- unlocked framerate for rendering, physics locked at custom value
- re-enabled OpenGL rendering, enabled vsync
- fullscreen scaling and "fit screen" scaling using F11 and F12
- merged flags_struct and keys_struct into one settings class
- Xbox controller support:
  - fixed aiming using the right stick
  - fixed player movement using left stick and dpad
  - calculating and updating the crosshair position when aiming with a controller
  - rebindable controller buttons, saving and loading of button bindings via config file
  - navigating the main menu and save/load screens using dpad or left stick
  - toggle controller use using F8
- toggle mouse scale type using F7
- fixed level music not being played correctly when level was loaded
- quick load using F9, quick save using F5 on save consoles
- fixed gamma setting not being saved
- added cheats via chat console: god, giveall, flypower, sneakypower, fastpower, healthpower
- enabled some high resolution images from the 1997 Mac OS release
- toggle window input grab using F6
- turned off that image that would randomly flicker in the bottom-left corner
- fixed health power object image
- turned on, what seems to be, the original big font and aligned GUI objects for it
- set the small font to always be used for save game thumbnails
- fixed mouse image when choosing initial gamma

### Abuse Tool

- extracting PCX images in SPEC files to modern image formats using OpenCV
  - extracting as individual files
  - extracting as tilemaps
  - extracting animations as individual files
  - extracting as a texture atlas using texture packing algorithm
  - handling transparency in different types of images
  - padding between images
  - outline around groups in a texture atlas
  - external color palettes(tints)
  - tilemap palettes for tile positions
  - extraction settings and list of files via "../abuse-tool/extract.txt"
  - printing to console trough a log, log file saved at "../abuse-tool/log.txt"
  - list of output texture files, image positions, sizes... saved at "../abuse-tool/texture_info.txt"
  - created a list of all available SPEC files containing images and added settings for individual SPEC files

## Abuse-XV 0.9 by Daniel "Xenoveritas" Potter (unreleased)

- Xenoveritas's fork of the SDL version
- Change to CMake for easier cross-platform build support
- Builds under Windows now
- Move from SDL to SDL2:
  - Fullscreen is now the default using SDL2's windowed fullscreen
    (Currently there is no method to do "real" fullscreen)
  - OpenGL support is punted to SDL2 instead of being handled here
  - Which also gets DirectX support plus whatever other backends SDL2 can offer

## Abuse 0.8 (2011-05-09)

- Support for the original music packs!
- The fRaBs and main Abuse artwork were merged. Due to name conflicts in the
  level files, fRaBs save games will no longer work.
- 15 save slots instead of 5.
- The Lisp interpreter was heavily improved and now uses its garbage collected
  allocator instead of malloc.
- A new abuse-tool binary helps editing Abuse data files.

## Abuse-SDL 0.7.1 (2008-03-02)

- Countless bugfixes (memory leaks, alignment issues, 64-bit portability, buffer
  overflows, uninitialised data).
- New webpage.

## Abuse-SDL 0.7.0 (2002-12-15)

- Added OpenGL support. (Joris Beugnies)
- Fixed to compile on MacOSX. (Ben Hines, Julian Mayer)
- Fixed window manager "Quit" events being ignored.
- Can specify the location of the datafiles in the abuserc now.
- Updated documentation regarding installation of the datafiles.

## Abuse-SDL 0.6.1 (2002-02-04)

- Fixed video blit routine, giving a large speedup. (Bob Ippolito)
- Fixed to compile with gcc 3.0.x.
- Fixed to compile on Sparc systems.(Arto Jantunen)

## Abuse-SDL 0.6.0 (2002-01-19)

- Rebuilt the make and configure scripts, including patches.(nix)
- gamma.lsp now stored in the ~/.abuse directory.
- Sound is disabled if no sound files are available.
- Tweaked timing routines.
- Improved mouse handling, should be more responsive on slower systems.
- The binary can now be in a different location to the datafiles. (nix)
- Fixed -fullscreen command-line option.
- Fixed pressing both mouse buttons being ignored if "Emulate3Buttons" is
  enabled.
- Fixed the size of the volume settings dialog.
- Fixed to compile on BigEndian systems.(Arto Jantunen)
- Added scaling.
- Added a man page.

## Abuse-SDL 0.5.0 (2001-08-14)

- More 100% CPU usage fixes.(Ed Snible)
- More temp files (light.tbl, end.mem) are now stored in the ~/.abuse/
  directory.
- Fixed compile problems on Solaris.
- Fixed slight graphics corruption at the bottom of the main menu screen.
- Added the ability to customise the keys used in the game.
- Updated documentation.

## Abuse-SDL 0.4.8 (2001-04-29)

- Stopped Abuse using 100% CPU time in certain places.(Ed Snible)
- Fixed problem with saving games on some systems.
- Added missing documentation about the abuserc file.
- Configure script now handles the X libraries not being in ld's default library
  path.(Arto Jantunen)
- Fixed compile problem when SDL isn't installed in a /SDL directory in the
  include path.
- Fixed the -window_scale option to set the size of the window properly.
  Performance when using this option is still bad.
- Can now use the mouse to skip the intro story.

## Abuse-SDL 0.4.7 (2001-03-09)

- Temp files (fastload.dat, level backups etc) are now stored in the ~/.abuse/
  directory.
- SDL include and library locations are now gathered from sdl-config.
- Fixed a crash on some systems when saving games.
  - Those who were getting this crash may not be able to save games.
  - Abuse-SDL will print out what it was trying to do in this case.
- Improved audio mixing when lots of sounds are playing.
- Toggling Grab Mouse now informs what state it is in.
- The numpad can now be used for movement aswell as the cursor keys.
- Settings can be stored in an rc file (~/.abuse/abuserc).

## Abuse-SDL 0.4.6 (2001-02-15)

- Don't even check for the existence of a sound file if sound is disabled.
- Added some checks to the video code to handle failure gracefully.
- Fixes to compile under gcc 2.96.
- Fixed the graphic glitches relating to the gamma window and the save game
  dialog.

## Abuse-SDL 0.4.5 (2001-02-06)

- Stopped loading and trying to play sound files when sound has been disabled.

## Abuse-SDL 0.4.4 (2001-02-05)

- Adjusted the make and configure scripts.
  - Can now specify the install location with the `--with-abuse-dir` flag to
    `configure`. Defaults to `/usr/local/bin` if not specified.
- Added grab-mouse support. F12 toggles this on and off.
- Added screenshot support. PRNT-SCRN key takes a screenshot.
- Can now toggle fullscreen during the game. F11 toggles this on and off.
- Added mousewheel support for changing weapons.
- Savegames are now stored under ~/.abuse/
- Window now has it's own icon(abuse.bmp). Looks nicer in tasklists etc.
- Updated the AUTHORS file as it was a little out of date.

## Abuse-SDL 0.4.3 (2001-01-21)

- Fixed a problem with initialising the mouse driver.
- Reenabled net code. Doesn't work very well yet.

## Abuse-SDL 0.4.2 (2001-01-20)

- Fixed to compile under gcc 2.95.2
- More code tidying.

## Abuse-SDL 0.4.1 (2001-01-13)

- Rebuilt the make and configure scripts.
- Added an icon. (abuse.png)
- Code tidying.
- Improved key and mouse handling.

## Abuse-SDL 0.4 (2000-12-30)

- First release with source available
- Fixed a _huge_ memory leak caused by the stereo sound stuff. (oops)
- Fixed a crash if SDL failed to initialise.
- Tidied up the code, makefiles and configure scripts.

## Abuse-SDL 0.3 (2000-12-24)

- Fixed palette changing when running at 8bpp.
- Fixed draw problem when a window was moved off the screen.
- Fixed sound getting choppy and eventually disappearing.
- Fixed an extremely rare crash when playing sounds.
- Added stereo sound and panning. (-mono to disable)
- Removed some stray debug messages that I accidently left in.

## Abuse-SDL 0.2 (2000-12-13)

- Removed dependancy on lnx_sndsrv.
  All sound is now handled through the SDL API.
- Added fullscreen and doublebuffer modes.

## Abuse-SDL 0.1 (2000-11-10)

- First public release.
