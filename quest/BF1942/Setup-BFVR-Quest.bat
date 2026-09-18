@echo off
rem BFVR Quest Beta: one-time setup of a MoonGamers Battlefield 1942 folder
rem for WinlatorXR. Run it inside the WinlatorXR container from the game
rem folder (D:\BF1942). Every changed file is backed up once as *.bfvr-backup.
rem The resolution below must match the container's screen size.
setlocal EnableExtensions
set SCREEN_W=3120
set SCREEN_H=1430
cd /d "%~dp0"
if not exist BF1942.exe goto :wrongfolder
if not exist BFVR\BFVRD3D8To9.dll goto :nopayload

echo BFVR Quest setup in %CD%

rem BFVR's own D3D8 translator replaces the package's D3D8.dll (dgVoodoo
rem crashes under Wine, and a DXVK D3D8 cannot create its device there).
call :backup D3D8.dll
copy /Y BFVR\BFVRD3D8To9.dll D3D8.dll >nul
rem A DXVK d3d9.dll in the game folder would replace WinlatorXR's own.
if exist d3d9.dll ren d3d9.dll d3d9.dll.bfvr-disabled

rem The intro movies only show black under WinlatorXR. The expansions' own
rem intro and legal clips go too; their menu backgrounds stay.
if exist Movies if not exist Movies.disabled ren Movies Movies.disabled
for %%M in (Mods\XPack1\Movies\legal.bik Mods\XPack2\Movies\Intro.bik Mods\XPack2\Movies\Legal.bik Mods\XPack2\Movies\LegalF.bik) do if exist "%%M" ren "%%M" "%%~nxM.bfvr-disabled"

set SETTINGS=Mods\bf1942\Settings
rem Normal object detail distance (the package uses 5x, far too heavy).
call :appendsetting %SETTINGS%\VideoDefault.con "renderer.globalLodRadiusScaleFactor 1"

for %%P in (Custom Default) do call :profile %SETTINGS%\Profiles\%%P
for %%F in (%SETTINGS%\Default\Video*.con) do call :appendsetting "%%F" "game.setGameDisplayMode %SCREEN_W% %SCREEN_H% 32 60"

echo.
echo Done. Start BFVR-VR.bat, BFVR-VR-RoadToRome.bat or BFVR-VR-SecretWeapons.bat
echo (or run Install-BFVR-Shortcut.bat once for shortcuts).
goto :end

:profile
if not exist "%~1" goto :eof
rem Hardware 3D sound crashes BF1942 with WinlatorXR's DirectSound.
call :appendsetting "%~1\Sound.con" "game.setHardware 0"
rem Settings that run well on Quest 3 (keep detail textures on: turning
rem them off crashes map loading there).
call :backup "%~1\Video.con"
(
echo rem *** Generated ***
echo.
echo game.setGameDisplayMode %SCREEN_W% %SCREEN_H% 32 60
echo game.setDetailTexture 1
echo game.setShadows 0
echo game.setEnvironmentMapping 0
echo game.setGraphicsQuality 3
echo game.setLightmaps 1
echo game.setRenderWhenSpawnMenu 0
echo game.setMenuViewdistance 60
echo game.setEffectsQuality 1
echo game.setPerformance 0
) > "%~1\Video.con"
call :appendsetting "%~1\VideoCustom.con" "game.setGameDisplayMode %SCREEN_W% %SCREEN_H% 32 60"
goto :eof

rem :appendsetting file line - BF1942 runs .con files top to bottom, so the
rem appended line overrides an earlier value of the same setting.
:appendsetting
if not exist "%~1" goto :eof
call :backup "%~1"
>> "%~1" echo.
>> "%~1" echo.%~2
goto :eof

:backup
if exist "%~1" if not exist "%~1.bfvr-backup" copy /Y "%~1" "%~1.bfvr-backup" >nul
goto :eof

:wrongfolder
echo Put this file into the Battlefield 1942 folder (next to BF1942.exe) and run it there.
goto :end

:nopayload
echo The BFVR folder is missing. Copy the complete BF1942 folder from the BFVR Quest Beta zip first.
goto :end

:end
endlocal
