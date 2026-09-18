# BFVR Quest Beta 1 (WinlatorXR)

Experimental standalone build of BFVR for Meta Quest 3. Battlefield 1942 runs
under Wine and Box64 inside WinlatorXR; BFVR takes head and controller
tracking from WinlatorXR and renders the game in stereo. The Windows PC version
is unchanged and not part of this package.

This is a beta. It was tested on one Quest 3 with the MoonGamers Battlefield
1942 package; expect rough edges (see Known issues).

## Requirements

- Meta Quest 3 with WinlatorXR `cats-27` (package `com.winlator.cmod`).
- Battlefield 1942 from the MoonGamers "BF1942 & Expansions" installer (the
  dgVoodoo version), installed on a PC. BFVR does not include any game files.
- About 2 GB free on the headset (the game folder is 1.8 GB). Play with more
  than 50% battery; the Quest throttles below that.

## Install

1. Install the MoonGamers package on a PC and copy its complete
   `Battlefield 1942` folder to the headset as `Download/BF1942`
   (so `BF1942.exe` is at `Download/BF1942/BF1942.exe`).
2. Copy the contents of this zip's `BF1942` folder into `Download/BF1942`
   (this adds `BFVR/`, the `.bat` files and `dxvk.conf`), and this zip's
   `Winlator` folder to `Download/Winlator`.
3. In WinlatorXR create a container (these settings were tested):
   - Screen size **3120x1430** (side-by-side needs about 2.2:1).
   - Drive `D:` = `/sdcard/Download`.
   - Wine `proton-9.0-x86_64`, Box64.
   - DX wrapper **DXVK 1.10.x** with **Async** on. Newer DXVK versions of the
     Direct3D 8 wrapper fail on the Quest; BFVR only needs DXVK's Direct3D 9.
   - XR controller mapping: give every button a key Battlefield 1942 does not
     use. `KEY_NONE` does not stay saved (WinlatorXR restores its defaults A,
     B, X, Y, Space, Enter and the arrow keys, which the game would receive in
     addition to BFVR's controls). Tested mapping:
     Button A `KEY_KP_7`, Button B `KEY_KP_9`, Button X `KEY_KP_1`,
     Button Y `KEY_KP_3`, Grip `KEY_KP_5`, Trigger `KEY_KP_DIVIDE`,
     Thumbstick up/down/left/right `KEY_KP_MULTIPLY`/`KEY_KP_SUBTRACT`/
     `KEY_KP_ADD`/`KEY_SCROLL_LOCK`.
   - Keep "Use XR controller as mouse" on (menus use it; BFVR blocks it in
     gameplay).
   - In WinlatorXR's XR settings set the CPU level to the highest value.
4. Start the container and run `D:\BF1942\Setup-BFVR-Quest.bat` once. It
   backs up every file it changes (`*.bfvr-backup`) and:
   - installs BFVR's Direct3D 8 translator as the game's `D3D8.dll`;
   - disables the intro movies (black under WinlatorXR);
   - switches to software sound (hardware sound crashes the game there);
   - sets 3120x1430, texture quality 3, 60% view distance and normal object
     detail distance.
   Keep detail textures on: turning them off crashes map loading on the
   Quest.
5. Optional: run `D:\BF1942\Install-BFVR-Shortcut.bat` once; WinlatorXR's
   Shortcuts tab then shows **BFVR-VR**. The shortcut expects container 3;
   edit `container_id` in `Download/Winlator/BFVR-VR.desktop` otherwise.

If you use another screen size, change `SCREEN_W`/`SCREEN_H` in
`Setup-BFVR-Quest.bat` and `screenSize` in `BFVR-VR.desktop` to match.

## Play

Start `D:\BF1942\BFVR-VR.bat` (or the BFVR-VR shortcut). The first start
takes a few minutes. Native menus appear on WinlatorXR's flat virtual screen;
gameplay is head tracked.

Controls:

| Input | Action |
|---|---|
| Right trigger | Fire |
| Right grip | Secondary fire |
| Left trigger | Use |
| A | Reload |
| B (short press) | Next weapon |
| B (hold 2 s) | Recenter |
| Left stick | Move |
| Right stick | Turn (snap turn in VR Settings), up jump, down crouch |
| X | Prone |
| Y (hold) | Scoreboard |
| Left grip (near the weapon) | Hold the weapon with both hands |
| Left stick click (hold) | Quick Menu; point with the right hand, release to choose |
| Left menu button | Game menu |

The Quick Menu also opens VR Settings (turning, HUD, comfort vignette, color
profile, kill sound and more).

`BFVR-VR.bat` options: `BFVR_WINLATORXR_HUD_SCALE` (HUD size, 0.3–1.0),
`BFVR_WINLATORXR_AER` (0 side by side, 1 alternate eye) and
`BFVR_WINLATORXR_FOV` (degrees). `dxvk.conf` enables 16x anisotropic
filtering.

## Known issues

- Occasionally the screen stays black with music right after starting. Close
  the game and start it again. `BFVR\logs\watchdog.log` records where it
  stopped; please include it in reports.
- Two-hand weapon grip does not always attach; hold the left grip closer to
  the weapon's front grip.
- The left stick click no longer pitches aircraft up.
- A left stick click in a native menu also toggles WinlatorXR's 3DoF mode.
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
