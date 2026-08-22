# syntax=docker/dockerfile:1

# Debian 12 intentionally supplies the oldest userspace ABI supported by the
# published AppImage. Do not replace this with a rolling distribution.
FROM debian:12-slim AS builder

ARG SDL_VERSION=3.4.0
ARG SDL_MIXER_VERSION=3.2.4
ARG SDL_NET_VERSION=3.2.0
ARG SDL_SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8
ARG SDL_MIXER_SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
ARG SDL_NET_SHA256=098522fc26d4e302ef9348aee6e76e67fe504dfefd7f596236568f8330570c41

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --yes --no-install-recommends \
        binutils \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        dpkg-dev \
        file \
        git \
        libasound2-dev \
        libdbus-1-dev \
        libegl-dev \
        libfluidsynth-dev \
        libgl-dev \
        libpulse-dev \
        libudev-dev \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxrandr-dev \
        libxss-dev \
        libxtst-dev \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp/sdl
RUN curl --location --fail --retry 3 \
        --output SDL3.tar.gz \
        "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz" \
    && echo "${SDL_SHA256}  SDL3.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3.tar.gz \
    && cmake -S "SDL3-${SDL_VERSION}" -B build-sdl -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/abuse \
        -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_JACK=OFF \
        -DSDL_KMSDRM=OFF \
        -DSDL_PIPEWIRE=OFF \
        -DSDL_SNDIO=OFF \
        -DSDL_STATIC=OFF \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -DSDL_WAYLAND=OFF \
    && cmake --build build-sdl --parallel \
    && cmake --install build-sdl

RUN curl --location --fail --retry 3 \
        --output SDL3_mixer.tar.gz \
        "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL_MIXER_VERSION}/SDL3_mixer-${SDL_MIXER_VERSION}.tar.gz" \
    && echo "${SDL_MIXER_SHA256}  SDL3_mixer.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3_mixer.tar.gz \
    && cmake -S "SDL3_mixer-${SDL_MIXER_VERSION}" -B build-sdl-mixer -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/abuse \
        -DCMAKE_PREFIX_PATH=/opt/abuse \
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
        -DSDLMIXER_WAVPACK=OFF \
    && cmake --build build-sdl-mixer --parallel \
    && cmake --install build-sdl-mixer

RUN curl --location --fail --retry 3 \
        --output SDL3_net.tar.gz \
        "https://github.com/libsdl-org/SDL_net/releases/download/release-${SDL_NET_VERSION}/SDL3_net-${SDL_NET_VERSION}.tar.gz" \
    && echo "${SDL_NET_SHA256}  SDL3_net.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3_net.tar.gz \
    && cmake -S "SDL3_net-${SDL_NET_VERSION}" -B build-sdl-net -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/abuse \
        -DCMAKE_PREFIX_PATH=/opt/abuse \
        -DBUILD_SHARED_LIBS=ON \
        -DSDLNET_SAMPLES=OFF \
    && cmake --build build-sdl-net --parallel \
    && cmake --install build-sdl-net

RUN curl --location --fail --retry 3 \
        --output /usr/local/bin/linuxdeploy \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage \
    && curl --location --fail --retry 3 \
        --output /usr/local/bin/linuxdeploy-plugin-appimage \
        https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage \
    && chmod +x /usr/local/bin/linuxdeploy /usr/local/bin/linuxdeploy-plugin-appimage

ENV LD_LIBRARY_PATH=/opt/abuse/lib
WORKDIR /src
COPY . .
RUN mkdir /output \
    && cmake -S . -B /build-deb -G Ninja \
        -DABUSE_BUNDLE_SDL_RUNTIME=ON \
        -DABUSE_SDL3_LICENSE_FILE=/tmp/sdl/SDL3-${SDL_VERSION}/LICENSE.txt \
        -DABUSE_SDL3_MIXER_LICENSE_FILE=/tmp/sdl/SDL3_mixer-${SDL_MIXER_VERSION}/LICENSE.txt \
        -DABUSE_SDL3_NET_LICENSE_FILE=/tmp/sdl/SDL3_net-${SDL_NET_VERSION}/LICENSE.txt \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/opt/abuse \
    && cmake --build /build-deb --parallel \
    && cpack --config /build-deb/CPackConfig.cmake -G DEB -B /output \
    && cmake -S . -B /build-appimage -G Ninja \
        -DABUSE_APPIMAGE_GLIBC_BASELINE=2.36 \
        -DABUSE_INSTALL_BINDIR=bin \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/opt/abuse \
        -DLINUXDEPLOY=/usr/local/bin/linuxdeploy \
    && cmake --build /build-appimage --target appimage --parallel \
    && cp /build-appimage/Abuse-*.AppImage /output/ \
    && cmake -E remove_directory /output/_CPack_Packages

FROM debian:12-slim AS packages-artifact
COPY --from=builder /output/ /artifact/
ENTRYPOINT ["cp", "-R", "--preserve=mode,timestamps", "/artifact/.", "/output/"]

FROM debian:12-slim AS appimage-artifact
COPY --from=builder /output/*.AppImage /artifact/
ENTRYPOINT ["cp", "-R", "--preserve=mode,timestamps", "/artifact/.", "/output/"]
