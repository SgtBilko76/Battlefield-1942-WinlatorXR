#include "winlatorxr/WinlatorXrProtocol.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace
{
bool Expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::fprintf(stderr, "WinlatorXR protocol test failed: %s\n", message);
    }
    return condition;
}

bool Near(float value, float expected, float tolerance = 0.0005F)
{
    return std::fabs(value - expected) <= tolerance;
}

// Left hand, right hand, head, ipd, fov (28 values) as XrAPI orders them.
std::string BasePacket(const char* buttons)
{
    std::string packet = "client";
    // left: identity aim, thumb (0.25, -0.5), position (-0.2, 1.1, -0.3)
    packet += " 0 0 0 1 0.25 -0.5 -0.2 1.1 -0.3";
    // right: 90 degree yaw, thumb (1.5, 0), position (0.2, 1.0, -0.3)
    packet += " 0 0.70710678 0 0.70710678 1.5 0 0.2 1.0 -0.3";
    // head: identity at (0, 1.6, 0), ipd 0.064, fov 100 x 90
    packet += " 0 0 0 1 0 1.6 0 0.064 100 90";
    packet += " 42 ";
    packet += buttons;
    return packet;
}
} // namespace

int main()
{
    using namespace bfvr;
    using namespace bfvr::winlatorxr;
    bool passed = true;

    // Protocol 0.2 packet: no extras.
    InputState state;
    passed &= Expect(
        ParsePacket(BasePacket("FFFFFFFFFFFFFFFFFFF"), state) && state.valid,
        "a protocol 0.2 packet must parse");
    passed &= Expect(state.frameId == 42, "the frame id must be read");
    passed &= Expect(
        Near(state.left.thumbX, 0.25F) && Near(state.left.thumbY, -0.5F) &&
            Near(state.right.posX, 0.2F) && Near(state.headY, 1.6F) &&
            Near(state.fovHorizontalDegrees, 100.0F) &&
            Near(state.fovVerticalDegrees, 90.0F),
        "the 28 values must map in XrAPI order");
    passed &= Expect(
        !state.hasHeadAltitude && !state.hasGripOrientation,
        "protocol 0.2 packets carry no extras");

    // Button order is part of the wire format.
    passed &= Expect(
        ParsePacket(BasePacket("TFFFFFFTTFFTFFFFFFT"), state) &&
            state.buttons.leftGrip && state.buttons.leftTrigger &&
            state.buttons.buttonX && !state.buttons.buttonY &&
            state.buttons.buttonB && !state.buttons.buttonA &&
            state.buttons.rightTrigger && !state.buttons.rightGrip &&
            !state.buttons.leftMenu,
        "buttons must follow the XrAPI order");

    // Protocol 0.5 extras plus a trailing flags token.
    const std::string extended = BasePacket("FFFFFFFFFFFFFFFFFFF") +
        " 1.7 0 0 0 1 0 0.38268343 0 0.92387953 TF";
    passed &= Expect(
        ParsePacket(extended, state) && state.hasHeadAltitude &&
            Near(state.headAltitude, 1.7F) && state.hasGripOrientation &&
            Near(state.right.gripQy, 0.38268343F),
        "protocol 0.5 extras must be read before the flags token");

    // Malformed packets.
    passed &= Expect(!ParsePacket("", state) && !state.valid, "an empty packet is rejected");
    passed &= Expect(
        !ParsePacket("client 0 0 0 1", state),
        "a truncated packet is rejected");
    passed &= Expect(
        !ParsePacket(BasePacket("TF"), state),
        "a short button string is rejected");
    std::string badNumber = BasePacket("FFFFFFFFFFFFFFFFFFF");
    badNumber.replace(badNumber.find("0.064"), 5, "nope!");
    passed &= Expect(!ParsePacket(badNumber, state), "a non-numeric value is rejected");

    // Views.
    passed &= Expect(ParsePacket(extended, state), "the extended packet parses again");
    RenderViews views;
    passed &= Expect(BuildRenderViews(state, views), "views must build from a valid packet");
    passed &= Expect(
        Near(views.eyes[0].pose.positionX, -0.032F) &&
            Near(views.eyes[1].pose.positionX, 0.032F) &&
            Near(views.eyes[0].pose.positionY, 1.6F) &&
            Near(views.head.positionY, 1.6F),
        "identity head must offset eyes by half the IPD on +X");
    passed &= Expect(
        Near(views.eyes[0].fov.angleLeft, -0.872665F) &&
            Near(views.eyes[0].fov.angleRight, 0.872665F) &&
            Near(views.eyes[1].fov.angleUp, 0.785398F) &&
            Near(views.eyes[1].fov.angleDown, -0.785398F),
        "symmetric degrees must become OpenXR half-angles in radians");
    passed &= Expect(
        views.standingHeightValid && Near(views.standingHeightMeters, 1.7F),
        "head altitude must become the standing height");

    // A head turned 90 degrees left (+Y yaw) has its right axis on -Z.
    InputState turned = state;
    turned.headQx = 0.0F;
    turned.headQy = 0.70710678F;
    turned.headQz = 0.0F;
    turned.headQw = 0.70710678F;
    passed &= Expect(
        BuildRenderViews(turned, views) &&
            Near(views.eyes[0].pose.positionZ, 0.032F) &&
            Near(views.eyes[1].pose.positionZ, -0.032F) &&
            Near(views.eyes[1].pose.positionX, 0.0F),
        "eye offsets must follow the head orientation");

    InputState invalid = state;
    invalid.headQw = 0.0F;
    passed &= Expect(!BuildRenderViews(invalid, views), "a zero quaternion is rejected");
    InputState oddValues = state;
    oddValues.ipd = 0.5F;
    oddValues.fovHorizontalDegrees = 1000.0F;
    oddValues.hasHeadAltitude = false;
    passed &= Expect(
        BuildRenderViews(oddValues, views) &&
            Near(views.eyes[1].pose.positionX, 0.032F) &&
            Near(views.eyes[0].fov.angleRight, 0.785398F) &&
            !views.standingHeightValid,
        "implausible IPD/FOV fall back to defaults");

    // Controller sample.
    passed &= Expect(ParsePacket(extended.substr(0, extended.find("42 ")) +
            "42 TFTFFFFTTFFTTTFFFFT 1.7 0 0 0 1 0 0.38268343 0 0.92387953",
            state),
        "a controller packet parses");
    shared::SharedControllerSample sample;
    BuildControllerSample(state, 12345, sample);
    const auto& left = sample.hands[0];
    const auto& right = sample.hands[1];
    passed &= Expect(
        sample.predictedDisplayTime == 12345 &&
            (sample.flags & shared::kControllerSampleFlagSessionFocused) != 0,
        "the sample must be focused and carry the request time");
    passed &= Expect(
        (left.flags & shared::kControllerHandFlagAimPositionTracked) != 0 &&
            (left.flags & shared::kControllerHandFlagGripOrientationValid) != 0 &&
            (right.flags & shared::kControllerHandFlagAimOrientationTracked) != 0,
        "tracked aim and grip flags must be set");
    passed &= Expect(
        left.squeezeValue == 1.0F && right.triggerValue == 1.0F && right.squeezeValue == 1.0F,
        "trigger and grip buttons must become full analog values");
    passed &= Expect(
        left.triggerValue == 1.0F,
        "the left trigger is use in the Quest layout");
    passed &= Expect(
        (left.buttons & shared::kControllerHandButtonPrimary) != 0 &&
            (left.buttons & shared::kControllerHandButtonThumbstick) == 0 &&
            (left.buttons & shared::kControllerHandButtonSecondary) == 0 &&
            (right.buttons & shared::kControllerHandButtonSecondary) != 0 &&
            (right.buttons & shared::kControllerHandButtonPrimary) != 0 &&
            (right.buttons & shared::kControllerHandButtonThumbstick) != 0 &&
            (right.buttons & shared::kControllerHandButtonMenu) == 0,
        "X/Y and B keep their meaning; the left stick click holds the Quick Menu");
    InputState pressA = state;
    pressA.buttons.buttonA = true;
    pressA.buttons.leftTrigger = false;
    pressA.buttons.leftThumbstick = false;
    shared::SharedControllerSample useSample;
    BuildControllerSample(pressA, 12345, useSample);
    passed &= Expect(
        useSample.hands[0].triggerValue == 0.0F &&
            (useSample.hands[1].buttons & shared::kControllerHandButtonQuestA) != 0 &&
            (useSample.hands[1].buttons & shared::kControllerHandButtonPrimary) == 0,
        "A is reported on its own and does not open the Quick Menu");
    InputState leftTriggerOnly = state;
    leftTriggerOnly.buttons.rightGrip = false;
    leftTriggerOnly.buttons.buttonA = false;
    leftTriggerOnly.buttons.leftTrigger = true;
    shared::SharedControllerSample triggerSample;
    BuildControllerSample(leftTriggerOnly, 12345, triggerSample);
    passed &= Expect(
        triggerSample.hands[0].triggerValue == 1.0F &&
            triggerSample.hands[1].squeezeValue == 0.0F &&
            (triggerSample.hands[1].buttons & shared::kControllerHandButtonQuestA) == 0,
        "the left trigger is use and not secondary fire");
    passed &= Expect(
        Near(right.thumbstickX, 1.0F) && Near(left.thumbstickY, -0.5F),
        "thumbsticks must be clamped to [-1, 1]");
    // XrAPI aim (0, 0.7071, 0, 0.7071) rolled back by 180 degrees about Z:
    // q * (0, 0, 1, 0) = (y, -x, w, -z) = (0.7071, 0, 0.7071, 0).
    passed &= Expect(
        Near(right.gripPose.orientationY, 0.38268343F) &&
            Near(right.aimPose.orientationX, 0.70710678F) &&
            Near(right.aimPose.orientationZ, 0.70710678F) &&
            Near(right.aimPose.orientationW, 0.0F) &&
            Near(right.gripPose.positionX, 0.2F),
        "grip uses its own orientation, aim has the XrAPI roll removed");
    passed &= Expect(
        Near(left.aimPose.orientationZ, 1.0F) && Near(left.aimPose.orientationW, 0.0F),
        "identity XrAPI aim becomes a 180 degree roll");

    // Measured Quest 3 sample: inverse(grip) * corrected aim is rotX(-60).
    InputState measured = state;
    measured.right.qx = -0.086F;
    measured.right.qy = -0.016F;
    measured.right.qz = -0.968F;
    measured.right.qw = 0.236F;
    measured.right.gripQx = 0.480F;
    measured.right.gripQy = 0.156F;
    measured.right.gripQz = 0.183F;
    measured.right.gripQw = 0.843F;
    BuildControllerSample(measured, 1, sample);
    {
        const auto& aim = sample.hands[1].aimPose;
        const auto& grip = sample.hands[1].gripPose;
        // inverse(grip) * aim
        const float gx = -grip.orientationX, gy = -grip.orientationY, gz = -grip.orientationZ, gw = grip.orientationW;
        const float ax = aim.orientationX, ay = aim.orientationY, az = aim.orientationZ, aw = aim.orientationW;
        const float rx = gw * ax + gx * aw + gy * az - gz * ay;
        const float ry = gw * ay - gx * az + gy * aw + gz * ax;
        const float rz = gw * az + gx * ay - gy * ax + gz * aw;
        const float rw = gw * aw - gx * ax - gy * ay - gz * az;
        const float sign = rw < 0.0F ? -1.0F : 1.0F;
        passed &= Expect(
            Near(sign * rx, -0.5F, 0.03F) && Near(sign * ry, 0.0F, 0.05F) &&
                Near(sign * rz, 0.0F, 0.05F) && Near(sign * rw, 0.866F, 0.03F),
            "corrected aim is the grip pitched down by 60 degrees");
        passed &= Expect(aim.orientationW >= 0.0F && grip.orientationW >= 0.0F,
            "orientations use the w >= 0 hemisphere");
    }

    InputState noGrip = state;
    noGrip.hasGripOrientation = false;
    BuildControllerSample(noGrip, 1, sample);
    {
        // Without protocol 0.5 extras the grip is the corrected aim * rotX(+60).
        const auto& aim = sample.hands[1].aimPose;
        const auto& grip = sample.hands[1].gripPose;
        const float gx = -grip.orientationX, gy = -grip.orientationY, gz = -grip.orientationZ, gw = grip.orientationW;
        const float rx = gw * aim.orientationX + gx * aim.orientationW + gy * aim.orientationZ - gz * aim.orientationY;
        const float rw = gw * aim.orientationW - gx * aim.orientationX - gy * aim.orientationY - gz * aim.orientationZ;
        const float sign = rw < 0.0F ? -1.0F : 1.0F;
        passed &= Expect(
            Near(sign * rx, -0.5F, 0.01F) && Near(sign * rw, 0.866F, 0.01F),
            "grip orientation is derived from the corrected aim before protocol 0.5");
    }

    InputState lostHand = state;
    lostHand.left.qw = 0.0F;
    BuildControllerSample(lostHand, 1, sample);
    passed &= Expect(
        (sample.hands[0].flags & shared::kControllerHandFlagAimPositionValid) == 0 &&
            (sample.hands[0].flags & shared::kControllerHandFlagGripActive) == 0 &&
            (sample.hands[0].flags & shared::kControllerHandFlagTriggerActive) != 0,
        "an invalid hand pose must clear only its pose flags");

    BuildControllerSample(InputState{}, 7, sample);
    passed &= Expect(
        sample.flags == 0 && sample.predictedDisplayTime == 7,
        "an invalid packet must produce an unfocused sample");

    // Environment detection.
    passed &= Expect(
        IsWinlatorXrEnvironment({L"", true, true, false}) &&
            IsWinlatorXrEnvironment({L"", true, false, true}) &&
            !IsWinlatorXrEnvironment({L"", true, false, false}) &&
            !IsWinlatorXrEnvironment({L"", false, true, true}),
        "detection needs Wine plus a Winlator marker");
    passed &= Expect(
        IsWinlatorXrEnvironment({L"1", false, false, false}) &&
            !IsWinlatorXrEnvironment({L"0", true, true, true}),
        "BFVR_WINLATORXR overrides detection");

    // State packet.
    passed &= Expect(
        FormatStatePacket(0.5F, 2.0F, DisplayMode::HeadTracked, StereoLayout::SideBySide, 0.0F, 0.0F) ==
            "0.5 1 1 1 0 0",
        "the state packet must be clamped and space separated");

    // Haptics.
    winlatorxr::HapticPulseState haptics;
    constexpr std::int64_t ms = 1'000'000;
    auto amplitudes = UpdateHapticPulses(haptics, {1, 0, 0, 0}, 1000 * ms);
    passed &= Expect(
        amplitudes[0] == 0.0F && Near(amplitudes[1], 0.6F),
        "a right shot pulses the right hand");
    amplitudes = UpdateHapticPulses(haptics, {}, 1030 * ms);
    passed &= Expect(Near(amplitudes[1], 0.6F), "the shot pulse lasts beyond 30 ms");
    amplitudes = UpdateHapticPulses(haptics, {}, 1050 * ms);
    passed &= Expect(amplitudes[1] == 0.0F, "the shot pulse ends after 45 ms");
    amplitudes = UpdateHapticPulses(haptics, {0, 0, 1, 0}, 2000 * ms);
    passed &= Expect(
        Near(amplitudes[0], 1.0F) && Near(amplitudes[1], 1.0F),
        "death pulses both hands at full strength");
    amplitudes = UpdateHapticPulses(haptics, {0, 0, 0, 1}, 2100 * ms);
    passed &= Expect(
        Near(amplitudes[1], 1.0F),
        "a weaker hover must not cut a stronger running pulse");

    return passed ? 0 : 1;
}
