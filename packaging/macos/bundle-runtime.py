#!/usr/bin/env python3
"""Relocate the Mach-O dependency closure and ad-hoc sign it on Linux."""

import pathlib
import shutil
import subprocess
import sys


def run(*args):
    return subprocess.check_output(args, text=True)


def bundle(app, arch, search_dirs):
    tools = f"/osxcross/bin/{arch}-apple-darwin23.6"
    frameworks = app / "Contents/Frameworks"
    frameworks.mkdir(parents=True, exist_ok=True)
    executable = app / "Contents/MacOS/abuse"
    pending = [executable]
    copied = {}
    binaries = []
    while pending:
        binary = pending.pop()
        binaries.append(binary)
        # otool -L includes the dylib's own ID; handle that separately.
        own_id = None
        if binary != executable:
            own_id = run(f"{tools}-otool", "-D", str(binary)).splitlines()[1].strip()
        for line in run(f"{tools}-otool", "-L", str(binary)).splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if dependency == own_id or dependency.startswith(("/usr/lib/", "/System/Library/")):
                continue
            name = pathlib.Path(dependency).name
            candidates = [directory / name for directory in search_dirs]
            source = next((p.resolve() for p in candidates if p.is_file()), None)
            if source is None:
                raise RuntimeError(f"Unresolved dependency in {binary}: {dependency}")
            if name in copied and copied[name] != source:
                raise RuntimeError(f"Conflicting library name: {name}")
            if name not in copied:
                copied[name] = source
                target = frameworks / name
                shutil.copy2(source, target)
                target.chmod(0o755)
                pending.append(target)
            relative = (f"@executable_path/../Frameworks/{name}" if binary == executable
                        else f"@loader_path/{name}")
            run(f"{tools}-install_name_tool", "-change", dependency, relative, str(binary))
        if own_id:
            run(f"{tools}-install_name_tool", "-id", f"@rpath/{binary.name}", str(binary))

    for binary in binaries:
        # Library rewrites invalidate the linker's arm64 signature. Both ABIs
        # receive a fresh ad-hoc signature; this is not Developer ID signing.
        run("ldid", "-S", str(binary))
        header = run(f"{tools}-lipo", "-archs", str(binary)).strip()
        if header != arch:
            raise RuntimeError(f"Wrong architecture for {binary}: {header}")
        for line in run(f"{tools}-otool", "-L", str(binary)).splitlines()[1:]:
            dependency = line.strip().split(" (", 1)[0]
            if not dependency.startswith(("/usr/lib/", "/System/Library/", "@")):
                raise RuntimeError(f"Non-relocatable dependency: {dependency}")


if __name__ == "__main__":
    bundle(pathlib.Path(sys.argv[1]), sys.argv[2],
           [pathlib.Path(p) for p in sys.argv[3:]])
