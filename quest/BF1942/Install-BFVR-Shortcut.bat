@echo off
rem Copies the BFVR shortcuts into this container's desktop so they appear in WinlatorXR's Shortcuts list.
for %%S in (BFVR-VR BFVR-VR-RoadToRome BFVR-VR-SecretWeapons) do if exist D:\Winlator\%%S.desktop copy /Y D:\Winlator\%%S.desktop C:\users\xuser\Desktop\%%S.desktop
echo BFVR shortcuts installed. Close this window and open the Shortcuts tab.
