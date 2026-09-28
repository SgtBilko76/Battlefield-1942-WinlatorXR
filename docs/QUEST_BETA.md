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

Set the container's **Screen size to 1792x1624** (or import the profile from
the zip), then extract the zip over `Download/BF1942` and run
`Install-BFVR-Shortcut.bat` once more. Your VR settings are kept.

## Requirements

- Meta Quest 3 with WinlatorXR `cats-27` (package `com.winlator.cmod`).
- Battlefield 1942 from the MoonGamers "BF1942 & Expansions" installer (the
  dgVoodoo version), installed on a PC. BFVR does not include any game files.
- About 2 GB free on the headset (the game folder is 1.8 GB). Play with more
  than 50% battery; the Quest throttles below that.

## Install

The release has two downloads with the same BFVR build:

- `BFVR-Quest-Dawn-Beta3.zip` for **WinlatorXR Dawn**, like the Crysis VR
  package: extract it into the game folder and import the container
  profiles it contains. `README_DAWN.txt` in the zip has the same steps.
  The steps below follow this package.
- `BFVR-Quest-Installer-Beta3.zip` for the **cats** builds (cats-27), like
  the Far Cry VR installer: copy its folder to `Download`, set the container
  up by hand and run `install.cmd` inside the container. It copies BFVR,
  runs `Setup-BFVR-Quest.bat` and adds the shortcuts for the folder and
  container number you enter. `README_INSTALL.txt` has the steps.

1. Install the MoonGamers package on a PC and copy its complete
   `Battlefield 1942` folder to the headset as `Download/BF1942`
   (so `BF1942.exe` is at `Download/BF1942/BF1942.exe`).
2. Extract everything from the zip into `Download/BF1942` (this adds
   `BFVR/`, the `.bat` files, `Winlator/` and `dxvk.conf`).
3. In WinlatorXR create a container with drive `D:` = `/sdcard/Download`,
   copy the zip's three `.wxrprofile.json` files (one per shortcut) to
   `Download/Winlator/WxrProfiles/` and import them once the shortcuts exist
   (step 5). They carry the tested settings (container 3):
   - Screen size **1792x1624** (one eye; the launchers use alternate-eye
     rendering, so each frame draws one eye into the whole window).
   - Wine `proton-9.0-x86_64`, Box64.
   - DX wrapper **DXVK 2.6.2-gplasync** with **Async** on, graphics driver
     wrapper (Turnip), Box64 preset Performance. BFVR translates Direct3D 8
     itself and only needs DXVK's Direct3D 9; DXVK's own Direct3D 8 wrapper
     fails on the Quest.
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
5. Start the container and run `D:\BF1942\Install-BFVR-Shortcut.bat` once;
   WinlatorXR's Shortcuts tab then shows **BFVR-VR**,
   **BFVR-VR-RoadToRome** and **BFVR-VR-SecretWeapons**. The shortcuts
   expect container 3; edit `container_id` in `BF1942/Winlator/*.desktop`
   first otherwise.
6. The first start of any BFVR launcher runs `Setup-BFVR-Quest.bat`, which
   backs up every file it changes (`*.bfvr-backup`) and:
   - installs BFVR's Direct3D 8 translator as the game's `D3D8.dll` (later
     starts keep it on the translator from the current package);
   - disables the intro movies, including the expansions' intro and legal
     clips (black under WinlatorXR);
   - switches to software sound (hardware sound crashes the game there);
   - sets 1792x1624, texture quality 3, 60% view distance and normal object
     detail distance.
   It then leaves `BFVR\quest-setup.done`; delete that file to run the setup
   again. Keep detail textures on: turning them off crashes map loading on
   the Quest.

If you use another screen size, change `SCREEN_W`/`SCREEN_H` in
`Setup-BFVR-Quest.bat` and `screenSize` in the shortcuts to match, and set
the container to the same size: a `.bat` started from the file manager uses
the container's size, and if the game's resolution does not fit, BF1942
falls back to 640x480 and saves that to its profile (delete
`BFVR\quest-setup.done` and start again to restore it).

## Play

Start the **BFVR-VR** shortcut (or `D:\BF1942\BFVR-VR.bat`). The expansions start
with `BFVR-VR-RoadToRome.bat` (*The Road to Rome*) and
`BFVR-VR-SecretWeapons.bat` (*Secret Weapons of WWII*); they are part of the
MoonGamers package and share the base game's settings. The first start
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

`BFVR-VR.bat` options: `BFVR_WINLATORXR_HUD_SCALE` (HUD size, 0.3–1.0),
`BFVR_WINLATORXR_AER` (1 alternate eye, the default in the launchers; 0 side
by side, which needs a screen twice as wide, e.g. 3584x1624) and
`BFVR_WINLATORXR_FOV` (degrees). `dxvk.conf` enables 16x anisotropic
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

Run `BFVR-VR-debug.bat` instead of `BFVR-VR.bat` and send
`BFVR\logs\observer.log`, `BFVR\logs\watchdog.log` and the files in
`D:\BF1942\vr-logs`.

## Uninstall

Delete `BFVR`, the `BFVR-*.bat` files, `Setup-BFVR-Quest.bat`,
`Install-BFVR-Shortcut.bat` and `dxvk.conf`, rename each `*.bfvr-backup` file
back to its original name, and rename `Movies.disabled` back to `Movies`.
