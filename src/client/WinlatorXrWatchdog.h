#pragma once

namespace bfvr
{

using WinlatorXrWatchdogLogCallback = void (*)(const wchar_t* message);

// Headset diagnostics for WinlatorXR, where no debugger can be attached
// without disabling BFVR's hooks. Once frames flow, a stall of the thread
// that composes them is logged with its instruction pointer and the code
// addresses found on its stack; access violations are logged the same way.
// Idempotent.
void StartWinlatorXrWatchdog(WinlatorXrWatchdogLogCallback log) noexcept;

// Called by the presentation companion for every composed frame.
void NoteWinlatorXrFrameComposed() noexcept;

} // namespace bfvr
