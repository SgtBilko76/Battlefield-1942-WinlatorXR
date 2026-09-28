@echo off
rem Copies the BFVR shortcuts (Winlator\*.desktop next to this file) into this
rem container's desktop so they appear in WinlatorXR's Shortcuts list.
for %%S in (BFVR-VR BFVR-VR-RoadToRome BFVR-VR-SecretWeapons) do if exist "%~dp0Winlator\%%S.desktop" copy /Y "%~dp0Winlator\%%S.desktop" "%USERPROFILE%\Desktop\%%S.desktop"
echo BFVR shortcuts installed. Close this window and open the Shortcuts tab.
