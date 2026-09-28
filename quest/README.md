# Quest package files

Source of the non-binary files in the BFVR Quest installer package. See
[docs/QUEST_BETA.md](../docs/QUEST_BETA.md) for how they are used.

`tools/stage_quest_installer.py` assembles the package from a Win32 build:

```
BFVR-Quest-Installer-Beta<N>/
  install.cmd                quest/Installer/install.cmd
  README_INSTALL.txt         quest/Installer/README_INSTALL.txt
  README-QUEST.md            docs/QUEST_BETA.md
  files/UserConfig.txt       written by BFVRUserSettingsSeedWriter (copied
                             only when the player has none)
  files/BF1942/              quest/BF1942/* except Install-BFVR-Shortcut.bat
  files/BF1942/BFVR/         BFVR.exe, BFVRClient.dll, BFVRD3D8To9.dll (Win32
                             build), assets/, THIRD_PARTY_NOTICES.md,
                             licenses/ (d3d8to9, MinHook)
```

`install.cmd` writes the WinlatorXR shortcuts itself; `quest/Winlator/` and
`Install-BFVR-Shortcut.bat` remain for manual installs. The `.bat` and `.cmd`
files use CRLF line endings; Wine's `cmd` needs them, and it does not accept
`echo(` (use `echo.`).
