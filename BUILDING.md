# Building Abuse

## Prerequists

### All Platforms

- SDL 1.1.6 or higher <http://www.libsdl.org> (note that SDL2 will not work)
- [SDL_mixer 1.2](http://www.libsdl.org/projects/SDL_mixer/release-1.2.html)
- [CMake 2.8.9 or later](http://www.cmake.org/)
- GL libraries and headers are required for OpenGL support.
- OpenCV library for extracting PCX images in SPEC files using abuse-tool

### Windows

- [Visual Studio 2013](http://www.visualstudio.com/downloads/download-visual-studio-vs#d-express-windows-desktop)
  (maybe earlier versions, haven't tried)
- CMake 2.8.11 or later for the WIX installer (tested with 3.0)

### Mac OS X

Mac OS X should have most of the stuff you need already. The easiest method for
getting CMake and SDL/SDL_mixer is probably using [Homebrew](http://brew.sh/).

```sh
brew install cmake
brew install sdl
brew install sdl_mixer
```

# Compiling

Clone this repository.

```sh
git clone https://github.com/metinc/Abuse_1996
```

> **Note:** If you are using Visual Studio Code you can use [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) to build and run the project and skip the following steps.

Inside the repository use CMake to build into the subdirectory `build`.

```sh
cmake -DCMAKE_INSTALL_PREFIX:PATH=out -B build
```

Under Windows, this will probably fail because it can't find the SDL2 or
SDL2_mixer libraries. Two solutions to this:

1. Run `cmake-gui` and provide the paths that way.
2. Set `SDL2DIR` and `SDL2MIXERDIR` to point to where you extracted the
   Windows VC devel binaries for each library.

## Build the files

Under Linux and Mac OS X, this is the familiar `make`.

Under Windows, you'll want to use `MSBuild abuse.sln`. (Alternatively, open
the solution in Visual Studio and build it that way.)

## Install the files

Note that you can skip this step if you're planning on building an installer.
This is simply `make install` or building `INSTALL.vcxproj` under Windows.
(Again, either `MSBuild INSTALL.vcxproj` or just build it directly within
Visual Studio.)

# Installers (Packages)

The CMake package includes some CPack stuff to enable building installers. Under
Windows, this will attempt to create a [WIX](http://wixtoolset.org/) installer
and a ZIP file. Under Mac OS X, it attempts to create a DMG and TGZ.

To build them under Linux and Mac OS X, it's just `make package`.

Under Windows, build `PROJECT.vcxproj`.
