BFVR - BATTLEFIELD 1942 VR - WinlatorXR DAWN PACKAGE - BETA 3
=============================================================

Battlefield 1942 in VR, standalone on Meta Quest 3 through WinlatorXR Dawn.
You need your own copy of the MoonGamers "BF1942 & Expansions" package
(dgVoodoo version). No game files are included.


WHAT IS IN HERE
---------------
  BFVR\                 the VR mod (BFVR.exe, BFVRClient.dll, translator,
                        menu art)
  BFVR-VR.bat           launcher; BFVR-VR-RoadToRome.bat and
                        BFVR-VR-SecretWeapons.bat start the expansions,
                        BFVR-VR-debug.bat writes logs for reports
  Setup-BFVR-Quest.bat  one-time game setup; the launchers run it by
                        themselves on their first start
  Install-BFVR-Shortcut.bat, Winlator\
                        adds the three BFVR shortcuts to WinlatorXR
  dxvk.conf             16x anisotropic filtering
  BFVR-VR*.wxrprofile.json
                        the tested container settings, one per shortcut,
                        ready to import into Dawn


INSTALL
-------
1. Install the MoonGamers package on a PC and copy its complete
   "Battlefield 1942" folder to the headset as
     /sdcard/Download/BF1942
   (so that BF1942\BF1942.exe exists). Keep about 2 GB free.

2. Extract everything from this zip INTO that folder, so that you get
     BF1942\BFVR\BFVR.exe
     BF1942\BFVR-VR.bat
     BF1942\Setup-BFVR-Quest.bat

3. In WinlatorXR Dawn create a container (drive D: = /sdcard/Download) and
   import the profiles: copy the three .wxrprofile.json files to
     /sdcard/Download/Winlator/WxrProfiles/
   and import them in the shortcut settings (after step 5). They set the
   screen size (1792x1624), DXVK 2.6.2-gplasync with async, the Turnip
   wrapper driver and Box64 (Performance preset). They expect container 3.

4. XR controller mapping: give every button a numpad key the game does not
   use (KEY_NONE is not kept, WinlatorXR restores keys the game would read):
     Button A KEY_KP_7        Button B KEY_KP_9
     Button X KEY_KP_1        Button Y KEY_KP_3
     Grip     KEY_KP_5        Trigger  KEY_KP_DIVIDE
     Thumbstick up/down/left/right: KEY_KP_MULTIPLY / KEY_KP_SUBTRACT /
                                    KEY_KP_ADD / KEY_SCROLL_LOCK
   Keep "Use XR controller as mouse" on, and set the CPU level to the
   highest value in the XR settings.

5. Start the container and run D:\BF1942\Install-BFVR-Shortcut.bat once from
   the file manager. The Shortcuts tab then shows BFVR-VR,
   BFVR-VR-RoadToRome and BFVR-VR-SecretWeapons (they expect container 3;
   change container_id in BF1942\Winlator\*.desktop first otherwise).

6. Put the headset ON, then start BFVR-VR. The first start runs the game
   setup (backs up every file it changes as *.bfvr-backup, installs BFVR's
   translator as D3D8.dll, turns off the intro movies, switches to software
   sound, sets 1792x1624) and takes a few minutes. Native menus appear on
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
  - Blurry, squeezed 640x480 picture: the container screen size is not
    1792x1624. Fix it, delete BF1942\BFVR\quest-setup.done and start again.
  - A button also opens the map or the scoreboard: WinlatorXR types a game
    key for it; check the controller mapping in step 4. (A typed Enter is
    ignored during gameplay, so it cannot open the spawn screen.)
  - Black screen with music after starting: close and start again.
  - Low fps: charge the headset above 50% (it throttles when low).
  - Reports: start BFVR-VR-debug.bat and send BFVR\logs\*.log and vr-logs.

Battlefield 1942 is (C) Electronic Arts / DICE.
BFVR: https://github.com/SgtBilko76/BFVR-Battlefield-1942-VR-Mod
