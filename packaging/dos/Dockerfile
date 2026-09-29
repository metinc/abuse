# syntax=docker/dockerfile:1

FROM debian:12-slim AS dependencies

ARG JOBS=8
ENV ABUSE_DOS_BUILD_DIR=/build-dos

RUN apt-get update && apt-get install --yes --no-install-recommends \
        build-essential \
        bzip2 \
        ca-certificates \
        cmake \
        curl \
        libfl2 \
        ninja-build \
        patch \
        python3 \
        unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY packaging/dos/build.sh packaging/dos/toml11-djgpp.patch packaging/dos/
# Game changes reuse the downloaded toolchain and compiled SDL libraries.
RUN JOBS="$JOBS" bash packaging/dos/build.sh --dependencies-only

FROM dependencies AS builder
COPY . .
RUN JOBS="$JOBS" bash packaging/dos/build.sh

FROM debian:12-slim AS dos-artifact
COPY --from=builder /build-dos/abuse-dos.zip /artifact/
ENTRYPOINT ["cp", "-R", "--preserve=mode,timestamps", "/artifact/.", "/output/"]
