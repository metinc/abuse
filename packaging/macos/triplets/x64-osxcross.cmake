set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_DEPLOYMENT_TARGET 11.0)
# Autoconf must not try to execute Mach-O probes on the Linux build host.
set(VCPKG_MAKE_BUILD_TRIPLET "--host=x86_64-apple-darwin23.6 --build=x86_64-pc-linux-gnu")
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../osxcross.cmake")
