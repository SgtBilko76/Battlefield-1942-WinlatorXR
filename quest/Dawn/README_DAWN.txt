BFVR - BATTLEFIELD 1942 VR - WinlatorXR DAWN PACKAGE - BETA 3
=============================================================

Battlefield 1942 in VR, standalone on Meta Quest 3 through WinlatorXR Dawn.
You need your own copy of the MoonGamers "BF1942 & Expansions" package
(dgVoodoo version). No game files are included. On the WinlatorXR cats
builds use BFVR-Quest-Installer-Beta3.zip instead (it has install.cmd).


WHAT IS IN HERE
---------------
  BFVR\                 the VR mod: BFVR.exe (the launcher), BFVRClient.dll,
                        the Direct3D 8 translator and the menu art
  dxvk.conf             16x anisotropic filtering
  BFVR-VR*.wxrprofile.json
                        the tested shortcut settings, ready to import into
                        Dawn: BFVR-VR (base game), BFVR-VR-RoadToRome and
                        BFVR-VR-SecretWeapons


INSTALL
-------
1. Install the MoonGamers package on a PC and copy its complete
   "Battlefield 1942" folder to the headset as
     /sdcard/Download/BF1942
   (so that BF1942\BF1942.exe exists). Keep about 2 GB free.

2. Extract everything from this zip INTO that folder, so that you get
     BF1942\BFVR\BFVR.exe
     BF1942\dxvk.conf

3. In WinlatorXR Dawn (drive D: = /sdcard/Download) create a shortcut for
     D:\BF1942\BFVR\BFVR.exe
   Copy the .wxrprofile.json files to
     /sdcard/Download/Winlator/WxrProfiles/
   and import BFVR-VR in the shortcut's settings. It sets the screen size
   (1792x1624), DXVK 2.6.2-gplasync with async, the Turnip wrapper driver
   and Box64. For the expansions make two more shortcuts to the same
   BFVR.exe and import BFVR-VR-RoadToRome / BFVR-VR-SecretWeapons (they add
   the launch arguments --play -- +game XPack1 / XPack2).

4. XR controller mapping: give every button a numpad key the game does not
   use (KEY_NONE is not kept, WinlatorXR restores keys the game would read):
     Button A KEY_KP_7        Button B KEY_KP_9
     Button X KEY_KP_1        Button Y KEY_KP_3
     Grip     KEY_KP_5        Trigger  KEY_KP_DIVIDE
     Thumbstick up/down/left/right: KEY_KP_MULTIPLY / KEY_KP_SUBTRACT /
                                    KEY_KP_ADD / KEY_SCROLL_LOCK
   Keep "Use XR controller as mouse" on, and set the CPU level to the
   highest value in the XR settings.

5. Put the headset ON, then start the shortcut. BFVR sets the game folder up
   by itself on the first start (backs up every file it changes as
   *.bfvr-backup, installs its translator as D3D8.dll, turns off the
   expansions' intro clips, switches to software sound and Quest video
   settings) and matches the game's resolution to the screen size on every
   start. The first start takes a few minutes. Native menus appear on
   WinlatorXR's flat screen; gameplay is head tracked.

UPDATING: extract the new zip over the folder again. Your VR settings
(BFVR\UserConfig.txt) are not in the zip and stay as they are.


CONTROLS (same as PC VR)
------------------------
  Right trigger = fire            Right grip = aim / alt fire
  Left trigger  = use / enter and leave vehicles
  A (hold)      = Quick Menu      B = reload (hold 2 s: recenter)
  X = prone                       Y (hold) = scoreboard
  Left stick    = move, click = map
  Right stick   = turn, up = jump, down = crouch
  Left grip     = hold the weapon with both hands
  Left menu button = game menu    Right stick click = WinlatorXR menu


TROUBLESHOOTING
---------------
  - A button also opens the map or the scoreboard: WinlatorXR types a game
    key for it; check the controller mapping in step 4. (A typed Enter is
    ignored during gameplay, so it cannot open the spawn screen.)
  - Black screen with music after starting: close and start again.
  - Low fps: charge the headset above 50% (it throttles when low).
  - Reports: send BFVR\loader.log and BFVR\logs\watchdog.log.

Battlefield 1942 is (C) Electronic Arts / DICE.
BFVR: https://github.com/SgtBilko76/Battlefield-1942-WinlatorXR
