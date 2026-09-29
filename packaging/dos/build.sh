#!/usr/bin/env bash
# Build inside the DOS container. The dependency pass is cached separately.
set -euo pipefail

dependencies_only=false
if [[ $# == 1 && $1 == --dependencies-only ]]; then
    dependencies_only=true
elif [[ $# != 0 ]]; then
    echo "usage: $0 [--dependencies-only]" >&2
    exit 1
fi

repo=$(cd "$(dirname "$0")/../.." && pwd)
build=${ABUSE_DOS_BUILD_DIR:-"$repo/build/dos"}
mkdir -p "$build"
build=$(cd "$build" && pwd)
jobs=${JOBS:-8}
downloads="$build/downloads"
sources="$build/sources"
prefix="$build/prefix"
package="$build/package"
mkdir -p "$downloads" "$sources" "$prefix" "$package/licenses"

for tool in cmake ninja curl tar bzip2 sha256sum patch unzip g++ python3; do
    command -v "$tool" >/dev/null || { echo "Missing build tool: $tool" >&2; exit 1; }
done

download() {
    local name=$1 url=$2 checksum=$3
    if [[ ! -f "$downloads/$name" ]]; then
        curl --fail --location --retry 3 "$url" -o "$downloads/$name.part"
        mv "$downloads/$name.part" "$downloads/$name"
    fi
    echo "$checksum  $downloads/$name" | sha256sum --check --status || {
        echo "Checksum mismatch: $downloads/$name" >&2
        exit 1
    }
}

if ! command -v i586-pc-msdosdjgpp-g++ >/dev/null && ! command -v i386-pc-msdosdjgpp-g++ >/dev/null; then
    if [[ $(uname -s)-$(uname -m) != Linux-x86_64 ]]; then
        echo "Install a DJGPP cross-compiler and add it to PATH on this host." >&2
        exit 1
    fi
    download djgpp.tar.bz2 \
        https://github.com/andrewwutw/build-djgpp/releases/download/v3.4/djgpp-linux64-gcc1220.tar.bz2 \
        8464f17017d6ab1b2bb2df4ed82357b5bf692e6e2b7fee37e315638f3d505f00
    [[ -d "$build/djgpp" ]] || tar -xjf "$downloads/djgpp.tar.bz2" -C "$build"
    export PATH="$build/djgpp/bin:$PATH"
fi

sdl_revision=1ce4c5bc2916702e8e0f6df1f612dbd8633011da
mixer_revision=df66ae893c91b0f6fa5d026a195bb278213d007d
adlmidi_revision=c391f887f917ab42d5c2315e145184e052650670
download sdl.tar.gz "https://codeload.github.com/libsdl-org/SDL/tar.gz/$sdl_revision" \
    b736d37352b70cf43ad69844352d1ea928e2aac5def0600d6b3c25eb2ddfa648
download mixer.tar.gz "https://codeload.github.com/libsdl-org/SDL_mixer/tar.gz/$mixer_revision" \
    02991af8f01e2d35a52876749a7077c795fcf3037c76a3696f153610c24e7a41
download toml11.tar.gz https://codeload.github.com/ToruNiina/toml11/tar.gz/refs/tags/v4.4.0 \
    815bfe6792aa11a13a133b86e7f0f45edc5d71eb78f5fb6686c49c7f792b9049
download adlmidi.tar.gz "https://codeload.github.com/Wohlstand/libADLMIDI/tar.gz/$adlmidi_revision" \
    147df675e3b82e8e6e2a9b710af7c86a4f0f4cb1c09d8217c6bc8024a61ef52b
download cwsdpmi.zip https://www.ibiblio.org/pub/micro/pc-stuff/freedos/files/util/system/cwsdpmi/csdpmi7b.zip \
    deacda0488e1cdd7c4a9f32fab45662b34c0ed6b2d7d4d13bc07041b62004a8c

sdl="$sources/SDL-$sdl_revision"
mixer="$sources/SDL_mixer-$mixer_revision"
toml="$sources/toml11-4.4.0"
adlmidi="$sources/libADLMIDI-$adlmidi_revision"
[[ -d "$sdl" ]] || tar -xzf "$downloads/sdl.tar.gz" -C "$sources"
[[ -d "$mixer" ]] || tar -xzf "$downloads/mixer.tar.gz" -C "$sources"
[[ -d "$toml" ]] || tar -xzf "$downloads/toml11.tar.gz" -C "$sources"
[[ -d "$adlmidi" ]] || tar -xzf "$downloads/adlmidi.tar.gz" -C "$sources"
if [[ ! -f "$toml/.abuse-djgpp-patched" ]]; then
    patch -d "$toml" -p1 < "$repo/packaging/dos/toml11-djgpp.patch"
    touch "$toml/.abuse-djgpp-patched"
fi
toolchain="$sdl/build-scripts/i586-pc-msdosdjgpp.cmake"
common=(-G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DCMAKE_INSTALL_PREFIX=$prefix")

cmake -S "$sdl" -B "$build/sdl" "${common[@]}" -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF
cmake --build "$build/sdl" -j "$jobs"
cmake --install "$build/sdl"
cmake -S "$mixer" -B "$build/mixer" "${common[@]}" \
    "-DSDL3_DIR=$prefix/lib/cmake/SDL3" -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_C_FLAGS=-DSDL_DISABLE_SSE \
    -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF -DSDLMIXER_FLAC=OFF \
    -DSDLMIXER_GME=OFF -DSDLMIXER_MOD=OFF -DSDLMIXER_MP3=OFF -DSDLMIXER_MIDI=OFF \
    -DSDLMIXER_OPUS=OFF -DSDLMIXER_VORBIS_STB=OFF -DSDLMIXER_VORBIS_VORBISFILE=OFF \
    -DSDLMIXER_WAVPACK=OFF
cmake --build "$build/mixer" -j "$jobs"
cmake --install "$build/mixer"

cmake -S "$adlmidi" -B "$build/adlmidi" "${common[@]}" \
    -DlibADLMIDI_STATIC=ON -DlibADLMIDI_SHARED=OFF -DWITH_MIDIPLAY=OFF \
    -DWITH_ADLMIDI2=OFF -DWITH_GENADLDATA=OFF -DBUILD_NO_GREY_BANKS=ON \
    -DWITH_XMI_SUPPORT=OFF
cmake --build "$build/adlmidi" -j "$jobs"
cmake --install "$build/adlmidi"

if "$dependencies_only"; then
    exit 0
fi

# Music conversion uses native Linux tools inside the build container.
mkdir -p "$build/host"
converter="$build/host/abuse-hmi2mid"
if [[ ! -x "$converter" || "$repo/src/sdlport/hmi.cpp" -nt "$converter" || \
      "$repo/src/sdlport/hmi.h" -nt "$converter" || "$repo/src/tool/hmi2mid.cpp" -nt "$converter" ]]; then
    g++ -std=c++17 -O2 -I"$repo/src" "$repo/src/sdlport/hmi.cpp" "$repo/src/tool/hmi2mid.cpp" \
        -o "$converter"
fi
cmake -S "$repo" -B "$build/game" "${common[@]}" \
    "-DCMAKE_INSTALL_PREFIX=$package" "-DSDL3_DIR=$prefix/lib/cmake/SDL3" \
    "-DSDL3_mixer_DIR=$prefix/lib/cmake/SDL3_mixer" \
    "-DlibADLMIDI_DIR=$prefix/lib/cmake/libADLMIDI" \
    "-DCMAKE_MODULE_PATH=$mixer/cmake" \
    "-DFETCHCONTENT_SOURCE_DIR_TOML11=$toml" -DTOML11_INSTALL=OFF \
    -DABUSE_NETWORK=OFF -DABUSE_BUILD_TOOLS=OFF \
    "-DABUSE_HMI2MID_EXECUTABLE=$build/host/abuse-hmi2mid"
cmake --build "$build/game" -j "$jobs"
cmake --install "$build/game"

unzip -p "$downloads/cwsdpmi.zip" bin/CWSDPMI.EXE > "$package/cwsdpmi.exe"
unzip -p "$downloads/cwsdpmi.zip" bin/cwsdpmi.doc > "$package/licenses/cwsdpmi.txt"
cp "$sdl/LICENSE.txt" "$package/licenses/sdl3.txt"
cp "$mixer/LICENSE.txt" "$package/licenses/mixer.txt"
cp "$adlmidi/LICENSE.txt" "$package/licenses/adlmidi.txt"
cp "$adlmidi/LICENSE.LGPL-2.1.txt" "$package/licenses/adllgpl.txt"
cp "$adlmidi/fm_banks_new/LICENSE-TheFatMan.txt" "$package/licenses/fmfatman.txt"
cp "$adlmidi/fm_banks_new/LICENSE-AIL2.txt" "$package/licenses/fmail.txt"
cp "$adlmidi/fm_banks_new/LICENSE-DMXOPL.txt" "$package/licenses/fmdmxopl.txt"
cp "$adlmidi/fm_banks_new/LICENSE-FMSynth.txt" "$package/licenses/fmsynth.txt"
cp "$adlmidi/fm_banks_new/LICENSE-IMF90.txt" "$package/licenses/fmimf90.txt"
cp "$repo/packaging/dos/dosbox.conf" "$package/dosbox.cfg"
cp "$repo/packaging/dos/README.md" "$package/readme.txt"
python3 "$repo/packaging/dos/package.py" "$package" "$build/abuse-dos.zip" \
    "$build/game/install_manifest.txt"
echo "DOS package: $build/abuse-dos.zip"
echo "Run: cd '$package' && dosbox -conf dosbox.cfg"
