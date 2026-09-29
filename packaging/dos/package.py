#!/usr/bin/env python3
"""Package installed DOS assets, checking 8.3 names and case collisions."""
import re
import sys
import zipfile
from pathlib import Path


def package_game(directory: Path, archive: Path, manifest: Path) -> None:
    directory = directory.resolve()
    files = {Path(line).resolve() for line in manifest.read_text().splitlines() if line}
    files.update(directory / name for name in (
        "cwsdpmi.exe", "dosbox.cfg", "readme.txt", "licenses/cwsdpmi.txt",
        "licenses/sdl3.txt", "licenses/mixer.txt",
        "licenses/adlmidi.txt", "licenses/adllgpl.txt", "licenses/fmfatman.txt",
        "licenses/fmail.txt", "licenses/fmdmxopl.txt", "licenses/fmsynth.txt", "licenses/fmimf90.txt",
    ))
    names = {}
    for file in sorted(files):
        relative = file.relative_to(directory)
        for part in relative.parts:
            if not re.fullmatch(r"[a-zA-Z0-9_!#$%&'()@^{}~+-]{1,8}(\.[a-zA-Z0-9_!#$%&'()@^{}~+-]{1,3})?", part):
                raise ValueError(f"Not a DOS 8.3 filename: {relative}")
        key = str(relative).upper()
        if key in names and names[key] != relative:
            raise ValueError(f"DOS filename collision: {relative} and {names[key]}")
        if not file.is_file():
            raise FileNotFoundError(file)
        names[key] = relative
    # Use the install manifest so local saves and test logs never enter the ZIP.
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as output:
        for relative in sorted(names.values()):
            output.write(directory / relative, relative.as_posix())
    print(f"Packaged {len(names)} files with DOS-compatible names")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit("usage: package.py GAME_DIRECTORY OUTPUT_ZIP INSTALL_MANIFEST")
    package_game(*(Path(arg) for arg in sys.argv[1:]))
