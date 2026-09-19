#!/bin/sh
set -eu
arch="$1"
triplet="$2"
prefix="/opt/abuse/${arch}"
deps="/opt/vcpkg-installed/${arch}/${triplet}"
export PKG_CONFIG_LIBDIR="${deps}/lib/pkgconfig"

/osxcross/bin/"${arch}"-apple-darwin23.6-clang \
    -O2 -fPIC -fvisibility=hidden -c /tmp/compiler-rt/os_version_check.c \
    -o "/tmp/compiler-rt/availability-${arch}.o"

cmake -S "SDL3-${SDL_VERSION}" -B "build-sdl-${arch}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SHARED_LINKER_FLAGS="/tmp/compiler-rt/availability-${arch}.o" \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/macos/osxcross.cmake \
        -DCMAKE_OSX_ARCHITECTURES="${arch}" \
        -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_FRAMEWORK=OFF \
        -DSDL_STATIC=OFF \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF
cmake --build "build-sdl-${arch}" --parallel
cmake --install "build-sdl-${arch}"

cmake -S "SDL3_mixer-${SDL_MIXER_VERSION}" -B "build-sdl-mixer-${arch}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/macos/osxcross.cmake \
        -DCMAKE_OSX_ARCHITECTURES="${arch}" \
        -DCMAKE_PREFIX_PATH="${prefix};${deps}" \
        -DBUILD_SHARED_LIBS=ON \
        -DSDLMIXER_EXAMPLES=OFF \
        -DSDLMIXER_FLAC=OFF \
        -DSDLMIXER_GME=OFF \
        -DSDLMIXER_INSTALL=ON \
        -DSDLMIXER_MIDI=ON \
        -DSDLMIXER_MIDI_FLUIDSYNTH=ON \
        -DSDLMIXER_MIDI_FLUIDSYNTH_SHARED=OFF \
        -DSDLMIXER_MIDI_TIMIDITY=ON \
        -DSDLMIXER_MOD=OFF \
        -DSDLMIXER_MP3=OFF \
        -DSDLMIXER_OPUS=OFF \
        -DSDLMIXER_STRICT=ON \
        -DSDLMIXER_TESTS=OFF \
        -DSDLMIXER_VORBIS_STB=OFF \
        -DSDLMIXER_VORBIS_VORBISFILE=OFF \
        -DSDLMIXER_WAVPACK=OFF
cmake --build "build-sdl-mixer-${arch}" --parallel
cmake --install "build-sdl-mixer-${arch}"

cmake -S "SDL3_net-${SDL_NET_VERSION}" -B "build-sdl-net-${arch}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${prefix}" \
        -DCMAKE_TOOLCHAIN_FILE=/opt/macos/osxcross.cmake \
        -DCMAKE_OSX_ARCHITECTURES="${arch}" \
        -DCMAKE_PREFIX_PATH="${prefix};${deps}" \
        -DBUILD_SHARED_LIBS=ON \
        -DSDLNET_SAMPLES=OFF
cmake --build "build-sdl-net-${arch}" --parallel
cmake --install "build-sdl-net-${arch}"
