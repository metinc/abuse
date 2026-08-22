# Building Abuse

## Requirements

- CMake 3.21 or newer
- C and C++ compiler
- SDL3 3.4.0 or newer
- SDL3_mixer 3.2.0 or newer
- SDL3_net 3.0.0 or newer
- libdatachannel 0.23 or newer with WebSocket support
- nlohmann-json

## Build

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

## Packages

Requires Docker.

```sh
cmake -S . -B build && cmake --build build --target packages-container
```

The DEB, RPM, TGZ, AppImage, Flatpak, ZIP, and MSI files are written to
`build/packages/`.
