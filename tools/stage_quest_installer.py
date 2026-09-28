#!/usr/bin/env python3
"""Stage the BFVR WinlatorXR (Meta Quest) installer package and zip it.

Usage:
  stage_quest_installer.py WIN32_BUILD USER_CONFIG DESTINATION_DIR NAME

WIN32_BUILD holds BFVR.exe, BFVRClient.dll and BFVRD3D8To9.dll; USER_CONFIG
is the seed written by BFVRUserSettingsSeedWriter. The package is created as
DESTINATION_DIR/NAME/ and DESTINATION_DIR/NAME.zip; existing output is never
overwritten. See quest/README.md for the layout.
"""

import shutil
import sys
import zipfile
from pathlib import Path

SOURCE = Path(__file__).resolve().parent.parent
BINARIES = ("BFVR.exe", "BFVRClient.dll", "BFVRD3D8To9.dll")
# Install-BFVR-Shortcut.bat is for manual installs; install.cmd writes the
# shortcuts itself.
QUEST_FILES = (
    "BFVR-VR.bat",
    "BFVR-VR-RoadToRome.bat",
    "BFVR-VR-SecretWeapons.bat",
    "BFVR-VR-debug.bat",
    "Setup-BFVR-Quest.bat",
    "dxvk.conf",
)
LICENSES = {
    "d3d8to9-LICENSE.md": SOURCE / "third_party/d3d8to9-1.15.1/LICENSE.md",
    "MinHook-LICENSE.txt": SOURCE / "third_party/minhook-1.3.4/LICENSE.txt",
}


def stage(win32_build: Path, user_config: Path, root: Path) -> None:
    shutil.copy2(SOURCE / "quest/Installer/install.cmd", root)
    shutil.copy2(SOURCE / "quest/Installer/README_INSTALL.txt", root)
    shutil.copy2(SOURCE / "docs/QUEST_BETA.md", root / "README-QUEST.md")

    files = root / "files"
    game = files / "BF1942"
    bfvr = game / "BFVR"
    (bfvr / "licenses").mkdir(parents=True)
    shutil.copy2(user_config, files / "UserConfig.txt")
    for name in QUEST_FILES:
        shutil.copy2(SOURCE / "quest/BF1942" / name, game)
    for name in BINARIES:
        shutil.copy2(win32_build / name, bfvr)
    shutil.copytree(SOURCE / "assets", bfvr / "assets")
    shutil.copy2(SOURCE / "THIRD_PARTY_NOTICES.md", bfvr)
    for name, path in LICENSES.items():
        shutil.copy2(path, bfvr / "licenses" / name)


def main() -> int:
    if len(sys.argv) != 5:
        print(__doc__)
        return 2
    win32_build, user_config, destination = (Path(arg) for arg in sys.argv[1:4])
    name = sys.argv[4]
    root = destination / name
    archive = destination / f"{name}.zip"
    if root.exists() or archive.exists():
        print(f"Refusing to overwrite {root} or {archive}.")
        return 1
    root.mkdir(parents=True)
    stage(win32_build, user_config, root)
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as package:
        for path in sorted(root.rglob("*")):
            package.write(path, path.relative_to(destination))
    print(f"Staged {root} and {archive}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
