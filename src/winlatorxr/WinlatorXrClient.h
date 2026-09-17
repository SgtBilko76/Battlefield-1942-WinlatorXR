#pragma once

#include "winlatorxr/WinlatorXrProtocol.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace bfvr::winlatorxr
{

// Reads BFVR_WINLATORXR and the Wine/Winlator markers of the current process.
[[nodiscard]] bool DetectWinlatorXrEnvironment() noexcept;

struct ReceivedState
{
    InputState input;
    // GetTickCount64() when the packet arrived; 0 before the first packet.
    std::uint64_t receivedAtMs = 0;
    std::uint64_t packetCount = 0;
};

// Owns the XrAPI UDP sockets and a receive thread. Thread-safe.
class Client
{
public:
    using LogCallback = void (*)(const wchar_t* message);

    Client();
    ~Client();

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    // Writes the WinlatorXR marker files, binds the receive port and announces
    // the game. Returns false if no port could be bound.
    bool Start(LogCallback log);
    void Stop();

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] ReceivedState Latest() const;

    // Sends haptics, display mode and FOV (0/0 keeps the headset's native FOV).
    void SendState(
        float leftHaptics,
        float rightHaptics,
        DisplayMode displayMode,
        StereoLayout layout,
        float fovHorizontalDegrees = 0.0F,
        float fovVerticalDegrees = 0.0F);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace bfvr::winlatorxr
