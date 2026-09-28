#!/usr/bin/env python3
"""Stage the BFVR WinlatorXR Dawn package (Meta Quest) and zip it.

Usage:
  stage_quest_dawn.py WIN32_BUILD DESTINATION_DIR NAME

WIN32_BUILD holds BFVR.exe, BFVRClient.dll and BFVRD3D8To9.dll. The package
is created as DESTINATION_DIR/NAME/ and DESTINATION_DIR/NAME.zip; existing
output is never overwritten. The zip is extracted INTO the Battlefield 1942
folder, so its files sit at the top level and are zipped without a
wrapping folder (see quest/README.md).
"""

import shutil
import sys
import zipfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent
BINARIES = ("BFVR.exe", "BFVRClient.dll", "BFVRD3D8To9.dll")
# No batch files: BFVR.exe applies the game setup itself on WinlatorXR, and
# the Dawn shortcut profiles start it directly.
GAME_FILES = ("dxvk.conf",)
LICENSES = {
    "d3d8to9-LICENSE.md": SOURCE / "third_party/d3d8to9-1.15.1/LICENSE.md",
    "MinHook-LICENSE.txt": SOURCE / "third_party/minhook-1.3.4/LICENSE.txt",
}


def stage(win32_build: Path, root: Path) -> None:
    dawn = SOURCE / "quest/Dawn"
    shutil.copy2(dawn / "README_DAWN.txt", root)
    for profile in dawn.glob("*.wxrprofile.json"):
        shutil.copy2(profile, root)
    for name in GAME_FILES:
        shutil.copy2(SOURCE / "quest/BF1942" / name, root)

    # No UserConfig.txt: BFVR starts from its defaults, and an update must
    # not overwrite the player's saved VR settings.
    bfvr = root / "BFVR"
    (bfvr / "licenses").mkdir(parents=True)
    for name in BINARIES:
        shutil.copy2(win32_build / name, bfvr)
    shutil.copytree(SOURCE / "assets", bfvr / "assets")
    shutil.copy2(SOURCE / "THIRD_PARTY_NOTICES.md", bfvr)
    for name, path in LICENSES.items():
        shutil.copy2(path, bfvr / "licenses" / name)


def main() -> int:
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    win32_build, destination = Path(sys.argv[1]), Path(sys.argv[2])
    name = sys.argv[3]
    root = destination / name
    archive = destination / f"{name}.zip"
    if root.exists() or archive.exists():
        print(f"Refusing to overwrite {root} or {archive}.")
        return 1
    root.mkdir(parents=True)
    stage(win32_build, root)
    count = 0
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as package:
        for path in sorted(p for p in root.rglob("*") if p.is_file()):
            package.write(path, path.relative_to(root))
            count += 1
    print(f"Staged {root} and {archive} ({count} files).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
