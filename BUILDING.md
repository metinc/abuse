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

Enter the repository and configure the build. All commands below are run from
the repository root:

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

To build them under Linux or macOS, run from the repository root:

```sh
cmake --build build --target package
```

Under Windows you can use [MSYS2 MinGW 64-bit](https://www.msys2.org/) to run the command.

# AppImage

The `appdir` and `appimage` targets use
[linuxdeploy](https://github.com/linuxdeploy/linuxdeploy) to bundle runtime
dependencies and create the AppImage. Download the official linuxdeploy
AppImage for your architecture, make it executable, and either put it in your
`PATH` as `linuxdeploy` or pass its path when configuring:

```sh
cmake -B build -DLINUXDEPLOY=/path/to/linuxdeploy-x86_64.AppImage
```

The `appdir` target creates a clean AppDir from the canonical CMake install
rules and asks linuxdeploy to bundle the executable's runtime dependencies.
FluidSynth is passed explicitly because SDL_mixer loads it dynamically:

```sh
cmake --build build --target appdir
```

To build the AppImage:

```sh
cmake --build build --target appimage
```

linuxdeploy's bundled AppImage output plugin creates
`Abuse-<version>-<architecture>.AppImage` in the build directory. The AppDir is
removed and recreated for each build, so stale libraries and assets cannot
leak into a new image. The targets disable linuxdeploy's stripping pass because
the binutils bundled by some linuxdeploy AppImages cannot process newer ELF
features used by distributions such as Arch Linux.
