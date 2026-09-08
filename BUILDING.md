# Building Abuse

## Packages

Requires CMake, Make or Ninja, and Docker or Podman on x86_64 Linux.

```sh
cmake -S packaging/container -B build-container
cmake --build build-container
```

Packages are written to `build-container/packages/`. Docker layers and the
Flatpak cache are reused; missing dependencies require internet access.

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

The local Docker toolchain setup and Apple SDK import are documented in
[`packaging/macos/README.md`](packaging/macos/README.md). This currently
prepares the compiler; the full Abuse cross-build and `.app` packaging are
not yet integrated.
