#!/bin/sh
set -eu

: "${ABUSE_VERSION:?ABUSE_VERSION is required}"

# Preserve completed packages and their ownership even when Flatpak fails.
fix_ownership() {
    if [ -n "${OUTPUT_UID:-}" ] && [ -n "${OUTPUT_GID:-}" ]; then
        chown -R "${OUTPUT_UID}:${OUTPUT_GID}" /output
    fi
}
trap fix_ownership EXIT
cp -R --preserve=mode,timestamps /artifact/. /output/

# Keep downloads and completed module builds across disposable containers.
mkdir -p /cache
exec 9>/cache/build.lock
flock 9
flatpak-builder \
    --disable-updates \
    --disable-rofiles-fuse \
    --force-clean \
    --state-dir=/cache/state \
    --repo=/cache/repo \
    /cache/build \
    /src/packaging/flatpak/io.github.metinc.abuse.yaml
flatpak build-bundle \
    --arch=x86_64 \
    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    /cache/repo \
    "/output/Abuse-${ABUSE_VERSION}-x86_64.flatpak" \
    io.github.metinc.abuse
