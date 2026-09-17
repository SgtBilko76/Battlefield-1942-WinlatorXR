#include "winlatorxr/WinlatorXrProtocol.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace bfvr::winlatorxr
{
namespace
{
constexpr std::size_t kFloatCount = 28;
constexpr std::size_t kButtonCount = 19;
constexpr std::size_t kExtraCount = 9;
constexpr float kDefaultIpd = 0.064F;
constexpr float kDegreesToRadians = 3.14159265358979F / 180.0F;

std::vector<std::string_view> SplitTokens(std::string_view text)
{
    std::vector<std::string_view> tokens;
    std::size_t index = 0;
    while (index < text.size())
    {
        while (index < text.size() &&
            (text[index] == ' ' || text[index] == '\t' ||
             text[index] == '\r' || text[index] == '\n'))
        {
            ++index;
        }
        const std::size_t start = index;
        while (index < text.size() &&
            text[index] != ' ' && text[index] != '\t' &&
            text[index] != '\r' && text[index] != '\n')
        {
            ++index;
        }
        if (index > start)
        {
            tokens.push_back(text.substr(start, index - start));
        }
    }
    return tokens;
}

bool ParseFloat(std::string_view token, float& value)
{
    if (token.empty() || token.size() > 63)
    {
        return false;
    }
    char buffer[64] = {};
    std::copy(token.begin(), token.end(), buffer);
    char* end = nullptr;
    const float parsed = std::strtof(buffer, &end);
    if (end != buffer + token.size() || !std::isfinite(parsed))
    {
        return false;
    }
    value = parsed;
    return true;
}

bool ParseInt(std::string_view token, int& value)
{
    if (token.empty() || token.size() > 15)
    {
        return false;
    }
    char buffer[16] = {};
    std::copy(token.begin(), token.end(), buffer);
    char* end = nullptr;
    const long parsed = std::strtol(buffer, &end, 10);
    if (end != buffer + token.size())
    {
        return false;
    }
    value = static_cast<int>(parsed);
    return true;
}

float QuaternionLengthSquared(float x, float y, float z, float w)
{
    return x * x + y * y + z * z + w * w;
}

bool NormalizeQuaternion(float& x, float& y, float& z, float& w)
{
    const float lengthSquared = QuaternionLengthSquared(x, y, z, w);
    if (!std::isfinite(lengthSquared) || lengthSquared < 0.25F ||
        lengthSquared > 2.25F)
    {
        return false;
    }
    // q and -q are the same rotation; keep w >= 0 so code that extracts
    // yaw or compares orientations sees one canonical form.
    const float inverse = (w < 0.0F ? -1.0F : 1.0F) / std::sqrt(lengthSquared);
    x *= inverse;
    y *= inverse;
    z *= inverse;
    w *= inverse;
    return true;
}

// XrAPI's aim orientation is the OpenXR aim pose rolled 180 degrees about its
// own forward axis (+X and +Y inverted). Measured on Quest 3 with WinlatorXR
// cats-27: for both hands inverse(grip) * xrapiAim was constant at
// (0.04, +-0.50, +-0.87, 0.02), i.e. rotX(-60 deg) * rotZ(180 deg).
void UndoXrApiAimRoll(float& x, float& y, float& z, float& w)
{
    // q * (0, 0, 1, 0)
    const float fixedX = y;
    const float fixedY = -x;
    const float fixedZ = w;
    const float fixedW = -z;
    x = fixedX;
    y = fixedY;
    z = fixedZ;
    w = fixedW;
}

// Grip orientation = OpenXR aim orientation * rotX(+60 deg), measured above.
void GripFromAim(float& x, float& y, float& z, float& w)
{
    constexpr float kHalfSin = 0.5F;       // sin(30 deg)
    constexpr float kHalfCos = 0.8660254F; // cos(30 deg)
    const float gripX = w * kHalfSin + x * kHalfCos;
    const float gripY = y * kHalfCos + z * kHalfSin;
    const float gripZ = z * kHalfCos - y * kHalfSin;
    const float gripW = w * kHalfCos - x * kHalfSin;
    x = gripX;
    y = gripY;
    z = gripZ;
    w = gripW;
}

bool IsFinitePosition(float x, float y, float z)
{
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

void SetPose(
    shared::SharedPresentationPose& pose,
    float qx,
    float qy,
    float qz,
    float qw,
    float x,
    float y,
    float z)
{
    pose.orientationX = qx;
    pose.orientationY = qy;
    pose.orientationZ = qz;
    pose.orientationW = qw;
    pose.positionX = x;
    pose.positionY = y;
    pose.positionZ = z;
}

void BuildHand(
    const HandState& hand,
    bool gripOrientationAvailable,
    bool trigger,
    bool squeeze,
    bool primary,
    bool secondary,
    bool menu,
    bool thumbstick,
    shared::SharedControllerHandSample& sample)
{
    sample = {};
    sample.flags = shared::kControllerHandFlagTriggerActive |
        shared::kControllerHandFlagSqueezeActive |
        shared::kControllerHandFlagThumbstickActive;

    float aimX = hand.qx;
    float aimY = hand.qy;
    float aimZ = hand.qz;
    float aimW = hand.qw;
    const bool positionValid = IsFinitePosition(hand.posX, hand.posY, hand.posZ);
    if (positionValid && NormalizeQuaternion(aimX, aimY, aimZ, aimW))
    {
        UndoXrApiAimRoll(aimX, aimY, aimZ, aimW);
        NormalizeQuaternion(aimX, aimY, aimZ, aimW);
        SetPose(sample.aimPose, aimX, aimY, aimZ, aimW, hand.posX, hand.posY, hand.posZ);
        sample.flags |= shared::kControllerHandFlagAimActive |
            shared::kControllerHandFlagAimPositionValid |
            shared::kControllerHandFlagAimOrientationValid |
            shared::kControllerHandFlagAimPositionTracked |
            shared::kControllerHandFlagAimOrientationTracked;

        // XrAPI carries no separate grip position; the aim position is the
        // closest available approximation. The grip orientation is exact on
        // protocol 0.5 and derived from the aim orientation before that.
        float gripX = aimX;
        float gripY = aimY;
        float gripZ = aimZ;
        float gripW = aimW;
        GripFromAim(gripX, gripY, gripZ, gripW);
        NormalizeQuaternion(gripX, gripY, gripZ, gripW);
        if (gripOrientationAvailable)
        {
            float x = hand.gripQx;
            float y = hand.gripQy;
            float z = hand.gripQz;
            float w = hand.gripQw;
            if (NormalizeQuaternion(x, y, z, w))
            {
                gripX = x;
                gripY = y;
                gripZ = z;
                gripW = w;
            }
        }
        SetPose(sample.gripPose, gripX, gripY, gripZ, gripW, hand.posX, hand.posY, hand.posZ);
        sample.flags |= shared::kControllerHandFlagGripActive |
            shared::kControllerHandFlagGripPositionValid |
            shared::kControllerHandFlagGripOrientationValid |
            shared::kControllerHandFlagGripPositionTracked |
            shared::kControllerHandFlagGripOrientationTracked;
    }

    sample.buttons =
        (primary ? shared::kControllerHandButtonPrimary : 0) |
        (secondary ? shared::kControllerHandButtonSecondary : 0) |
        (menu ? shared::kControllerHandButtonMenu : 0) |
        (thumbstick ? shared::kControllerHandButtonThumbstick : 0);
    sample.triggerValue = trigger ? 1.0F : 0.0F;
    sample.squeezeValue = squeeze ? 1.0F : 0.0F;
    sample.thumbstickX = std::clamp(
        std::isfinite(hand.thumbX) ? hand.thumbX : 0.0F, -1.0F, 1.0F);
    sample.thumbstickY = std::clamp(
        std::isfinite(hand.thumbY) ? hand.thumbY : 0.0F, -1.0F, 1.0F);
}

void StartPulse(
    std::int64_t& until,
    float& amplitude,
    std::int64_t now,
    std::int64_t durationNanoseconds,
    float strength) noexcept
{
    const std::int64_t end = now + durationNanoseconds;
    if (now >= until || strength >= amplitude)
    {
        amplitude = strength;
    }
    until = std::max(until, end);
}
} // namespace

bool ParsePacket(std::string_view text, InputState& state)
{
    state = {};
    const std::vector<std::string_view> tokens = SplitTokens(text);
    // client id, 28 floats, frame id, button string
    if (tokens.size() < 1 + kFloatCount + 2)
    {
        return false;
    }

    std::array<float, kFloatCount> values = {};
    for (std::size_t index = 0; index < kFloatCount; ++index)
    {
        if (!ParseFloat(tokens[1 + index], values[index]))
        {
            return false;
        }
    }
    int frameId = -1;
    if (!ParseInt(tokens[1 + kFloatCount], frameId))
    {
        return false;
    }
    const std::string_view buttonText = tokens[2 + kFloatCount];
    if (buttonText.size() < kButtonCount)
    {
        return false;
    }

    InputState parsed = {};
    parsed.left = {values[0], values[1], values[2], values[3], values[4], values[5],
        values[6], values[7], values[8]};
    parsed.right = {values[9], values[10], values[11], values[12], values[13], values[14],
        values[15], values[16], values[17]};
    parsed.headQx = values[18];
    parsed.headQy = values[19];
    parsed.headQz = values[20];
    parsed.headQw = values[21];
    parsed.headX = values[22];
    parsed.headY = values[23];
    parsed.headZ = values[24];
    parsed.ipd = values[25];
    parsed.fovHorizontalDegrees = values[26];
    parsed.fovVerticalDegrees = values[27];
    parsed.frameId = frameId;

    const auto pressed = [&](std::size_t index) { return buttonText[index] == 'T'; };
    Buttons& buttons = parsed.buttons;
    buttons.leftGrip = pressed(0);
    buttons.leftMenu = pressed(1);
    buttons.leftThumbstick = pressed(2);
    buttons.leftThumbLeft = pressed(3);
    buttons.leftThumbRight = pressed(4);
    buttons.leftThumbUp = pressed(5);
    buttons.leftThumbDown = pressed(6);
    buttons.leftTrigger = pressed(7);
    buttons.buttonX = pressed(8);
    buttons.buttonY = pressed(9);
    buttons.buttonA = pressed(10);
    buttons.buttonB = pressed(11);
    buttons.rightGrip = pressed(12);
    buttons.rightThumbstick = pressed(13);
    buttons.rightThumbLeft = pressed(14);
    buttons.rightThumbRight = pressed(15);
    buttons.rightThumbUp = pressed(16);
    buttons.rightThumbDown = pressed(17);
    buttons.rightTrigger = pressed(18);

    // Protocol 0.5: head altitude, then left and right grip quaternions. A
    // trailing non-numeric flags token (protocol 0.3+) ends the extras.
    std::array<float, kExtraCount> extras = {};
    std::size_t extraCount = 0;
    for (std::size_t index = 3 + kFloatCount;
         index < tokens.size() && extraCount < kExtraCount;
         ++index)
    {
        if (!ParseFloat(tokens[index], extras[extraCount]))
        {
            break;
        }
        ++extraCount;
    }
    if (extraCount >= 1)
    {
        parsed.hasHeadAltitude = true;
        parsed.headAltitude = extras[0];
    }
    if (extraCount >= kExtraCount)
    {
        parsed.left.gripQx = extras[1];
        parsed.left.gripQy = extras[2];
        parsed.left.gripQz = extras[3];
        parsed.left.gripQw = extras[4];
        parsed.right.gripQx = extras[5];
        parsed.right.gripQy = extras[6];
        parsed.right.gripQz = extras[7];
        parsed.right.gripQw = extras[8];
        parsed.hasGripOrientation =
            QuaternionLengthSquared(extras[1], extras[2], extras[3], extras[4]) > 0.5F &&
            QuaternionLengthSquared(extras[5], extras[6], extras[7], extras[8]) > 0.5F;
    }

    parsed.valid = true;
    state = parsed;
    return true;
}

std::string FormatStatePacket(
    float leftHaptics,
    float rightHaptics,
    DisplayMode displayMode,
    StereoLayout layout,
    float fovHorizontalDegrees,
    float fovVerticalDegrees)
{
    char buffer[128] = {};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%g %g %d %d %g %g",
        static_cast<double>(std::clamp(leftHaptics, 0.0F, 1.0F)),
        static_cast<double>(std::clamp(rightHaptics, 0.0F, 1.0F)),
        static_cast<int>(displayMode),
        static_cast<int>(layout),
        static_cast<double>(fovHorizontalDegrees),
        static_cast<double>(fovVerticalDegrees));
    return buffer;
}

bool IsWinlatorXrEnvironment(const EnvironmentProbe& probe) noexcept
{
    if (probe.overrideValue == L"0")
    {
        return false;
    }
    if (probe.overrideValue == L"1")
    {
        return true;
    }
    return probe.runningUnderWine &&
        (probe.hasAndroidSysvShmServer || probe.hasEvshimSharedMemory);
}

bool BuildRenderViews(const InputState& state, RenderViews& views)
{
    views = {};
    if (!state.valid)
    {
        return false;
    }
    float qx = state.headQx;
    float qy = state.headQy;
    float qz = state.headQz;
    float qw = state.headQw;
    if (!NormalizeQuaternion(qx, qy, qz, qw) ||
        !IsFinitePosition(state.headX, state.headY, state.headZ))
    {
        return false;
    }
    SetPose(views.head, qx, qy, qz, qw, state.headX, state.headY, state.headZ);

    float ipd = state.ipd;
    if (!std::isfinite(ipd) || ipd < 0.040F || ipd > 0.090F)
    {
        ipd = kDefaultIpd;
    }
    // The head's +X axis in reference space.
    const float rightX = 1.0F - 2.0F * (qy * qy + qz * qz);
    const float rightY = 2.0F * (qx * qy + qw * qz);
    const float rightZ = 2.0F * (qx * qz - qw * qy);
    const float halfIpd = ipd * 0.5F;

    float fovH = state.fovHorizontalDegrees;
    float fovV = state.fovVerticalDegrees;
    if (!std::isfinite(fovH) || fovH < 40.0F || fovH > 150.0F)
    {
        fovH = 90.0F;
    }
    if (!std::isfinite(fovV) || fovV < 40.0F || fovV > 150.0F)
    {
        fovV = 90.0F;
    }
    const float halfH = fovH * 0.5F * kDegreesToRadians;
    const float halfV = fovV * 0.5F * kDegreesToRadians;

    for (std::size_t eye = 0; eye < views.eyes.size(); ++eye)
    {
        const float sign = eye == 0 ? -1.0F : 1.0F;
        SetPose(
            views.eyes[eye].pose,
            qx,
            qy,
            qz,
            qw,
            state.headX + sign * rightX * halfIpd,
            state.headY + sign * rightY * halfIpd,
            state.headZ + sign * rightZ * halfIpd);
        views.eyes[eye].fov.angleLeft = -halfH;
        views.eyes[eye].fov.angleRight = halfH;
        views.eyes[eye].fov.angleUp = halfV;
        views.eyes[eye].fov.angleDown = -halfV;
    }

    if (state.hasHeadAltitude && std::isfinite(state.headAltitude) &&
        state.headAltitude >= 0.20F && state.headAltitude <= 3.0F)
    {
        views.standingHeightValid = true;
        views.standingHeightMeters = state.headAltitude;
    }
    return true;
}

void BuildControllerSample(
    const InputState& state,
    std::int64_t predictedDisplayTime,
    shared::SharedControllerSample& sample)
{
    sample = {};
    sample.predictedDisplayTime = predictedDisplayTime;
    if (!state.valid)
    {
        return;
    }
    sample.flags = shared::kControllerSampleFlagSessionFocused;
    const Buttons& buttons = state.buttons;
    BuildHand(
        state.left,
        state.hasGripOrientation,
        buttons.leftTrigger,
        buttons.leftGrip,
        buttons.buttonX,
        buttons.buttonY,
        buttons.leftMenu,
        buttons.leftThumbstick,
        sample.hands[0]);
    BuildHand(
        state.right,
        state.hasGripOrientation,
        buttons.rightTrigger,
        buttons.rightGrip,
        buttons.buttonA,
        buttons.buttonB,
        false,
        buttons.rightThumbstick,
        sample.hands[1]);
}

std::array<float, 2> UpdateHapticPulses(
    HapticPulseState& state,
    const HapticEvents& newEvents,
    std::int64_t now) noexcept
{
    constexpr std::int64_t kMillisecond = 1'000'000;
    if (newEvents.shotRight != 0)
    {
        StartPulse(state.rightUntil, state.rightAmplitude, now, 45 * kMillisecond, 0.6F);
    }
    if (newEvents.shotBoth != 0)
    {
        StartPulse(state.leftUntil, state.leftAmplitude, now, 45 * kMillisecond, 0.6F);
        StartPulse(state.rightUntil, state.rightAmplitude, now, 45 * kMillisecond, 0.6F);
    }
    if (newEvents.death != 0)
    {
        StartPulse(state.leftUntil, state.leftAmplitude, now, 300 * kMillisecond, 1.0F);
        StartPulse(state.rightUntil, state.rightAmplitude, now, 300 * kMillisecond, 1.0F);
    }
    if (newEvents.menuHover != 0)
    {
        StartPulse(state.rightUntil, state.rightAmplitude, now, 15 * kMillisecond, 0.2F);
    }
    return {
        now < state.leftUntil ? state.leftAmplitude : 0.0F,
        now < state.rightUntil ? state.rightAmplitude : 0.0F};
}

} // namespace bfvr::winlatorxr
