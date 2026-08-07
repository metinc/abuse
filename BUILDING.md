# Building Abuse

## Prerequisites

- SDL3
- SDL3_mixer
- [CMake 3.21 or later](https://cmake.org/)
- OpenCV (only when building the optional `abuse-tool` asset extraction utility)

### Linux

On Arch Linux, install these packages:

```sh
sudo pacman -S sdl3 sdl3_mixer cmake dpkg rpm-tools
```

Also install `opencv` if you want to build `abuse-tool`.

For other distributions, use the equivalent packages from your package manager.

### macOS

macOS should have many of the necessary tools already. The easiest method for installing CMake, SDL, and SDL_mixer is using [Homebrew](http://brew.sh/):

```sh
brew install cmake
brew install sdl3
brew install sdl3_mixer
brew install opencv # Only needed for abuse-tool
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

The `abuse-tool` utility is enabled by default. To build only the game and
avoid the OpenCV dependency, configure with:

```sh
cmake -B build -DABUSE_BUILD_TOOLS=OFF
```

Build and run directly from the build tree:

```sh
cmake --build build
./build/src/abuse
```

The uninstalled executable automatically uses the repository's `data`
directory. Installing is only required when testing an installed layout.

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

The `appdir` target always creates a clean AppDir from the same canonical
CMake install rules used by normal installs and CPack packages:

```sh
cmake --build build --target appdir
```

If `appimagetool` is installed, CMake also provides an `appimage` target.
You can download `appimagetool` from the [AppImageKit releases
page](https://github.com/AppImage/AppImageKit/releases). Make sure it is
executable and in your `PATH`.

To build the AppImage:

```sh
cmake --build build --target appimage
```

This creates `Abuse-<version>-<architecture>.AppImage` in the build directory.
The AppDir is removed and recreated for each build, so stale libraries and
assets cannot leak into a new image.
