# Quest package files

Source of the non-binary files in the BFVR Quest package for WinlatorXR Dawn.
See [docs/QUEST_BETA.md](../docs/QUEST_BETA.md) for how they are used.

`tools/stage_quest_dawn.py` assembles the zip from a Win32 build. It is
extracted into the Battlefield 1942 folder, so the files sit at the top:

```
README_DAWN.txt              quest/Dawn/README_DAWN.txt
*.wxrprofile.json            quest/Dawn/ (container profile for Dawn's import)
*.bat, dxvk.conf             quest/BF1942/*
Winlator/*.desktop           quest/Winlator/* (Install-BFVR-Shortcut.bat
                             copies them to the container desktop)
BFVR/                        BFVR.exe, BFVRClient.dll, BFVRD3D8To9.dll (Win32
                             build), assets/, THIRD_PARTY_NOTICES.md,
                             licenses/ (d3d8to9, MinHook)
```

`tools/stage_quest_installer.py` builds the second download, the installer
for the WinlatorXR cats builds (`quest/Installer/install.cmd` and
`README_INSTALL.txt`, with the same BFVR files under `files/` plus a seeded
`files/UserConfig.txt` that is copied only when the player has none).

The Dawn zip has no `UserConfig.txt`: BFVR starts from its defaults and an update
must not overwrite the player's settings. The `.bat` files use CRLF line
endings; Wine's `cmd` needs them, and it does not accept `echo(` (use
`echo.`).
