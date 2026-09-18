@echo off
cd /d D:\BF1942\BFVR
set BFVR_DIAGNOSTICS=off
rem WinlatorXR options: HUD size per eye (0.3-1.0), alternate-eye rendering (1 on, 0 side by side)
set BFVR_WINLATORXR_HUD_SCALE=0.36
set BFVR_WINLATORXR_AER=0
rem Field of view in degrees for rendering and display (empty = headset default)
set BFVR_WINLATORXR_FOV=110
rem DXVK options (anisotropic filtering); DXVK writes only warnings to vr-logs
set DXVK_CONFIG_FILE=D:\BF1942\dxvk.conf
set DXVK_LOG_LEVEL=warn
set DXVK_LOG_PATH=D:\BF1942\vr-logs
if not exist D:\BF1942\vr-logs mkdir D:\BF1942\vr-logs
echo BFVR-VR-RoadToRome started %DATE% %TIME% > D:\BF1942\vr-logs\result.txt
rem No debugger here: BFVR skips its CreateDevice hook when one is attached.
D:\BF1942\BFVR\BFVR.exe --d3d8to9-observer-probe --d3d8-openxr-presentation-probe --weapon-motion-probe --run-until-stopped -- +game XPack1 > D:\BF1942\vr-logs\bfvr-output.txt 2>&1
echo exit %ERRORLEVEL% at %TIME% >> D:\BF1942\vr-logs\result.txt
