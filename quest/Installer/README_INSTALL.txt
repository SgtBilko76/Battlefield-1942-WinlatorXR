BFVR - BATTLEFIELD 1942 VR (Meta Quest / WinlatorXR cats) - INSTALLER - BETA 3
===============================================================================

This package installs BFVR onto an EXISTING Battlefield 1942 folder inside a
WinlatorXR container on Meta Quest 3. It does NOT include the game - use your
own copy of the MoonGamers "BF1942 & Expansions" package (dgVoodoo version).

This is the package for the WinlatorXR "cats" builds (cats-27). On
WinlatorXR Dawn use BFVR-Quest-Dawn-Beta3.zip instead: it is extracted into
the game folder and brings importable container profiles.

Package contents:
  install.cmd          - the installer (run inside the container)
  README_INSTALL.txt   - this file
  README-QUEST.md      - full guide: controls, options, known issues
  files\               - BFVR, launchers, dxvk.conf and the setup script

--------------------------------------------------------------------------
WHAT THE INSTALLER DOES vs. MANUAL STEPS (WinlatorXR UI only)
--------------------------------------------------------------------------
install.cmd copies BFVR into your game folder, applies the Quest game
settings (Setup-BFVR-Quest.bat) and adds the three WinlatorXR shortcuts. It
CANNOT configure the container itself - do those steps by hand (below).

--------------------------------------------------------------------------
SETUP - do these in order
--------------------------------------------------------------------------
0) Use WinlatorXR cats-27 (package com.winlator.cmod).

1) Install the MoonGamers package on a PC and copy its complete
   "Battlefield 1942" folder to the headset as Download/BF1942
   (so D:\BF1942\BF1942.exe exists). Keep about 2 GB free.

2) Create a container in WinlatorXR and set:
     - Screen size : 1792x1624
     - Drive  D:   : /sdcard/Download
     - Wine        : proton-9.0-x86_64, Box64
     - DX wrapper  : DXVK, Async on (1.10.x and 2.6.2-gplasync were used;
                     BFVR brings its own Direct3D 8 translator and only
                     needs DXVK's Direct3D 9)
     - XR controller mapping - give every button a numpad key the game
       does not use (KEY_NONE is not kept):
         Button A KEY_KP_7      Button B KEY_KP_9
         Button X KEY_KP_1      Button Y KEY_KP_3
         Grip     KEY_KP_5      Trigger  KEY_KP_DIVIDE
         Thumbstick up/down/left/right: KEY_KP_MULTIPLY / KEY_KP_SUBTRACT /
                                         KEY_KP_ADD / KEY_SCROLL_LOCK
     - Keep "Use XR controller as mouse" on.
     - XR settings: CPU level at the highest value.

3) Copy this whole installer folder to D: (e.g. D:\BFVR-Setup), start the
   container and run install.cmd INSIDE it (file manager, or a one-shot
   shortcut: wine C:\windows\system32\cmd.exe  /c D:\BFVR-Setup\install.cmd).
   Accept D:\BF1942 (or type your folder, without spaces) and the container
   number shown in WinlatorXR (default 3).

4) Close the window, open WinlatorXR's Shortcuts tab and start BFVR-VR
   (or BFVR-VR-RoadToRome / BFVR-VR-SecretWeapons). The first start takes a
   few minutes. Native menus appear on WinlatorXR's flat screen; gameplay is
   head tracked.

UPDATING (also from Beta 2): set the container's screen size to 1792x1624,
then run the new installer the same way. Your VR settings
(BFVR\UserConfig.txt) are kept.

--------------------------------------------------------------------------
CONTROLS (same as PC VR)
--------------------------------------------------------------------------
  Right trigger = fire            Right grip = aim / alt fire
  Left trigger  = use / enter and leave vehicles
  A (hold)      = Quick Menu      B = reload (hold 2 s: recenter)
  X = prone                       Y (hold) = scoreboard
  Left stick    = move, click = map
  Right stick   = turn, up = jump, down = crouch
  Left grip     = hold the weapon with both hands
  Left menu button = game menu    Right stick click = WinlatorXR menu

--------------------------------------------------------------------------
TROUBLESHOOTING
--------------------------------------------------------------------------
  - Blurry/squeezed 640x480 picture: the container screen size is not
    1792x1624. Fix it and run D:\BF1942\Setup-BFVR-Quest.bat again.
  - A button also opens the map/scoreboard: WinlatorXR types a game key for
    it; check the XR controller mapping in step 2.
  - Black screen with music after starting: close and start again.
  - Low fps: charge the headset above 50%.
  - Reports: start BFVR-VR-debug.bat and send BFVR\logs\*.log and vr-logs.
