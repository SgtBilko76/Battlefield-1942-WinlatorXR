@echo off
setlocal EnableExtensions
rem ===========================================================================
rem  BFVR (Battlefield 1942 VR) - WinlatorXR installer for Meta Quest
rem  Run this INSIDE your WinlatorXR container (wine cmd /c install.cmd).
rem  It installs BFVR onto your EXISTING Battlefield 1942 folder. It does not
rem  contain the game; use your own copy of the MoonGamers package.
rem ===========================================================================
set "SRC=%~dp0"

echo.
echo  BFVR - Battlefield 1942 VR for WinlatorXR
echo  -----------------------------------------
echo.
rem BFVR_GAME_DIR and BFVR_CONTAINER skip the questions (unattended installs).
set "GAME=D:\BF1942"
if defined BFVR_GAME_DIR (set "GAME=%BFVR_GAME_DIR%") else set /p "GAME=Path to your Battlefield 1942 folder [%GAME%]: "
if "%GAME%"=="" set "GAME=D:\BF1942"
if "%GAME:~-1%"=="\" set "GAME=%GAME:~0,-1%"
if not exist "%GAME%\BF1942.exe" (
  echo.
  echo  ERROR: "%GAME%\BF1942.exe" was not found.
  echo  Copy the complete Battlefield 1942 folder there first, then run this
  echo  installer again.
  echo.
  pause
  exit /b 1
)
set "CONTAINER=3"
if defined BFVR_CONTAINER (set "CONTAINER=%BFVR_CONTAINER%") else set /p "CONTAINER=WinlatorXR container number for the shortcuts [%CONTAINER%]: "
if "%CONTAINER%"=="" set "CONTAINER=3"

echo.
echo  Installing BFVR into "%GAME%" ...
xcopy /E /I /Y "%SRC%files\BF1942" "%GAME%" >nul
if errorlevel 1 (
  echo  ERROR: failed to copy the BFVR files.
  pause
  exit /b 1
)
rem Keep the player's VR settings when updating.
if not exist "%GAME%\BFVR\UserConfig.txt" copy /Y "%SRC%files\UserConfig.txt" "%GAME%\BFVR\UserConfig.txt" >nul

echo  Applying the Quest game settings ...
call "%GAME%\Setup-BFVR-Quest.bat"

echo  Adding the WinlatorXR shortcuts ...
set "DESKTOP=%USERPROFILE%\Desktop"
if not exist "%DESKTOP%" mkdir "%DESKTOP%"
call :shortcut BFVR-VR
call :shortcut BFVR-VR-RoadToRome
call :shortcut BFVR-VR-SecretWeapons

echo.
echo  ============================================================
echo   BFVR installed. REMAINING STEPS in the WinlatorXR UI:
echo.
echo   1^) Container %CONTAINER% settings:
echo        - Screen size : 1792x1624
echo        - DX wrapper  : DXVK, Async on
echo        - Drive D:    : /sdcard/Download
echo        - XR controller mapping: numpad keys, see README_INSTALL.txt
echo        - XR settings : CPU level at the highest value
echo   2^) Close this window and open the Shortcuts tab:
echo        BFVR-VR, BFVR-VR-RoadToRome, BFVR-VR-SecretWeapons
echo.
echo   See README_INSTALL.txt for the full walkthrough.
echo  ============================================================
echo.
if not defined BFVR_NO_PAUSE pause
endlocal
exit /b 0

:shortcut
> "%DESKTOP%\%1.desktop" (
  echo [Desktop Entry]
  echo Name=%1
  echo Exec=wine C:\\\\windows\\\\system32\\\\cmd.exe
  echo Type=Application
  echo StartupWMClass=BF1942.exe
  echo.
  echo [Extra Data]
  echo container_id=%CONTAINER%
  echo screenSize=1792x1624
  echo execArgs=/c %GAME%\%1.bat
)
goto :eof
