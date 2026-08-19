# syntax=docker/dockerfile:1

# Fedora is used for the native RPM and the TGZ archive.
# Keep the release pinned so generated RPM requirements remain reproducible.
FROM fedora:44 AS builder

ARG SDL_VERSION=3.4.0
ARG SDL_MIXER_VERSION=3.2.4
ARG SDL_NET_VERSION=3.2.0
ARG VCPKG_TAG=2025.12.12
ARG SDL_SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8
ARG SDL_MIXER_SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
ARG SDL_NET_SHA256=098522fc26d4e302ef9348aee6e76e67fe504dfefd7f596236568f8330570c41

RUN dnf install --assumeyes --setopt=install_weak_deps=False \
        alsa-lib-devel \
        binutils \
        cmake \
        curl \
        dbus-devel \
        file \
        flatpak \
        flatpak-builder \
        fluidsynth-devel \
        gcc \
        gcc-c++ \
        git \
        gzip \
        libX11-devel \
        libXcursor-devel \
        libXext-devel \
        libXfixes-devel \
        libXi-devel \
        libXrandr-devel \
        libXScrnSaver-devel \
        libXtst-devel \
        libglvnd-devel \
        libglvnd-egl \
        libglvnd-opengl \
        mingw64-gcc-c++ \
        msitools \
        ninja-build \
        pkgconf-pkg-config \
        pulseaudio-libs-devel \
        rpm-build \
        systemd-devel \
        vcpkg \
        zip \
    && dnf clean all

RUN git clone --branch "${VCPKG_TAG}" --depth 1 \
        https://github.com/microsoft/vcpkg.git /opt/vcpkg

RUN flatpak remote-add --system --if-not-exists flathub \
        https://dl.flathub.org/repo/flathub.flatpakrepo \
    && flatpak install --system --noninteractive --or-update flathub \
        org.freedesktop.Platform//25.08 \
        org.freedesktop.Sdk//25.08

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

ENV LD_LIBRARY_PATH=/opt/abuse/lib64:/opt/abuse/lib
WORKDIR /tmp/windows-manifest
COPY packaging/windows/vcpkg-container.json vcpkg.json
COPY packaging/windows/x86_64-w64-mingw32.cmake /tmp/windows-toolchain.cmake
RUN sed --in-place \
        '/vcpkg_copy_tools(TOOL_NAMES fluidsynth AUTO_CLEAN)/d' \
        /opt/vcpkg/ports/fluidsynth/portfile.cmake
RUN VCPKG_DISABLE_METRICS=1 vcpkg install \
        --triplet x64-mingw-dynamic \
        --host-triplet x64-linux \
        --vcpkg-root=/opt/vcpkg \
        --x-manifest-root=/tmp/windows-manifest \
        --x-install-root=/opt/vcpkg-installed \
        --x-wait-for-lock

RUN cmake -S "/tmp/sdl/SDL3-${SDL_VERSION}" -B /tmp/sdl/build-sdl-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/windows \
        -DCMAKE_TOOLCHAIN_FILE=/tmp/windows-toolchain.cmake \
        -DSDL_EXAMPLES=OFF \
        -DSDL_INSTALL_DOCS=OFF \
        -DSDL_STATIC=OFF \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
    && cmake --build /tmp/sdl/build-sdl-windows --parallel \
    && cmake --install /tmp/sdl/build-sdl-windows

RUN cmake -S "/tmp/sdl/SDL3_mixer-${SDL_MIXER_VERSION}" -B /tmp/sdl/build-sdl-mixer-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/windows \
        -DCMAKE_PREFIX_PATH="/opt/windows;/opt/vcpkg-installed/x64-mingw-dynamic" \
        -DCMAKE_TOOLCHAIN_FILE=/tmp/windows-toolchain.cmake \
        -DFluidSynth_INCLUDE_PATH=/opt/vcpkg-installed/x64-mingw-dynamic/include \
        -DFluidSynth_LIBRARY=/opt/vcpkg-installed/x64-mingw-dynamic/lib/libfluidsynth-3.dll.a \
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
    && cmake --build /tmp/sdl/build-sdl-mixer-windows --parallel \
    && cmake --install /tmp/sdl/build-sdl-mixer-windows

RUN cmake -S "/tmp/sdl/SDL3_net-${SDL_NET_VERSION}" -B /tmp/sdl/build-sdl-net-windows -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/windows \
        -DCMAKE_PREFIX_PATH=/opt/windows \
        -DCMAKE_TOOLCHAIN_FILE=/tmp/windows-toolchain.cmake \
        -DBUILD_SHARED_LIBS=ON \
        -DSDLNET_SAMPLES=OFF \
    && cmake --build /tmp/sdl/build-sdl-net-windows --parallel \
    && cmake --install /tmp/sdl/build-sdl-net-windows

WORKDIR /src
COPY . .
RUN mkdir /output \
    && cmake -S . -B /build-rpm -G Ninja \
        -DABUSE_BUNDLE_SDL_RUNTIME=ON \
        -DABUSE_RPM_RUNTIME_REQUIRES= \
        -DABUSE_SDL3_LICENSE_FILE=/tmp/sdl/SDL3-${SDL_VERSION}/LICENSE.txt \
        -DABUSE_SDL3_MIXER_LICENSE_FILE=/tmp/sdl/SDL3_mixer-${SDL_MIXER_VERSION}/LICENSE.txt \
        -DABUSE_SDL3_NET_LICENSE_FILE=/tmp/sdl/SDL3_net-${SDL_NET_VERSION}/LICENSE.txt \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH=/opt/abuse \
    && cmake --build /build-rpm --parallel \
    && cpack --config /build-rpm/CPackConfig.cmake -G RPM -B /output \
    && cpack --config /build-rpm/CPackConfig.cmake -G TGZ -B /output \
    && cmake -E remove_directory /output/_CPack_Packages

RUN cmake -S . -B /build-windows -G Ninja \
        -DABUSE_BUILD_TOOLS=OFF \
        -DABUSE_HMI2MID_EXECUTABLE=/build-rpm/src/abuse-hmi2mid \
        -DABUSE_WINDOWS_OUTPUT_DIR=/output \
        -DCMAKE_BUILD_TYPE=Release \
        -DABUSE_WINDOWS_RUNTIME_DIR=/opt/vcpkg-installed/x64-mingw-dynamic/bin \
        -DCMAKE_PREFIX_PATH="/opt/windows;/opt/vcpkg-installed/x64-mingw-dynamic" \
        -DCMAKE_TOOLCHAIN_FILE=/src/packaging/windows/x86_64-w64-mingw32.cmake \
        -DWIXL_EXECUTABLE=/usr/bin/wixl \
        -DWIXL_HEAT_EXECUTABLE=/usr/bin/wixl-heat \
    && cmake --build /build-windows \
        --target windows-package-artifacts --parallel \
    && cmake -E remove_directory /output/_CPack_Packages

FROM builder AS artifact
RUN mkdir /artifact \
    && cp -R --preserve=mode,timestamps /output/. /artifact/ \
    && chmod +x /src/packaging/container/export-packages.sh
ENTRYPOINT ["/src/packaging/container/export-packages.sh"]
