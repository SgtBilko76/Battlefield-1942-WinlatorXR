#pragma once

namespace bfvr
{

using WinlatorXrMouseFilterLogCallback = void (*)(const wchar_t* message);

// Under WinlatorXR the headset controllers also drive an emulated desktop
// mouse. During VR gameplay that mouse turns BF1942's soldier behind BFVR's
// back, so its DirectInput data is dropped while blocking is enabled.
// Installs only when WinlatorXR is detected; call outside DllMain before the
// game creates its DirectInput devices.
void InstallWinlatorXrDesktopMouseFilter(WinlatorXrMouseFilterLogCallback log) noexcept;

// Set by the presentation companion: true while a head-tracked gameplay frame
// is shown, false in menus (where the WinlatorXR pointer is the mouse).
// The same flag drops Enter from the DirectInput keyboard: WinlatorXR can
// type Enter for a controller button, which opens BF1942's spawn screen.
// BFVR_WINLATORXR_ENTER_FILTER=0 turns that off.
void SetWinlatorXrDesktopMouseBlocked(bool blocked) noexcept;

// Lets the next Enter through for a moment; called right before BFVR sends
// Enter itself (Quick Menu Deploy).
void AllowWinlatorXrOwnEnterKey() noexcept;

} // namespace bfvr
