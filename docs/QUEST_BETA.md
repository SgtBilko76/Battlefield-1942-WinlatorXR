# BFVR Quest Beta 3 (WinlatorXR)

Experimental standalone build of BFVR for Meta Quest 3. Battlefield 1942 runs
under Wine and Box64 inside WinlatorXR; BFVR takes head and controller
tracking from WinlatorXR and renders the game in stereo. The Windows PC version
is unchanged and not part of this package.

This is a beta. It was tested on one Quest 3 with the MoonGamers Battlefield
1942 package, including both expansions; expect rough edges (see Known
issues).

Beta 3 renders one full 1792x1624 eye per frame (alternate-eye) for a sharper
image, and the Quest controllers now use the PC VR button layout, with the
left stick click as the map button.

### Updating from Beta 2

Dawn: extract the zip over `Download/BF1942`, point the BFVR shortcuts at
`D:\BF1942\BFVR\BFVR.exe` and import the profiles from the zip. cats: set
the container's screen size to 1792x1624 and run the new `install.cmd`. Your
VR settings are kept.

## Requirements

- Meta Quest 3 with WinlatorXR `cats-27` (package `com.winlator.cmod`).
- Battlefield 1942 from the MoonGamers "BF1942 & Expansions" installer (the
  dgVoodoo version), installed on a PC. BFVR does not include any game files.
- About 2 GB free on the headset (the game folder is 1.8 GB). Play with more
  than 50% battery; the Quest throttles below that.

## Install

The release has two downloads with the same BFVR build:

- `BFVR-Quest-Dawn-Beta3.zip` for **WinlatorXR Dawn**, like the Crysis VR
  package: the mod files in the game folder layout plus the exported
  shortcut profiles, zipped without a wrapping folder. No batch files:
  `BFVR.exe` sets the game folder up itself. `README_DAWN.txt` in the zip
  has the steps; they are repeated below.
- `BFVR-Quest-Installer-Beta3.zip` for the **cats** builds (cats-27), like
  the Far Cry VR installer: copy its folder to `Download`, set the container
  up by hand and run `install.cmd` inside the container. It copies BFVR,
  runs `Setup-BFVR-Quest.bat` and adds `.bat` shortcuts for the folder and
  container number you enter. `README_INSTALL.txt` has the steps.

### Dawn

1. Install the MoonGamers package on a PC and copy its complete
   `Battlefield 1942` folder to the headset as `Download/BF1942`
   (so `BF1942.exe` is at `Download/BF1942/BF1942.exe`).
2. Extract everything from the zip into `Download/BF1942` (this adds
   `BFVR/` and `dxvk.conf`).
3. In WinlatorXR Dawn (drive `D:` = `/sdcard/Download`) create a shortcut
   for `D:\BF1942\BFVR\BFVR.exe`, copy the zip's `.wxrprofile.json` files
   to `Download/Winlator/WxrProfiles/` and import **BFVR-VR** in the
   shortcut's settings. For the expansions make two more shortcuts to the
   same `BFVR.exe` and import **BFVR-VR-RoadToRome** and
   **BFVR-VR-SecretWeapons**, which add `--play -- +game XPack1` / `XPack2`.
   The profiles carry the tested settings:
   - Screen size **1792x1624** (one eye; BFVR uses alternate-eye rendering,
     so each frame draws one eye into the whole window).
   - Wine `proton-9.0-x86_64`, Box64 preset Performance.
   - DX wrapper **DXVK 2.6.2-gplasync** with **Async** on, graphics driver
     wrapper (Turnip). BFVR translates Direct3D 8 itself and only needs
     DXVK's Direct3D 9; DXVK's own Direct3D 8 wrapper fails on the Quest.
4. XR controller mapping: give every button a key Battlefield 1942 does not
   use. `KEY_NONE` does not stay saved (WinlatorXR restores its defaults A,
   B, X, Y, Space, Enter and the arrow keys, which the game would receive in
   addition to BFVR's controls). Tested mapping:
   Button A `KEY_KP_7`, Button B `KEY_KP_9`, Button X `KEY_KP_1`,
   Button Y `KEY_KP_3`, Grip `KEY_KP_5`, Trigger `KEY_KP_DIVIDE`,
   Thumbstick up/down/left/right `KEY_KP_MULTIPLY`/`KEY_KP_SUBTRACT`/
   `KEY_KP_ADD`/`KEY_SCROLL_LOCK`. Keep "Use XR controller as mouse" on
   (menus use it; BFVR blocks it in gameplay) and set the CPU level to the
   highest value in WinlatorXR's XR settings.
5. Start the shortcut. Under WinlatorXR, `BFVR.exe` sets the game folder up
   before every launch:
   - it installs BFVR's Direct3D 8 translator as the game's `D3D8.dll`
     (dgVoodoo crashes under Wine) and moves a DXVK `d3d9.dll` out of the
     game folder;
   - once (then it leaves `BFVR\quest-setup.done`): it turns off the
     expansions' intro and legal clips (black under WinlatorXR), switches to
     software sound (hardware sound crashes the game there) and writes Quest
     video settings (texture quality 3, 60% view distance, normal object
     detail distance). Detail textures stay on: turning them off crashes map
     loading on the Quest;
   - every start: sets the game's display mode to the WinlatorXR screen
     size, so the game cannot fall back to 640x480.
   Every changed file is backed up once as `*.bfvr-backup`.

The HUD size (0.36), the 110° field of view and alternate-eye rendering are
BFVR's WinlatorXR defaults; `BFVR_WINLATORXR_HUD_SCALE`,
`BFVR_WINLATORXR_FOV` and `BFVR_WINLATORXR_AER` still override them.

## Play

Start the **BFVR-VR** shortcut; **BFVR-VR-RoadToRome** (*The Road to Rome*)
and **BFVR-VR-SecretWeapons** (*Secret Weapons of WWII*) start the
expansions, which are part of the MoonGamers package and share the base
game's settings. With the cats installer the shortcuts start
`BFVR-VR.bat`, `BFVR-VR-RoadToRome.bat` and `BFVR-VR-SecretWeapons.bat`. The first start
takes a few minutes. Native menus appear on WinlatorXR's flat virtual screen;
gameplay is head tracked.

Controls:

| Input | Action |
|---|---|
| Right trigger | Fire |
| Right grip | Secondary fire |
| Left trigger | Use |
| A (hold) | Quick Menu; point with the right hand, release to choose |
| B | Reload |
| B (hold 2 s) | Recenter |
| Left stick | Move |
| Right stick | Turn (snap turn in VR Settings), up jump, down crouch |
| X | Prone |
| Y (hold) | Scoreboard |
| Left grip (near the weapon) | Hold the weapon with both hands |
| Left stick click | Map (on foot and in vehicles) |
| Left menu button | Game menu (WinlatorXR sends Esc) |

The Quick Menu also opens VR Settings (turning, HUD, comfort vignette, color
profile, kill sound and more).

Options (environment variables, e.g. in the cats `.bat` launchers):
`BFVR_WINLATORXR_HUD_SCALE` (HUD size, 0.3–1.0, default 0.36),
`BFVR_WINLATORXR_AER` (1 alternate eye, the default; 0 side by side, which
needs a screen twice as wide, e.g. 3584x1624) and `BFVR_WINLATORXR_FOV`
(degrees, default 110; 0 uses the headset's own). `dxvk.conf` enables 16x anisotropic
filtering.

## Known issues

- *Secret Weapons of WWII* runs noticeably slower than the base game (busier
  maps). Lowering View Distance in the game's video options helps; the setting
  is shared by the base game and both expansions.

- Occasionally the screen stays black with music right after starting. Close
  the game and start it again. `BFVR\logs\watchdog.log` records where it
  stopped; please include it in reports.
- Two-hand weapon grip does not always attach; hold the left grip closer to
  the weapon's front grip.
- A left stick click in a native menu also toggles WinlatorXR's 3DoF mode.
- If a controller button also opens the map, the scoreboard or another game
  action, WinlatorXR is typing a game key for it: check the container's XR
  controller mapping against the tested mapping above. BFVR drops a typed
  Enter during gameplay (it would open the spawn screen); set
  `BFVR_WINLATORXR_ENTER_FILTER=0` in the launcher to turn that off.
- Not available on the Quest: ambient occlusion, bloom, reflections, FXAA and
  the separate scope layer.

## Reporting problems

Send `BFVR\loader.log` and `BFVR\logs\watchdog.log`. With the cats
installer, run `BFVR-VR-debug.bat` instead of `BFVR-VR.bat` and also send
`BFVR\logs\observer.log` and the files in `D:\BF1942\vr-logs`.

## Uninstall

Delete `BFVR`, `dxvk.conf` and (cats installer) the `BFVR-*.bat` files and
`Setup-BFVR-Quest.bat`, rename each `*.bfvr-backup` file back to its original
name and each `*.bfvr-disabled` file back without that ending, and rename
`Movies.disabled` back to `Movies` if it exists.
