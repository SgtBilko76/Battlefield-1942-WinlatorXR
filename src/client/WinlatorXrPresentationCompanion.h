#pragma once

#include "client/WinlatorXrMenuOverlay.h"
#include "openxr/OpenXRControllerShortcutPolicy.h"
#include "presenter/SharedPresentationProtocol.h"
#include "stereo/ComfortVignette.h"
#include "winlatorxr/WinlatorXrClient.h"

#include "bfvr_shared_bridge.hpp"

#include <windows.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace bfvr
{

// In-process replacement for the x64 OpenXR presenter on WinlatorXR
// (standalone Quest/Pico under Wine). It answers the same ControlBlock
// handshake from XrAPI packets, and instead of submitting OpenXR layers it
// composes both eyes side by side into the game's back buffer just before the
// native Present. All calls come from the game's D3D8 device thread.
class WinlatorXrPresentationCompanion
{
public:
    using LogCallback = void (*)(const wchar_t* message);

    // payloadDirectory is the BFVR folder holding assets\ for the menus.
    bool Start(LogCallback log, const std::wstring& payloadDirectory);
    void Stop();

    // Publishes runtime requirements for a side-by-side game window whose
    // logical size is logicalUiWidth x logicalUiHeight.
    void PublishRequirements(
        shared::ControlBlock& block,
        UINT logicalUiWidth,
        UINT logicalUiHeight) const;

    // Publishes render request and controller sample for sequence. Returns
    // false until a head pose has been received.
    bool PublishRenderRequest(shared::ControlBlock& block, LONG sequence);

    // Marks sequence as rendered, consumed and presented.
    void OnFramePublished(
        shared::ControlBlock& block,
        LONG sequence,
        bool headLocked,
        bool eyeFillingScope);

    void SetNextFrameHasWorld(bool hasWorld) noexcept;
    // Alternate-eye presentation renders eye (sequence % 2); -1 = both eyes.
    [[nodiscard]] int ActiveEyeForRequest(LONG sequence) const noexcept;
    void SetTargets(const std::array<void*, shared::kTextureCount>& targets) noexcept;
    // Also releases the menu textures; call before the device is Reset.
    void ClearTargets() noexcept;

    // Draws the latest published frame into the current back buffer.
    void Compose(void* d3d8Device, const shared::ControlBlock& block);

private:
    void Log(const wchar_t* message) const;
    void LogHandSample(const winlatorxr::InputState& input);
    std::array<float, 2> ConsumeHaptics(const shared::ControlBlock& block);
    void PollSettings();
    void UpdateComfortMotion(const shared::ControlBlock& block);
    void UpdateWorldEffects(const shared::ControlBlock& block);
    void PlayKillSounds(const shared::ControlBlock& block);
    // Appends the "Back to game" button when the published frame shows it; the
    // UI covers the normalized columns [regionLeft, regionLeft+regionWidth)
    // scaled by uiScale around their centre.
    void AppendBackToGame(
        void* d3d8Device,
        float regionLeft,
        float regionWidth,
        float uiScale,
        BFVRD3D8To9SideBySideParamsV2& params);
    void UpdateMenus(
        shared::ControlBlock& block,
        const winlatorxr::RenderViews& views,
        const shared::SharedControllerSample& sample,
        std::int64_t predictedTime);

    winlatorxr::Client client_;
    LogCallback log_ = nullptr;
    BFVRD3D8To9ComposeSideBySideFn compose_ = nullptr;
    std::array<void*, shared::kTextureCount> targets_ = {};
    OpenXRControllerShortcutState shortcuts_ = {};
    WinlatorXrMenuOverlay menus_;
    LONG mountedCameraToggleSequence_ = 0;
    LONG hudToggleSequence_ = 0;
    UINT pendingMenuHoverHaptics_ = 0;
    std::array<shared::SharedPresentationView, 2> requestViews_ = {};
    std::int64_t requestPredictedTime_ = 0;

    // Presenter-only effects, applied while composing the world.
    ULONGLONG nextSettingsPollMs_ = 0;
    bool comfortVignetteEnabled_ = true;
    bool deathComfortEnabled_ = true;
    bool killSoundEnabled_ = true;
    float colorProfile_ = 0.0F;
    float colorExposureEv_ = 0.0F;
    float colorContrast_ = 0.0F;
    float colorSaturation_ = 0.0F;
    stereo::ComfortVignetteMotionState comfortMotion_ = {};
    stereo::DeathComfortState deathComfort_ = {};
    float comfortMotionTarget_ = 0.0F;
    ULONGLONG lastComfortSampleMs_ = 0;
    float vignetteStrength_ = 0.0F;
    float vignetteDeathBlend_ = 0.0F;
    std::int64_t lastEffectsTime_ = 0;
    std::vector<BYTE> killSound_;
    bool killSoundLoaded_ = false;
    LONG consumedKillSounds_ = 0;
    bool killSoundCounterInitialized_ = false;
    std::array<shared::SharedPresentationView, 2> publishedViews_ = {};
    LONG recenterSequence_ = 0;
    std::int64_t lastPredictedTime_ = 0;
    LONG requestSequence_ = 0;
    int requestFrameId_ = 0;
    bool framePublished_ = false;
    int publishedFrameId_ = 0;
    bool publishedHeadLocked_ = true;
    bool publishedScope_ = false;
    bool nextFrameHasWorld_ = true;
    bool alternateEye_ = true;
    float fovHorizontalOverride_ = 0.0F;
    float fovVerticalOverride_ = 0.0F;
    int publishedEye_ = -1;
    bool publishedHasWorld_ = true;
    LONG publishedOverlayFlags_ = 0;
    ULONGLONG publishedAtMs_ = 0;
    bool showingVirtualScreen_ = true;
    winlatorxr::HapticPulseState haptics_ = {};
    std::array<LONG, 4> hapticCounters_ = {};
    bool hapticCountersInitialized_ = false;
    bool composeFailureLogged_ = false;
    bool firstComposeLogged_ = false;
    bool effectsLogged_ = false;
    bool effectsFailureLogged_ = false;
    ULONGLONG lastHandLogMs_ = 0;
    int handLogCount_ = 0;
};

} // namespace bfvr
