# Abuse for DOSBox

This build runs the singleplayer game in DOSBox. It includes the game data,
Sound Blaster effects, live OPL FM music, and the CWSDPMI memory extender.
LAN and online multiplayer are disabled at build time.

## Play

Extract `abuse-dos.zip` into a directory. From that directory, run:

```sh
dosbox -conf dosbox.cfg
```

The supplied configuration uses an S3-compatible VGA/VESA adapter, a dynamic
CPU core, maximum cycles, 63 MB of RAM, and Sound Blaster 16. These are DOSBox
settings, not minimum requirements for a physical DOS PC. Real hardware has
not been tested.

The DOS renderer uses an indexed 8-bit VGA/VESA framebuffer, normally 320x200.
DOSBox handles scaling and 4:3 aspect correction (`aspect=true`). The game's
`window_scale` setting only applies to desktop builds. This avoids converting
and scaling every frame to true color on the emulated CPU. The DOS renderer
also yields to SDL's cooperative audio thread before and after presenting.

Keyboard and mouse controls match the desktop game: WASD/arrows to move,
mouse to aim, left mouse button to fire, Escape for the menu.
Startup can take a while under emulation, especially while the first lighting
cache is generated. Screenshots use the filename `SCREEN.BMP`.

Saves and configuration live in `SAVE` beside `ABUSE.EXE`. `SAVE/ABUSE.CFG`
uses TOML syntax, with a short filename for DOS. The packaged `USER/ABUSE.CFG`
is the template for new configurations.

Music uses the same small MIDI files as the desktop build. libADLMIDI sequences
them through the Sound Blaster's OPL chip; DOSBox synthesizes the FM audio.
Sound effects continue through SDL_mixer and Sound Blaster digital audio.
No music WAVs, external MIDI synthesizer, or SoundFonts are needed. The supplied
DOSBox configuration selects OPL3; music volume and looping work in the game.

The original DOS Abuse used HMI's music driver with separate melodic and drum
banks. Those original bank files are absent from this repository. This build
uses libADLMIDI's HMI (Descent, Asterix) bank, based on the Fat Man 2-op
instruments. It follows the original FM playback approach; instrument timbres
may differ from the original Abuse banks. Song changes stop the previous OPL
voices before loading the next song; the hardware has one shared set of voices.

Sources: [original DOS sound driver](https://github.com/videogamepreservation/abuse/blob/master/imlib/port/dos4gw/sound.c),
[libADLMIDI](https://github.com/Wohlstand/libADLMIDI).

To show the measured frame rate, set `show_fps = true` in the existing `[video]`
section of `SAVE/ABUSE.CFG`. The first number in the level view is FPS; the
second is the active object count. The default `[gameplay] max_fps = 30` caps
rendering; it does not change the physics tick rate.

All registered sound effects are decoded during the first level load and kept
in the cache across level changes. This avoids loading pauses on the first ammo
pickup, enemy sound, or other effect. In the test package, 99 effects occupy
about 1.4 MB of decoded audio, plus mixer overhead. Reading each effect directly
and decoding it from memory avoids costly DOS file-status checks; initial cache
loading takes about 0.4 seconds in the test scene, down from about eight seconds.
General Settings applies skin changes
to the preview immediately and saves the configuration once when the window closes.

## Build with Docker

Requires CMake 3.21+, Make or Ninja, and Docker or Podman on x86_64 Linux.
Run from the source checkout:

```sh
cmake -S packaging/container -B build-container
cmake --build build-container --target dos-container
```

The result is `build-container/packages/abuse-dos.zip`. Extract it into a game
directory and run `dosbox -conf dosbox.cfg` there. The regular
`packages-container` target also includes the DOS ZIP alongside the other
platform packages.

If you already configured the native development project, you can instead run
`cmake --build build --target dos-container`; its ZIP goes into `build/packages/`.

The Debian 12 build container installs all compilation and music-conversion
tools. It runs `packaging/dos/build.sh`, which downloads SHA-256-checked versions
of DJGPP GCC 12.2, SDL3, SDL_mixer, libADLMIDI, toml11, and CWSDPMI. The compiler and SDL
libraries are built in a separate Docker layer and reused when game sources
change. The first build needs internet access; the final ZIP export runs with
networking disabled and writes files with the host user's UID/GID.

To limit build parallelism, configure with `-DABUSE_DOS_BUILD_JOBS=4`.
HMI-to-MIDI conversion runs with the native tool inside the container. libADLMIDI
is built for direct DOS hardware output, with no OPL emulator running inside the
game and no PCM rendering during the build. The DOS executable links neither
FluidSynth nor network libraries. DOSBox is only needed to run and test the game.

The build applies `toml11-djgpp.patch` to its private toml11 source copy:
DJGPP's `int32_t` is a `long`, and its C++ library lacks `std::copysign`.
SDL_mixer is compiled with `SDL_DISABLE_SSE` for DOSBox's emulated CPU.

The ZIP is assembled from the CMake install manifest, checks every filename
for DOS 8.3 compatibility, and excludes local saves and test logs. Existing
local builds and their `SAVE` directories are not mounted into the container.

## Verification

Tested with DOSBox 0.74-3 on Linux using the settings above:

- Intro, singleplayer menu, and the first campaign level
- Movement, aiming, firing, and non-silent Sound Blaster audio output
- Ladder climbing and entry/exit animations use the selected upper-body skin
  for their single full-body sprite, including when the lower-body skin differs
- Switching between the game and editor resolutions and back
- A save created through the game's Lisp `save_game` function, loaded again,
  then reloaded with F9 after moving the player

On the same test host and DOSBox configuration, the first level improved from
14 FPS with the true-color software renderer to the configured 30 FPS limit
with indexed output. This is a measured comparison, not a minimum frame-rate
guarantee for every level or host.

With `max_fps = 300`, the stationary first-level scene improved from about
85 to 183 FPS after specializing white-light interpolation. Lighting time fell
from 9.8 to 3.9 ms per frame. The optimized path preserves the existing rounding;
solid lights and doubled output continue to use the general path.

A separate first-level test reproduced a drop from about 157 FPS while idle to
45 FPS during sustained fire from the starting weapon. Any visible colored
laser previously selected the slower color-lighting path for the entire view.
The optimized path limits RGB interpolation and tinting to affected grid cells,
using the white-light loop elsewhere. Sustained fire now reaches about 149 FPS
on that test host, versus 155 FPS while idle; lighting takes 4.9 rather than
19.7 ms per frame. A comparison of 4,800 renders found identical pixel output,
including colored line lights, overlapping colors, all lighting detail levels,
clipping, and doubled output.

The native Linux builds also compile with multiplayer enabled and disabled.
The full campaign, editor tools, and physical DOS hardware have not been tested.
