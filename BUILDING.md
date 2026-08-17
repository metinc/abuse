# Building Abuse

## Prerequisites

- SDL3
- SDL3_mixer
- SDL3_net
- [CMake 3.21 or later](https://cmake.org/)

### Linux

On Arch Linux, install these packages:

```sh
sudo pacman -S sdl3 sdl3_mixer cmake dpkg rpm-tools
```

SDL3_net is not currently available in the official Arch repositories or the
AUR. Do not install `sdl_net` or `sdl2_net`; those packages provide older,
incompatible APIs. Build and install SDL3_net 3.2.0 from the
[official release](https://github.com/libsdl-org/SDL_net/releases/tag/release-3.2.0):

```sh
curl -LO https://github.com/libsdl-org/SDL_net/releases/download/release-3.2.0/SDL3_net-3.2.0.tar.gz
tar -xf SDL3_net-3.2.0.tar.gz

cmake -S SDL3_net-3.2.0 -B SDL3_net-3.2.0/build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DSDLNET_SAMPLES=OFF

cmake --build SDL3_net-3.2.0/build --parallel
sudo cmake --install SDL3_net-3.2.0/build
```

The Abuse build will find SDL3_net under `/usr/local` automatically.

For other distributions, use the equivalent packages from your package manager.

### macOS

macOS should have many of the necessary tools already. The easiest method for installing CMake and the SDL libraries is using [Homebrew](http://brew.sh/):

```sh
brew install cmake
brew install sdl3
brew install sdl3_mixer
brew install sdl3_net
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

The `abuse-tool` utility is enabled by default and uses its bundled lightweight
image writer. To build only the game, configure with:

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

On Arch Linux, install the cross-build tools and clone the vcpkg ports registry:

```sh
sudo pacman -S --needed mingw-w64-gcc msitools vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/.local/share/vcpkg
cmake -B build -DVCPKG_ROOT="$HOME/.local/share/vcpkg"
cmake --build build --target windows-packages
```

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

# Flatpak

Build the Flatpak bundle:

```sh
cmake --build build --target flatpak
```
