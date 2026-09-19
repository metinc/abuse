#!/bin/sh
set -eu
arch="$1"
triplet="$2"
prefix="/opt/abuse/${arch}"
deps="/opt/vcpkg-installed/${arch}/${triplet}"
build="/build-macos-${arch}"
stage="/stage-macos-${arch}"

cmake -S /src -B "${build}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=/opt/macos/osxcross.cmake \
    -DCMAKE_OSX_ARCHITECTURES="${arch}" \
    -DCMAKE_PREFIX_PATH="${prefix};${deps}" \
    -DABUSE_BUILD_TOOLS=OFF \
    -DABUSE_HMI2MID_EXECUTABLE=/usr/local/bin/abuse-hmi2mid
cmake --build "${build}" --parallel
cmake --install "${build}" --prefix "${stage}"

python3 /src/packaging/macos/bundle-runtime.py \
    "${stage}/abuse.app" "${arch}" "${prefix}/lib" "${deps}/lib"

licenses="${stage}/abuse.app/Contents/Resources/third-party"
mkdir -p "${licenses}"
cp "${prefix}/share/licenses/SDL3/LICENSE.txt" "${licenses}/SDL3.txt"
cp "${prefix}/share/licenses/SDL3_mixer/LICENSE.txt" "${licenses}/SDL3_mixer.txt"
cp "${prefix}/share/licenses/SDL3_net/LICENSE.txt" "${licenses}/SDL3_net.txt"
cp /tmp/compiler-rt/LICENSE.txt "${licenses}/compiler-rt.txt"
for copyright in "${deps}"/share/*/copyright; do
    name=$(basename "$(dirname "${copyright}")")
    cp "${copyright}" "${licenses}/${name}.txt"
done

# Seal the completed bundle, including Info.plist, resources and nested code.
ldid -S "${stage}/abuse.app"

# Package the already relocated and ad-hoc signed bundle, preserving modes.
version=$(sed -n 's/^set(CPACK_PACKAGE_VERSION "\([^"]*\)").*/\1/p' "${build}/CPackConfig.cmake")
test -n "${version}"
mkdir -p /output
cd "${stage}"
zip -q -r "/output/Abuse-${version}-macos-${arch}.zip" abuse.app
