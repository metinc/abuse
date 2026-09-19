# syntax=docker/dockerfile:1

FROM crazymax/osxcross:14.5-r0-ubuntu AS osxcross
FROM ubuntu:24.04 AS dependencies

ARG VCPKG_TAG=2025.12.12
ARG SDL_VERSION=3.4.0
ARG SDL_MIXER_VERSION=3.2.4
ARG SDL_NET_VERSION=3.2.0
ARG SDL_SHA256=082cbf5f429e0d80820f68dc2b507a94d4cc1b4e70817b119bbb8ec6a69584b8
ARG SDL_MIXER_SHA256=182a07c745375e113dc740d43964ff21b0be29f29f59876c4dbc4db3d32f6901
ARG SDL_NET_SHA256=098522fc26d4e302ef9348aee6e76e67fe504dfefd7f596236568f8330570c41

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --yes --no-install-recommends \
        autoconf automake build-essential ca-certificates clang cmake curl \
        file git libplist-dev libtool libxml2-dev libssl-dev lld ninja-build \
        patch perl pkg-config python3 unzip zip \
    && rm -rf /var/lib/apt/lists/*
RUN git clone --branch v2.1.5-procursus7 --depth 1 \
        https://github.com/ProcursusTeam/ldid.git /tmp/ldid \
    # Bundle signing in this ldid release ignores -Cadhoc. Always mark a
    # signature without a signing identity as ad-hoc, including nested code.
    && sed --in-place \
        's/directory.flags = Swap(uint32_t(flags));/directory.flags = Swap(uint32_t(flags | (key.empty() ? kSecCodeSignatureAdhoc : 0)));/' \
        /tmp/ldid/ldid.cpp \
    && make -C /tmp/ldid --jobs=2 \
    && make -C /tmp/ldid install
COPY --from=osxcross /osxcross /osxcross
ENV PATH="/osxcross/bin:$PATH"
ENV LD_LIBRARY_PATH=/osxcross/lib
ENV MACOSX_DEPLOYMENT_TARGET=11.0
ENV VCPKG_DISABLE_METRICS=1

RUN git clone --branch "${VCPKG_TAG}" --depth 1 \
        https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics
COPY packaging/macos/osxcross.cmake packaging/macos/vcpkg.json /opt/macos/
COPY packaging/macos/triplets /opt/macos/triplets
# vcpkg_copy_tools tries to inspect target executables on the Linux host.
# Only the FluidSynth library is needed by SDL_mixer.
RUN sed --in-place '/vcpkg_copy_tools(TOOL_NAMES fluidsynth AUTO_CLEAN)/d' \
        /opt/vcpkg/ports/fluidsynth/portfile.cmake \
    && ln -s /osxcross/bin/x86_64-apple-darwin23.6-install_name_tool /usr/local/bin/install_name_tool \
    && ln -s /osxcross/bin/x86_64-apple-darwin23.6-otool /usr/local/bin/otool
RUN /opt/vcpkg/vcpkg install \
        --triplet x64-osxcross --host-triplet x64-linux \
        --overlay-triplets=/opt/macos/triplets \
        --x-manifest-root=/opt/macos --x-install-root=/opt/vcpkg-installed/x86_64
RUN /opt/vcpkg/vcpkg install \
        --triplet arm64-osxcross --host-triplet x64-linux \
        --overlay-triplets=/opt/macos/triplets \
        --x-manifest-root=/opt/macos --x-install-root=/opt/vcpkg-installed/arm64

# This OSXCross image has no Darwin compiler-rt. SDL's Objective-C @available
# checks need LLVM's availability helper, even when targeting macOS 11+.
RUN mkdir -p /tmp/compiler-rt \
    && curl --location --fail --retry 3 --output /tmp/compiler-rt/os_version_check.c \
        https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.8/compiler-rt/lib/builtins/os_version_check.c \
    && echo '8ed75fa38482b18f17797c1530739688b6939b7c6495b2ffc1361114b1184c1c  /tmp/compiler-rt/os_version_check.c' | sha256sum --check \
    && curl --location --fail --retry 3 --output /tmp/compiler-rt/LICENSE.txt \
        https://raw.githubusercontent.com/llvm/llvm-project/llvmorg-18.1.8/compiler-rt/LICENSE.TXT

WORKDIR /tmp/sdl
RUN curl --location --fail --retry 3 --output SDL3.tar.gz \
        "https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz" \
    && echo "${SDL_SHA256}  SDL3.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3.tar.gz
RUN curl --location --fail --retry 3 --output SDL3_mixer.tar.gz \
        "https://github.com/libsdl-org/SDL_mixer/releases/download/release-${SDL_MIXER_VERSION}/SDL3_mixer-${SDL_MIXER_VERSION}.tar.gz" \
    && echo "${SDL_MIXER_SHA256}  SDL3_mixer.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3_mixer.tar.gz
RUN curl --location --fail --retry 3 --output SDL3_net.tar.gz \
        "https://github.com/libsdl-org/SDL_net/releases/download/release-${SDL_NET_VERSION}/SDL3_net-${SDL_NET_VERSION}.tar.gz" \
    && echo "${SDL_NET_SHA256}  SDL3_net.tar.gz" | sha256sum --check \
    && tar --extract --file SDL3_net.tar.gz
COPY packaging/macos/build-sdl.sh /opt/macos/build-sdl.sh
RUN sh /opt/macos/build-sdl.sh x86_64 x64-osxcross
RUN sh /opt/macos/build-sdl.sh arm64 arm64-osxcross

FROM dependencies AS builder
WORKDIR /src
COPY . .
# Asset conversion must run on Linux, independently of either target ABI.
RUN c++ -std=c++17 -O2 -Isrc src/sdlport/hmi.cpp src/tool/hmi2mid.cpp \
        -o /usr/local/bin/abuse-hmi2mid
RUN sh packaging/macos/build-package.sh x86_64 x64-osxcross
RUN sh packaging/macos/build-package.sh arm64 arm64-osxcross

FROM ubuntu:24.04 AS artifact
COPY --from=builder /output/ /artifact/
ENTRYPOINT ["cp", "-R", "--preserve=mode,timestamps", "/artifact/.", "/output/"]
