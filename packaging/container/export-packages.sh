#!/bin/sh
set -eu

: "${ABUSE_VERSION:?ABUSE_VERSION is required}"

cp -R --preserve=mode,timestamps /artifact/. /output/

rm -rf /tmp/abuse-flatpak-build /tmp/abuse-flatpak-repo /tmp/abuse-flatpak-state
flatpak-builder \
    --disable-rofiles-fuse \
    --force-clean \
    --state-dir=/tmp/abuse-flatpak-state \
    --repo=/tmp/abuse-flatpak-repo \
    /tmp/abuse-flatpak-build \
    /src/packaging/flatpak/com.github.metinc.abuse.yaml
flatpak build-bundle \
    --arch=x86_64 \
    /tmp/abuse-flatpak-repo \
    "/output/Abuse-${ABUSE_VERSION}-x86_64.flatpak" \
    com.github.metinc.abuse

if [ -n "${OUTPUT_UID:-}" ] && [ -n "${OUTPUT_GID:-}" ]; then
    chown -R "${OUTPUT_UID}:${OUTPUT_GID}" /output || true
fi
