#pragma once

#include "presenter/SharedPresentationProtocol.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

// Pure, socket-free parts of the WinlatorXR XrAPI client.
//
// WinlatorXR runs Windows applications under Wine on standalone Android
// headsets. Its XrAPI is a small text protocol over localhost UDP: the Android
// side sends head/controller state to the game (port 7872, fallback 7873) and
// the game answers with haptics and display mode (port 7278). A frame is shown
// by composing it into the game window; screen pixel (0,0) carries the pose
// index the frame was rendered with.
namespace bfvr::winlatorxr
{

inline constexpr std::uint16_t kReceivePort = 7872;
inline constexpr std::uint16_t kReceiveFallbackPort = 7873;
inline constexpr std::uint16_t kSendPort = 7278;
inline constexpr std::string_view kRequestedProtocolVersion = "0.5";

// Values WinlatorXR accepts in the state packet sent by the game.
enum class DisplayMode : int
{
    Off = 0,
    HeadTracked = 1,
    VirtualScreen = 2,
};

enum class StereoLayout : int
{
    Mono = 0,
    SideBySide = 1,
    AlternateEye = 2,
};

struct HandState
{
    // OpenXR aim pose (same reference space and conventions as the head).
    float qx = 0.0F;
    float qy = 0.0F;
    float qz = 0.0F;
    float qw = 1.0F;
    float thumbX = 0.0F;
    float thumbY = 0.0F;
    float posX = 0.0F;
    float posY = 0.0F;
    float posZ = 0.0F;
    // OpenXR grip orientation, protocol 0.5 and newer.
    float gripQx = 0.0F;
    float gripQy = 0.0F;
    float gripQz = 0.0F;
    float gripQw = 1.0F;
};

struct Buttons
{
    bool leftGrip = false;
    bool leftMenu = false;
    bool leftThumbstick = false;
    bool leftThumbLeft = false;
    bool leftThumbRight = false;
    bool leftThumbUp = false;
    bool leftThumbDown = false;
    bool leftTrigger = false;
    bool buttonX = false;
    bool buttonY = false;
    bool buttonA = false;
    bool buttonB = false;
    bool rightGrip = false;
    bool rightThumbstick = false;
    bool rightThumbLeft = false;
    bool rightThumbRight = false;
    bool rightThumbUp = false;
    bool rightThumbDown = false;
    bool rightTrigger = false;
};

struct InputState
{
    bool valid = false;
    int frameId = -1;
    HandState left;
    HandState right;
    // Head orientation (left-eye view orientation) and the midpoint between
    // both eye views, in OpenXR convention: +X right, +Y up, -Z forward, metres.
    float headQx = 0.0F;
    float headQy = 0.0F;
    float headQz = 0.0F;
    float headQw = 1.0F;
    float headX = 0.0F;
    float headY = 0.0F;
    float headZ = 0.0F;
    float ipd = 0.064F;
    // Symmetric field of view in degrees.
    float fovHorizontalDegrees = 90.0F;
    float fovVerticalDegrees = 90.0F;
    Buttons buttons;
    // Protocol 0.5 extras; absent on older WinlatorXR builds.
    bool hasHeadAltitude = false;
    float headAltitude = 0.0F;
    bool hasGripOrientation = false;
};

// Parses one XrAPI packet. Returns false (and leaves state.valid false) for a
// malformed packet. Protocol 0.2..0.5 packets are accepted.
[[nodiscard]] bool ParsePacket(std::string_view text, InputState& state);

// Formats the game-to-WinlatorXR state packet.
[[nodiscard]] std::string FormatStatePacket(
    float leftHaptics,
    float rightHaptics,
    DisplayMode displayMode,
    StereoLayout layout,
    float fovHorizontalDegrees,
    float fovVerticalDegrees);

struct EnvironmentProbe
{
    // Value of BFVR_WINLATORXR: empty when unset, "0" forces off, "1" on.
    std::wstring_view overrideValue;
    // Wine exports wine_get_version from ntdll; native Windows does not.
    bool runningUnderWine = false;
    // Winlator-family launchers export these Unix variables to Wine.
    bool hasAndroidSysvShmServer = false;
    bool hasEvshimSharedMemory = false;
};

[[nodiscard]] bool IsWinlatorXrEnvironment(const EnvironmentProbe& probe) noexcept;

struct RenderViews
{
    shared::SharedPresentationPose head;
    std::array<shared::SharedPresentationView, 2> eyes;
    bool standingHeightValid = false;
    float standingHeightMeters = 0.0F;
};

// Builds BFVR's head pose and per-eye views from one packet. Returns false
// when the packet does not describe a finite, unit-length head orientation.
[[nodiscard]] bool BuildRenderViews(const InputState& state, RenderViews& views);

// Converts one packet to BFVR's normalized controller sample (hands[0] is the
// left hand). predictedDisplayTime must match the render request.
void BuildControllerSample(
    const InputState& state,
    std::int64_t predictedDisplayTime,
    shared::SharedControllerSample& sample);

// Converts accumulated haptic event counters into a pulse amplitude.
struct HapticPulseState
{
    std::int64_t leftUntil = 0;
    std::int64_t rightUntil = 0;
    float leftAmplitude = 0.0F;
    float rightAmplitude = 0.0F;
};

struct HapticEvents
{
    std::uint32_t shotRight = 0;
    std::uint32_t shotBoth = 0;
    std::uint32_t death = 0;
    std::uint32_t menuHover = 0;
};

// now is in nanoseconds. Returns the amplitudes to send for this frame.
[[nodiscard]] std::array<float, 2> UpdateHapticPulses(
    HapticPulseState& state,
    const HapticEvents& newEvents,
    std::int64_t now) noexcept;

} // namespace bfvr::winlatorxr
