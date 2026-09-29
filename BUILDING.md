# Building Abuse

## Packages

Requires CMake, Make or Ninja, and Docker or Podman on x86_64 Linux.

```sh
cmake -S packaging/container -B build-container
cmake --build build-container
```

Packages are written to `build-container/packages/`. Docker layers and the
Flatpak cache are reused; missing dependencies require internet access.

### DOSBox package

Build only the DOS singleplayer ZIP with the same container project:

```sh
cmake -S packaging/container -B build-container
cmake --build build-container --target dos-container
```

The result is `build-container/packages/abuse-dos.zip`; it is also included in
`packages-container`. The DJGPP compiler, SDL libraries, and music-conversion
tools run inside Docker. See [the DOSBox instructions](packaging/dos/README.md)
for running the game and build options.

## Local development

Use the root CMake project for local builds, including the VS Code CMake
extension and debugger. This requires the following local dependencies:

### Requirements

- CMake 3.21 or newer
- C and C++ compiler
- SDL3 3.4.0 or newer
- SDL3_mixer 3.2.0 or newer
- SDL3_net 3.0.0 or newer
- libdatachannel 0.23 or newer with WebSocket support
- nlohmann-json

### Build

```sh
git clone https://github.com/metinc/abuse.git
cd abuse
cmake -S . -B build
cmake --build build --parallel
```

Room-code multiplayer uses
`wss://abusecoop.com` by default. Another deployment URL can be compiled
in with `-DABUSE_SIGNALING_URL=wss://play.example.com`; players may also
override it with `-signal-server`.

## Experimental macOS cross-toolchain on Linux

Requires Docker.

```sh
cmake -S . -B build && cmake --build build --target packages-container
```

The Linux DEB, RPM, TGZ, AppImage and Flatpak packages, Windows ZIP and MSI
packages, macOS ZIP packages, and the DOSBox ZIP are written to `build/packages/`.
