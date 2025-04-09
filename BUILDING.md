# Building Abuse

## Prerequisites

- SDL3
- SDL2_mixer
- [CMake 3.16 or later](http://www.cmake.org/)
- GL libraries and headers (e.g., mesa, libgl, or similar) for OpenGL support
- OpenCV library for extracting PCX images in SPEC files using `abuse-tool`

### Linux

On Arch Linux, install these packages:

```sh
sudo pacman -S sdl3 sdl2_mixer opencv cmake dpkg rpm-tools
```

For other distributions, use the equivalent packages from your package manager.

### macOS

macOS should have many of the necessary tools already. The easiest method for installing CMake, SDL, and SDL_mixer is using [Homebrew](http://brew.sh/):

```sh
brew install cmake
brew install sdl
brew install sdl_mixer
```

# Compiling

Clone this repository.

```sh
git clone https://github.com/metinc/abuse
```

Enter the repository and configure the build:

```sh
cmake -B build
```

Next, build and install:

```sh
sudo cmake --build ./build --target install
```

(If you prefer a local installation, omit sudo and specify a custom prefix:
`cmake -B build -DCMAKE_INSTALL_PREFIX=$HOME/.local`)

Once installed, you can run the game:

```sh
abuse
```

> **Note:** If you’re using Visual Studio Code, you can use [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) to build and run the project more conveniently.

# Installers (Packages)

The CMake setup includes some CPack configurations to enable building installers.
Under Windows, it will create a [WIX](http://wixtoolset.org/) installer and a .zip file.
Under Linux, it will create .deb, .rpm, and .tar.gz packages.
Under macOS, it will create .dmg and .tgz files.

To build them under Linux or macOS, run:

```sh
make package
```

from inside the `build` folder (or `ninja package` if you're using the Ninja generator).

Under Windows you can use [MSYS2 MinGW 64-bit](https://www.msys2.org/) to run the command.

# AppImage

If `appimagetool` is installed on your system, an AppImage will be built automatically. You can download `appimagetool` from the [AppImageKit releases page](https://github.com/AppImage/AppImageKit/releases). Make sure to make it executable and in your PATH.

To build the AppImage (if `appimagetool` is installed):

```sh
make appimage
```

This will create an `Abuse-${PROJECT_VERSION}.AppImage` file in the build directory. You can then run this file to play the game.
