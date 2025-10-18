# Building Abuse

## Prerequisites

- SDL2
- SDL2_mixer
- [CMake 3.16 or later](http://www.cmake.org/)
- GL libraries and headers (e.g., mesa, libgl, or similar) for OpenGL support
- OpenCV library for extracting PCX images in SPEC files using `abuse-tool`

### Linux

On Arch Linux, install these packages:

```sh
sudo pacman -S sdl2 sdl2_mixer opencv cmake dpkg rpm-tools
```

For other distributions, use the equivalent packages from your package manager.

Building in Windows via the command line involves using the Visual Studio developer environment. Visual Studio should have installed a shortcut named "Developer Command Prompt for VS 2019" (or whatever version used - the latest is recommended) - this runs a CMD file that sets the necessary environment variables to use the Visual Studio command line tools. Commands need to be run within this environment for CMake to locate Visual Studio and for the development tools to be available.

CMake and WiX can both be installed individually or via the [Chocolatey package manager](https://chocolatey.org/). Via Chocolatey, the command to install CMake and the WiX toolset is simply:

    choco install cmake wixtoolset

For Windows, the easiest way to get SDL2 and SDL2_mixer installed is via [vcpkg](https://vcpkg.io/en/index.html). Follow the [getting started instructions](https://vcpkg.io/en/getting-started.html). With it installed there should be nothing else to do, the `vcpkg.json` file indicates the required SDL2 and SDL2-mixer dependencies.

With that set up, the CMake generation should succeed without any error.

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

# AppImage

If `appimagetool` is installed on your system, an AppImage will be built automatically. You can download `appimagetool` from the [AppImageKit releases page](https://github.com/AppImage/AppImageKit/releases). Make sure to make it executable and in your PATH.

To build the AppImage (if `appimagetool` is installed):

```sh
make appimage
```

This will create an `Abuse-${PROJECT_VERSION}.AppImage` file in the build directory. You can then run this file to play the game.
