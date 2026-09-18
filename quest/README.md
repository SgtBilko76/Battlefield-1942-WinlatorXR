# Quest package files

Source of the non-binary files in the BFVR Quest beta zip. See
[docs/QUEST_BETA.md](../docs/QUEST_BETA.md) for how they are used.

The zip is assembled as:

```
BFVR-Quest-Beta<N>/
  README-QUEST.md            docs/QUEST_BETA.md
  BF1942/                    quest/BF1942/*
  BF1942/BFVR/               BFVR.exe, BFVRClient.dll, BFVRD3D8To9.dll (Win32
                             build), assets/, UserConfig.txt (written by
                             BFVRUserSettingsSeedWriter), THIRD_PARTY_NOTICES.md,
                             licenses/ (d3d8to9, MinHook)
  Winlator/                  quest/Winlator/*
```

The `.bat` files use CRLF line endings; Wine's `cmd` needs them.
